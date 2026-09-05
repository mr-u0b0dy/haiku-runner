#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "audio_router.h"
#include "input_adapters.h"
#include "source_manager.h"

#if defined(CONFIG_HR_UI_SOURCE_CONTROL)
#include "source_control.h"
#endif

LOG_MODULE_REGISTER(haiku_runner, LOG_LEVEL_INF);

#if defined(CONFIG_HR_DEFAULT_INPUT_AUX)
#define HR_DEFAULT_INPUT AUDIO_INPUT_AUX
#elif defined(CONFIG_HR_DEFAULT_INPUT_USB)
#define HR_DEFAULT_INPUT AUDIO_INPUT_USB
#elif defined(CONFIG_HR_DEFAULT_INPUT_WIFI)
#define HR_DEFAULT_INPUT AUDIO_INPUT_WIFI
#else
#define HR_DEFAULT_INPUT AUDIO_INPUT_BLE
#endif

static struct source_policy g_source_policy = {
  .mode = SOURCE_MODE_HYBRID,
  .manual_selection = HR_DEFAULT_INPUT,
#if defined(CONFIG_HR_SWITCH_ALLOW_AUTO_FALLBACK)
  .allow_auto_fallback = true,
#else
  .allow_auto_fallback = false,
#endif
};

static void register_enabled_inputs(void)
{
#if defined(CONFIG_HR_INPUT_BLE)
  (void)input_adapter_ble_register();
#endif
#if defined(CONFIG_HR_INPUT_AUX)
  (void)input_adapter_aux_register();
#endif
#if defined(CONFIG_HR_INPUT_USB)
  (void)input_adapter_usb_register();
#endif
#if defined(CONFIG_HR_INPUT_WIFI)
  (void)input_adapter_wifi_register();
#endif
}

int main(void)
{
  LOG_INF("haiku-runner boot");

  if (audio_router_init() != 0) {
    LOG_ERR("audio router init failed");
    return -1;
  }

  register_enabled_inputs();

  if (source_manager_init(&g_source_policy) != 0) {
    LOG_ERR("source manager init failed");
    return -1;
  }

  if (audio_router_start() != 0 || source_manager_start() != 0) {
    LOG_ERR("pipeline start failed");
    return -1;
  }

#if defined(CONFIG_HR_UI_SOURCE_CONTROL)
  /* After the adapters are registered, so the button cycles what this build
   * actually contains. */
  if (source_control_init() != 0) {
    LOG_WRN("source control unavailable");
  }
#endif

  while (true) {
    source_manager_tick();

#if defined(CONFIG_HR_UI_SOURCE_CONTROL)
    /* Keeps the LEDs following automatic fallback too, not just presses. */
    source_control_refresh();
#endif

    k_sleep(K_MSEC(200));
  }
}
