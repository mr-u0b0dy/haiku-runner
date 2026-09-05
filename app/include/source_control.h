#ifndef HAIKU_RUNNER_SOURCE_CONTROL_H_
#define HAIKU_RUNNER_SOURCE_CONTROL_H_

/* Front-panel control: a button cycles the active input, an LED shows which
 * one it is, and the speaker announces the change (see audio_cue.h).
 *
 * Call once at boot, after the input adapters are registered and
 * source_manager is initialised - it reads the registry to decide what it can
 * cycle through.
 */
int source_control_init(void);

/* Refreshes the LEDs from the currently active source. Called from the main
 * loop so the indication also follows automatic fallback, not just presses. */
void source_control_refresh(void);

#endif
