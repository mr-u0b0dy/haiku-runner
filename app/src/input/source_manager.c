#include <errno.h>
#include <zephyr/logging/log.h>

#include "audio_router.h"
#include "input_registry.h"
#include "source_manager.h"

LOG_MODULE_REGISTER(source_manager, LOG_LEVEL_INF);

static struct source_policy g_policy;
static enum audio_input_id g_active = AUDIO_INPUT_UNKNOWN;

static bool input_is_healthy(const struct audio_input_descriptor *descriptor)
{
  if (descriptor == NULL || descriptor->ops == NULL || descriptor->ops->healthy == NULL) {
    return false;
  }

  return descriptor->ops->healthy();
}

int source_manager_init(const struct source_policy *policy)
{
  if (policy == NULL) {
    return -EINVAL;
  }

  g_policy = *policy;
  g_active = policy->manual_selection;

  for (size_t index = 0; index < input_registry_count(); ++index) {
    const struct audio_input_descriptor *descriptor = input_registry_at(index);
    if (descriptor != NULL && descriptor->ops != NULL) {
      if (descriptor->ops->set_frame_callback != NULL) {
        (void)descriptor->ops->set_frame_callback(source_manager_on_frame);
      }
      if (descriptor->ops->init != NULL) {
        (void)descriptor->ops->init();
      }
    }
  }

  return 0;
}

int source_manager_start(void)
{
  for (size_t index = 0; index < input_registry_count(); ++index) {
    const struct audio_input_descriptor *descriptor = input_registry_at(index);
    if (descriptor != NULL && descriptor->ops != NULL && descriptor->ops->start != NULL) {
      (void)descriptor->ops->start();
    }
  }

  return 0;
}

int source_manager_stop(void)
{
  for (size_t index = 0; index < input_registry_count(); ++index) {
    const struct audio_input_descriptor *descriptor = input_registry_at(index);
    if (descriptor != NULL && descriptor->ops != NULL && descriptor->ops->stop != NULL) {
      (void)descriptor->ops->stop();
    }
  }

  return 0;
}

int source_manager_select(enum audio_input_id id)
{
  if (input_registry_get(id) == NULL) {
    return -ENOENT;
  }

  g_policy.manual_selection = id;
  g_active = id;
  LOG_INF("source selected: %d", id);
  return 0;
}

enum audio_input_id source_manager_active(void)
{
  return g_active;
}

void source_manager_on_frame(enum audio_input_id id, const struct audio_frame *frame)
{
  if (id != g_active || frame == NULL) {
    return;
  }

  (void)audio_router_submit(frame);
}

void source_manager_tick(void)
{
  const struct audio_input_descriptor *active_descriptor = input_registry_get(g_active);

  if (!g_policy.allow_auto_fallback || active_descriptor == NULL || input_is_healthy(active_descriptor)) {
    return;
  }

  for (size_t index = 0; index < input_registry_count(); ++index) {
    const struct audio_input_descriptor *candidate = input_registry_at(index);
    if (candidate != NULL && candidate->id != g_active && input_is_healthy(candidate)) {
      g_active = candidate->id;
      LOG_WRN("fallback source: %d", g_active);
      return;
    }
  }
}
