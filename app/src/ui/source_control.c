/* Front-panel source control: button cycles the input, LEDs show which one is
 * active, and the speaker announces the change.
 *
 * The press is handled on a dedicated thread rather than in the GPIO callback
 * or the system workqueue, because announcing the new source means playing a
 * few hundred milliseconds of audio and therefore sleeping. Blocking the
 * system workqueue for that long would stall Bluetooth and USB work items.
 */
#include <errno.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "audio_cue.h"
#include "input_registry.h"
#include "source_control.h"
#include "source_manager.h"

LOG_MODULE_REGISTER(source_control, LOG_LEVEL_INF);

#define SW0_NODE DT_ALIAS(sw0)

static const struct gpio_dt_spec g_button = GPIO_DT_SPEC_GET(SW0_NODE, gpios);
static struct gpio_callback g_button_cb;

/* One LED per input, indexed by enum audio_input_id. The DK has four LEDs and
 * there are four ids including UNKNOWN, so the mapping is direct. */
static const struct gpio_dt_spec g_leds[] = {
  GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios), /* AUDIO_INPUT_BLE */
  GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios), /* AUDIO_INPUT_AUX */
  GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios), /* AUDIO_INPUT_USB */
  GPIO_DT_SPEC_GET(DT_ALIAS(led3), gpios), /* AUDIO_INPUT_WIFI */
};

BUILD_ASSERT(ARRAY_SIZE(g_leds) == AUDIO_INPUT_UNKNOWN,
             "Need one LED per audio_input_id");

static K_THREAD_STACK_DEFINE(g_ui_stack, CONFIG_HR_UI_STACK_SIZE);
static struct k_thread g_ui_thread;
static K_SEM_DEFINE(g_press, 0, 1);

static enum audio_input_id g_shown = AUDIO_INPUT_UNKNOWN;

static void source_control_set_leds(enum audio_input_id id)
{
  for (size_t i = 0; i < ARRAY_SIZE(g_leds); ++i) {
    if (!gpio_is_ready_dt(&g_leds[i])) {
      continue;
    }
    (void)gpio_pin_set_dt(&g_leds[i], (enum audio_input_id)i == id ? 1 : 0);
  }
}

void source_control_refresh(void)
{
  enum audio_input_id active = source_manager_active();

  if (active != g_shown) {
    g_shown = active;
    source_control_set_leds(active);
  }
}

/* Returns the next registered input after `from`, wrapping. Only registered
 * adapters are offered, so the button cycles what the build actually has
 * rather than every id in the enum. */
static enum audio_input_id source_control_next(enum audio_input_id from)
{
  const size_t count = input_registry_count();

  if (count == 0U) {
    return AUDIO_INPUT_UNKNOWN;
  }

  size_t start = 0U;

  for (size_t i = 0; i < count; ++i) {
    const struct audio_input_descriptor *d = input_registry_at(i);

    if (d != NULL && d->id == from) {
      start = i;
      break;
    }
  }

  for (size_t step = 1; step <= count; ++step) {
    const struct audio_input_descriptor *d = input_registry_at((start + step) % count);

    if (d != NULL) {
      return d->id;
    }
  }

  return from;
}

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
  ARG_UNUSED(dev);
  ARG_UNUSED(cb);
  ARG_UNUSED(pins);

  k_sem_give(&g_press);
}

static void ui_thread(void *p1, void *p2, void *p3)
{
  ARG_UNUSED(p1);
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  while (true) {
    k_sem_take(&g_press, K_FOREVER);

    /* Settle the contacts, then confirm the button is really down. This also
     * swallows the burst of edges a single press generates. */
    k_sleep(K_MSEC(CONFIG_HR_UI_DEBOUNCE_MS));
    if (gpio_pin_get_dt(&g_button) != 1) {
      k_sem_reset(&g_press);
      continue;
    }

    enum audio_input_id next = source_control_next(source_manager_active());

    if (next == AUDIO_INPUT_UNKNOWN) {
      continue;
    }

    if (source_manager_select(next) == 0) {
      const struct audio_input_descriptor *d = input_registry_get(next);

      LOG_INF("source switched to %s", d != NULL ? d->name : "?");
      g_shown = next;
      source_control_set_leds(next);

      if (IS_ENABLED(CONFIG_HR_UI_AUDIO_CUE)) {
        audio_cue_play_source(next);
      }
    }

    /* Drop edges accumulated while the cue was playing, so one press is one
     * switch however long the announcement takes. */
    k_sem_reset(&g_press);
  }
}

int source_control_init(void)
{
  int ret;

  for (size_t i = 0; i < ARRAY_SIZE(g_leds); ++i) {
    if (!gpio_is_ready_dt(&g_leds[i])) {
      LOG_WRN("LED %zu not ready", i);
      continue;
    }

    ret = gpio_pin_configure_dt(&g_leds[i], GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
      LOG_WRN("LED %zu configure failed: %d", i, ret);
    }
  }

  if (!gpio_is_ready_dt(&g_button)) {
    LOG_ERR("source button not ready");
    return -ENODEV;
  }

  ret = gpio_pin_configure_dt(&g_button, GPIO_INPUT);
  if (ret != 0) {
    LOG_ERR("button configure failed: %d", ret);
    return ret;
  }

  ret = gpio_pin_interrupt_configure_dt(&g_button, GPIO_INT_EDGE_TO_ACTIVE);
  if (ret != 0) {
    LOG_ERR("button interrupt configure failed: %d", ret);
    return ret;
  }

  gpio_init_callback(&g_button_cb, button_pressed, BIT(g_button.pin));
  ret = gpio_add_callback(g_button.port, &g_button_cb);
  if (ret != 0) {
    LOG_ERR("button callback registration failed: %d", ret);
    return ret;
  }

  k_thread_create(&g_ui_thread, g_ui_stack, K_THREAD_STACK_SIZEOF(g_ui_stack),
                  ui_thread, NULL, NULL, NULL, CONFIG_HR_UI_THREAD_PRIORITY, 0, K_NO_WAIT);
  k_thread_name_set(&g_ui_thread, "source_ui");

  source_control_refresh();

  LOG_INF("source control ready (button on %s pin %d)", g_button.port->name, g_button.pin);
  return 0;
}
