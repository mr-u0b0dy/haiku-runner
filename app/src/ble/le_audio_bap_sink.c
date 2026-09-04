/* LE Audio (BAP) unicast sink: the Bluetooth stack integration behind the
 * "ble" audio_input adapter (le_audio_sink.c). Modeled on Zephyr's
 * samples/bluetooth/audio/bap_unicast_server, trimmed to a single sink ASE
 * (this device only ever plays audio - it has no microphone/source ASE).
 *
 * Fixed to 48 kHz / 10 ms / mono LC3, matching the mono MAX98357A amp wired
 * to I2S0 (see audio_backend_i2s.c) and the 10 ms I2S block size it uses.
 */
#include <errno.h>

#include <zephyr/bluetooth/audio/audio.h>
#include <zephyr/bluetooth/audio/bap.h>
#include <zephyr/bluetooth/audio/lc3.h>
#include <zephyr/bluetooth/audio/pacs.h>
#include <zephyr/bluetooth/audio/tmap.h>
#include <zephyr/bluetooth/audio/vcp.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/bluetooth/iso.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net_buf.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>

#include "le_audio_bap_sink.h"
#include "le_audio_events.h"
#include "le_audio_sink.h"

LOG_MODULE_REGISTER(le_audio_bap_sink, LOG_LEVEL_INF);

#define HR_BAP_CHANNELS 1U
#define HR_BAP_BITS_PER_SAMPLE 16U

/* Worst case PCM per LC3 frame: 10 ms at 48 kHz. */
#define HR_BAP_MAX_SAMPLES 480U

/* The frequencies LC3 actually supports. Deliberately not
 * BT_AUDIO_CODEC_CAP_FREQ_ANY, which also claims 88.2-384 kHz that liblc3
 * cannot decode. A sink advertising only 48 kHz is too narrow for a central
 * to find a usable configuration (BAP makes 16 kHz mandatory for a Sink), so
 * publish the full LC3 range and adapt the output to whatever is negotiated. */
#define HR_BAP_FREQ_SUPPORTED (BT_AUDIO_CODEC_CAP_FREQ_8KHZ |  \
			       BT_AUDIO_CODEC_CAP_FREQ_16KHZ | \
			       BT_AUDIO_CODEC_CAP_FREQ_24KHZ | \
			       BT_AUDIO_CODEC_CAP_FREQ_32KHZ | \
			       BT_AUDIO_CODEC_CAP_FREQ_44KHZ | \
			       BT_AUDIO_CODEC_CAP_FREQ_48KHZ)

#define AVAILABLE_SINK_CONTEXT (BT_AUDIO_CONTEXT_TYPE_UNSPECIFIED |    \
				 BT_AUDIO_CONTEXT_TYPE_MEDIA |          \
				 BT_AUDIO_CONTEXT_TYPE_CONVERSATIONAL)

static const struct bt_audio_codec_cap lc3_codec_cap = BT_AUDIO_CODEC_CAP_LC3(
	HR_BAP_FREQ_SUPPORTED, BT_AUDIO_CODEC_CAP_DURATION_ANY,
	BT_AUDIO_CODEC_CAP_CHAN_COUNT_SUPPORT(1), 26U, 155U, 1U,
	(BT_AUDIO_CONTEXT_TYPE_CONVERSATIONAL | BT_AUDIO_CONTEXT_TYPE_MEDIA));

/* Format actually negotiated for the active stream, captured at ASE enable.
 * Previously the receive path hardcoded 480 samples at 48 kHz, which fed the
 * output garbage for any other negotiated configuration. */
static uint32_t g_sample_rate_hz;
static uint16_t g_samples_per_frame;

static struct bt_bap_stream sink_stream;
static struct bt_conn *default_conn;

static const struct bt_bap_qos_cfg_pref qos_pref =
	BT_BAP_QOS_CFG_PREF(true, BT_GAP_LE_PHY_2M, 0x02U, 10U, 40000U, 40000U, 40000U, 40000U);

static struct bt_le_ext_adv *adv;

/* Targeted (not general) announcement: tells a scanning phone this device
 * actively wants to be connected to and streamed to right now, which is what
 * gets it picked up as an available audio output rather than just another
 * connectable peripheral. */
static uint8_t unicast_server_addata[] = {
	BT_UUID_16_ENCODE(BT_UUID_ASCS_VAL),
	BT_AUDIO_UNICAST_ANNOUNCEMENT_TARGETED,
	BT_BYTES_LIST_LE16(AVAILABLE_SINK_CONTEXT),
	BT_BYTES_LIST_LE16(0x0000), /* No source context: sink-only device */
	0x00U, /* Metadata length */
};

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID16_SOME,
		      BT_UUID_16_ENCODE(BT_UUID_ASCS_VAL),
		      BT_UUID_16_ENCODE(BT_UUID_CAS_VAL)),
	BT_DATA_BYTES(BT_DATA_SVC_DATA16,
		      BT_UUID_16_ENCODE(BT_UUID_CAS_VAL),
		      BT_AUDIO_UNICAST_ANNOUNCEMENT_TARGETED),
	BT_DATA(BT_DATA_SVC_DATA16, unicast_server_addata, ARRAY_SIZE(unicast_server_addata)),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

#if defined(CONFIG_LIBLC3)

#include "lc3.h"

static lc3_decoder_t lc3_decoder;
static lc3_decoder_mem_48k_t lc3_decoder_mem;
static int frames_per_sdu;

#endif /* CONFIG_LIBLC3 */

static int lc3_config(struct bt_conn *conn, const struct bt_bap_ep *ep, enum bt_audio_dir dir,
		      const struct bt_audio_codec_cfg *codec_cfg, struct bt_bap_stream **stream,
		      struct bt_bap_qos_cfg_pref *const pref, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(ep);

	if (dir != BT_AUDIO_DIR_SINK || sink_stream.conn != NULL) {
		*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_NO_MEM, BT_BAP_ASCS_REASON_NONE);
		return -ENOMEM;
	}

	LOG_INF("ASE Codec Config: stream %p", &sink_stream);

	*stream = &sink_stream;
	*pref = qos_pref;

#if defined(CONFIG_LIBLC3)
	lc3_decoder = NULL;
#endif

	return 0;
}

static int lc3_reconfig(struct bt_bap_stream *stream, enum bt_audio_dir dir,
			const struct bt_audio_codec_cfg *codec_cfg,
			struct bt_bap_qos_cfg_pref *const pref, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(dir);
	ARG_UNUSED(codec_cfg);
	ARG_UNUSED(pref);

	*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_CONF_UNSUPPORTED, BT_BAP_ASCS_REASON_NONE);
	return -ENOEXEC;
}

static int lc3_qos(struct bt_bap_stream *stream, const struct bt_bap_qos_cfg *qos,
		   struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(qos);
	ARG_UNUSED(rsp);

	return 0;
}

static int lc3_enable(struct bt_bap_stream *stream, const uint8_t meta[], size_t meta_len,
		      struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(meta);
	ARG_UNUSED(meta_len);

	LOG_INF("Enable: stream %p", stream);

#if defined(CONFIG_LIBLC3)
	{
		int freq;
		int frame_duration_us;
		int ret;

		ret = bt_audio_codec_cfg_get_freq(stream->codec_cfg);
		if (ret <= 0) {
			LOG_ERR("Codec frequency not set");
			*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_CONF_INVALID,
					       BT_BAP_ASCS_REASON_CODEC_DATA);
			return -EINVAL;
		}
		freq = bt_audio_codec_cfg_freq_to_freq_hz(ret);

		ret = bt_audio_codec_cfg_get_frame_dur(stream->codec_cfg);
		if (ret <= 0) {
			LOG_ERR("Codec frame duration not set");
			*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_CONF_INVALID,
					       BT_BAP_ASCS_REASON_CODEC_DATA);
			return -EINVAL;
		}
		frame_duration_us = bt_audio_codec_cfg_frame_dur_to_frame_dur_us(ret);

		frames_per_sdu = bt_audio_codec_cfg_get_frame_blocks_per_sdu(stream->codec_cfg, true);

		const uint32_t samples_per_frame =
			((uint32_t)freq * (uint32_t)frame_duration_us) / USEC_PER_SEC;

		if (samples_per_frame == 0U || samples_per_frame > HR_BAP_MAX_SAMPLES) {
			LOG_ERR("Unsupported config: %d Hz, %d us -> %u samples/frame", freq,
				frame_duration_us, samples_per_frame);
			*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_CONF_INVALID,
					       BT_BAP_ASCS_REASON_CODEC_DATA);
			return -EINVAL;
		}

		lc3_decoder = lc3_setup_decoder(frame_duration_us, freq, 0, &lc3_decoder_mem);
		if (lc3_decoder == NULL) {
			LOG_ERR("Failed to set up LC3 decoder");
			*rsp = BT_BAP_ASCS_RSP(BT_BAP_ASCS_RSP_CODE_CONF_INVALID,
					       BT_BAP_ASCS_REASON_CODEC_DATA);
			return -EINVAL;
		}

		g_sample_rate_hz = (uint32_t)freq;
		g_samples_per_frame = (uint16_t)samples_per_frame;

		LOG_INF("Stream config: %u Hz, %u us, %u samples/frame, %d frames/SDU",
			g_sample_rate_hz, frame_duration_us, g_samples_per_frame, frames_per_sdu);
	}
#else
	ARG_UNUSED(rsp);
#endif

	return 0;
}

static int lc3_start(struct bt_bap_stream *stream, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(rsp);

	return 0;
}

static int lc3_metadata(struct bt_bap_stream *stream, const uint8_t meta[], size_t meta_len,
			struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(meta);
	ARG_UNUSED(meta_len);
	ARG_UNUSED(rsp);

	return 0;
}

static int lc3_disable(struct bt_bap_stream *stream, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(rsp);

	return 0;
}

static int lc3_stop(struct bt_bap_stream *stream, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(rsp);

	return 0;
}

static int lc3_release(struct bt_bap_stream *stream, struct bt_bap_ascs_rsp *rsp)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(rsp);

	return 0;
}

static struct bt_bap_unicast_server_register_param param = {
	1U, /* snk_cnt */
	0U, /* src_cnt */
};

static const struct bt_bap_unicast_server_cb unicast_server_cb = {
	.config = lc3_config,
	.reconfig = lc3_reconfig,
	.qos = lc3_qos,
	.enable = lc3_enable,
	.start = lc3_start,
	.metadata = lc3_metadata,
	.disable = lc3_disable,
	.stop = lc3_stop,
	.release = lc3_release,
};

#if defined(CONFIG_LIBLC3)

static void stream_recv(struct bt_bap_stream *stream, const struct bt_iso_recv_info *info,
			struct net_buf *buf)
{
	static int16_t pcm_buf[HR_BAP_MAX_SAMPLES];
	const bool valid_data = (info->flags & BT_ISO_FLAGS_VALID) != 0;

	ARG_UNUSED(stream);

	if (lc3_decoder == NULL || frames_per_sdu <= 0) {
		return;
	}

	const int octets_per_frame = valid_data ? (int)(buf->len / frames_per_sdu) : 0;

	/* Deliver each decoded frame as it comes out, rather than decoding the
	 * whole SDU over one buffer and forwarding only the last frame. */
	for (int i = 0; i < frames_per_sdu; i++) {
		/* Passing NULL performs packet loss concealment. */
		const int err = lc3_decode(lc3_decoder,
					   valid_data ? net_buf_pull_mem(buf, octets_per_frame) : NULL,
					   octets_per_frame, LC3_PCM_FORMAT_S16, pcm_buf, 1);

		if (err < 0) {
			LOG_WRN("LC3 decode failed: %d", err);
			return;
		}

		(void)le_audio_sink_receive_frame((const uint8_t *)pcm_buf,
						  (size_t)g_samples_per_frame * sizeof(int16_t),
						  g_sample_rate_hz, HR_BAP_CHANNELS,
						  HR_BAP_BITS_PER_SAMPLE);
	}
}

#else

static void stream_recv(struct bt_bap_stream *stream, const struct bt_iso_recv_info *info,
			struct net_buf *buf)
{
	ARG_UNUSED(stream);
	ARG_UNUSED(info);
	ARG_UNUSED(buf);
}

#endif /* CONFIG_LIBLC3 */

static void stream_started(struct bt_bap_stream *stream)
{
	ARG_UNUSED(stream);

	LOG_INF("Audio stream started");
	le_audio_on_stream_ready();
}

static void stream_stopped(struct bt_bap_stream *stream, uint8_t reason)
{
	ARG_UNUSED(stream);

	LOG_INF("Audio stream stopped (reason 0x%02X)", reason);
	le_audio_on_stream_lost();
}

static struct bt_bap_stream_ops stream_ops = {
	.recv = stream_recv,
	.started = stream_started,
	.stopped = stream_stopped,
};

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err != 0) {
		LOG_WRN("Connection failed (err 0x%02X)", err);
		return;
	}

	LOG_INF("Connected");
	default_conn = bt_conn_ref(conn);
}

/* Set on disconnect, cleared once bt_le_ext_adv_start() actually succeeds.
 * bt_le_ext_adv_start() needs a free slot in the connection pool
 * (CONFIG_BT_MAX_CONN=1), but at the point the disconnected callback runs
 * the just-dropped bt_conn isn't guaranteed to have been returned to that
 * pool yet (the host still holds it for the duration of this callback), so
 * starting advertising here inline can fail with -ENOMEM. Retrying from the
 * adapter's regular poll cycle (le_audio_bap_sink_poll(), a few hundred ms
 * later) sidesteps the race instead of racing it. */
static bool g_need_adv_restart;

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (conn != default_conn) {
		return;
	}

	LOG_INF("Disconnected (reason 0x%02X)", reason);
	bt_conn_unref(default_conn);
	default_conn = NULL;

	g_need_adv_restart = true;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void vcs_state_cb(struct bt_conn *conn, int err, uint8_t volume, uint8_t mute)
{
	ARG_UNUSED(conn);

	if (err != 0) {
		LOG_WRN("VCS state failed: %d", err);
		return;
	}

	/* Reported only. The MAX98357A has a fixed analog gain and the I2S
	 * path does no digital scaling yet, so remote volume changes are
	 * acknowledged but not yet applied to the PCM stream. */
	LOG_INF("VCS volume %u, mute %u", volume, mute);
}

static void vcs_flags_cb(struct bt_conn *conn, int err, uint8_t flags)
{
	ARG_UNUSED(conn);

	if (err != 0) {
		LOG_WRN("VCS flags failed: %d", err);
		return;
	}

	LOG_INF("VCS flags 0x%02X", flags);
}

static struct bt_vcp_vol_rend_cb vcp_cbs = {
	.state = vcs_state_cb,
	.flags = vcs_flags_cb,
};

static int vcp_init(void)
{
	struct bt_vcp_vol_rend_register_param param = {
		.step = 1U,
		.mute = BT_VCP_STATE_UNMUTED,
		.volume = 100U,
		.cb = &vcp_cbs,
	};

	return bt_vcp_vol_rend_register(&param);
}

int le_audio_bap_sink_init(void)
{
	const struct bt_pacs_register_param pacs_param = {
		.snk_pac = true,
		.snk_loc = true,
	};
	static struct bt_pacs_cap cap_sink = {
		.codec_cap = &lc3_codec_cap,
	};
	int err;

	err = bt_enable(NULL);
	if (err != 0) {
		LOG_ERR("bt_enable failed: %d", err);
		return err;
	}

	/* Restores bonds saved in flash. Must run after bt_enable(), otherwise
	 * every reboot looks like a factory reset to an already-paired phone. */
	if (IS_ENABLED(CONFIG_BT_SETTINGS)) {
		err = settings_load();
		if (err != 0) {
			LOG_ERR("settings_load failed: %d", err);
			return err;
		}
	}

	err = bt_pacs_register(&pacs_param);
	if (err != 0) {
		LOG_ERR("bt_pacs_register failed: %d", err);
		return err;
	}

	err = bt_bap_unicast_server_register(&param);
	if (err != 0) {
		LOG_ERR("bt_bap_unicast_server_register failed: %d", err);
		return err;
	}

	err = bt_bap_unicast_server_register_cb(&unicast_server_cb);
	if (err != 0) {
		LOG_ERR("bt_bap_unicast_server_register_cb failed: %d", err);
		return err;
	}

	err = bt_pacs_cap_register(BT_AUDIO_DIR_SINK, &cap_sink);
	if (err != 0) {
		LOG_ERR("bt_pacs_cap_register failed: %d", err);
		return err;
	}

	bt_bap_stream_cb_register(&sink_stream, &stream_ops);

	err = bt_pacs_set_location(BT_AUDIO_DIR_SINK, BT_AUDIO_LOCATION_MONO_AUDIO);
	if (err != 0) {
		LOG_ERR("bt_pacs_set_location failed: %d", err);
		return err;
	}

	err = bt_pacs_set_supported_contexts(BT_AUDIO_DIR_SINK, AVAILABLE_SINK_CONTEXT);
	if (err != 0) {
		LOG_ERR("bt_pacs_set_supported_contexts failed: %d", err);
		return err;
	}

	err = bt_pacs_set_available_contexts(BT_AUDIO_DIR_SINK, AVAILABLE_SINK_CONTEXT);
	if (err != 0) {
		LOG_ERR("bt_pacs_set_available_contexts failed: %d", err);
		return err;
	}

	err = vcp_init();
	if (err != 0) {
		LOG_ERR("bt_vcp_vol_rend_register failed: %d", err);
		return err;
	}

	/* Declares the Unicast Media Receiver role, i.e. "I am a speaker".
	 * Requires CAS + a sink ASE + VCP, all registered above. */
	err = bt_tmap_register(BT_TMAP_ROLE_UMR);
	if (err != 0) {
		LOG_ERR("bt_tmap_register failed: %d", err);
		return err;
	}

	err = bt_le_ext_adv_create(BT_BAP_ADV_PARAM_CONN_QUICK, NULL, &adv);
	if (err != 0) {
		LOG_ERR("bt_le_ext_adv_create failed: %d", err);
		return err;
	}

	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err != 0) {
		LOG_ERR("bt_le_ext_adv_set_data failed: %d", err);
		return err;
	}

	LOG_INF("BLE Audio (BAP) sink initialized");
	return 0;
}

int le_audio_bap_sink_start_adv(void)
{
	int err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);

	if (err != 0 && err != -EALREADY) {
		LOG_ERR("bt_le_ext_adv_start failed: %d", err);
		return err;
	}

	LOG_INF("Advertising started");
	return 0;
}

void le_audio_bap_sink_poll(void)
{
	if (!g_need_adv_restart) {
		return;
	}

	if (le_audio_bap_sink_start_adv() == 0) {
		g_need_adv_restart = false;
	}
}

int le_audio_bap_sink_stop_adv(void)
{
	int err = bt_le_ext_adv_stop(adv);

	if (err != 0) {
		LOG_ERR("bt_le_ext_adv_stop failed: %d", err);
		return err;
	}

	return 0;
}
