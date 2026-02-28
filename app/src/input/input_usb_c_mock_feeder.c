#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "input_usb_c_uac.h"

LOG_MODULE_REGISTER(input_usb_c_mock, LOG_LEVEL_INF);

K_THREAD_STACK_DEFINE(g_usb_mock_stack, CONFIG_HR_INPUT_USB_C_MOCK_STACK_SIZE);
static struct k_thread g_usb_mock_thread;

static void usb_mock_feeder_thread(void *arg1, void *arg2, void *arg3)
{
  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  uint8_t sample = 0U;
  uint8_t chunk[CONFIG_HR_INPUT_USB_C_MOCK_CHUNK_BYTES];

  (void)input_usb_c_set_connected(true);
  LOG_INF("USB-C mock feeder started");

  while (true) {
    for (size_t index = 0; index < sizeof(chunk); ++index) {
      chunk[index] = sample;
      sample = (uint8_t)(sample + 1U);
    }

    (void)input_usb_c_push_pcm_bytes(chunk, sizeof(chunk));
    k_msleep(CONFIG_HR_INPUT_USB_C_MOCK_INTERVAL_MS);
  }
}

static int input_usb_c_mock_startup(void)
{
  k_thread_create(&g_usb_mock_thread,
                  g_usb_mock_stack,
                  K_THREAD_STACK_SIZEOF(g_usb_mock_stack),
                  usb_mock_feeder_thread,
                  NULL,
                  NULL,
                  NULL,
                  K_LOWEST_APPLICATION_THREAD_PRIO,
                  0,
                  K_NO_WAIT);

  return 0;
}

SYS_INIT(input_usb_c_mock_startup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
