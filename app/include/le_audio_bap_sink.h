#ifndef HAIKU_RUNNER_LE_AUDIO_BAP_SINK_H_
#define HAIKU_RUNNER_LE_AUDIO_BAP_SINK_H_

/* Brings up the Bluetooth LE Audio (BAP) unicast sink stack: bt_enable(),
 * PACS/ASCS registration, and the connectable advertising set. Frames
 * decoded from an accepted stream are delivered via
 * le_audio_sink_receive_frame(); stream lifecycle is reported via
 * le_audio_on_stream_ready()/le_audio_on_stream_lost() (see
 * le_audio_sink.h / le_audio_events.c). Safe to call once at BLE input
 * adapter init.
 */
int le_audio_bap_sink_init(void);

/* Starts/stops the connectable advertising set. */
int le_audio_bap_sink_start_adv(void);
int le_audio_bap_sink_stop_adv(void);

/* Call periodically (see ble_poll() in le_audio_sink.c). Retries starting
 * advertising after a disconnect if the first attempt raced the connection
 * pool freeing up (see the comment above g_need_adv_restart in
 * le_audio_bap_sink.c). No-op once advertising is running again. */
void le_audio_bap_sink_poll(void);

#endif
