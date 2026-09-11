#ifndef HAIKU_RUNNER_LE_AUDIO_EVENTS_H_
#define HAIKU_RUNNER_LE_AUDIO_EVENTS_H_

/* Called by the BLE Audio (BAP) sink when a stream starts/stops delivering
 * audio (see le_audio_bap_sink.c). Updates the "ble" input's link state and,
 * on loss, nudges source_manager to fall back if hybrid switching allows it.
 */
void le_audio_on_stream_ready(void);
void le_audio_on_stream_lost(void);

#endif
