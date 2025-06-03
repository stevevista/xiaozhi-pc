#include "event_groups.h"
#include <mutex>
#include <condition_variable>

namespace {



} // namespace

struct EventGroupHandle {
  std::mutex mutex;
  std::condition_variable cond;
  EventBits_t bits{0};
};

EventGroupHandle_t xEventGroupCreate( void ) {
  return new EventGroupHandle();
}

void vEventGroupDelete( EventGroupHandle_t xEventGroup ) {
  delete xEventGroup;
}

EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup,
                                 const EventBits_t uxBitsToWaitFor,
                                 const BaseType_t xClearOnExit,
                                 const BaseType_t xWaitForAllBits,
                                 TickType_t xTicksToWait ) {
  std::unique_lock lk(xEventGroup->mutex);
  if (xTicksToWait == portMAX_DELAY) {
    xEventGroup->cond.wait(lk, [=] {
      if (xWaitForAllBits)
        return (xEventGroup->bits & uxBitsToWaitFor) == uxBitsToWaitFor;
      else
        return !!(xEventGroup->bits & uxBitsToWaitFor);
    });
  } else {
    xEventGroup->cond.wait_for(lk, std::chrono::milliseconds(portTICK_PERIOD_MS * xTicksToWait), [=] {
      if (xWaitForAllBits)
        return (xEventGroup->bits & uxBitsToWaitFor) == uxBitsToWaitFor;
      else
        return !!(xEventGroup->bits & uxBitsToWaitFor);
    });
  }

  EventBits_t ret = xEventGroup->bits;

  if (ret & uxBitsToWaitFor) {
    if (xClearOnExit) {
      xEventGroup->bits &= ~uxBitsToWaitFor;
    }
  }

  return ret;
}

EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup,
                                const EventBits_t uxBitsToSet) {
  std::lock_guard lk(xEventGroup->mutex);
  xEventGroup->bits |= uxBitsToSet;
  xEventGroup->cond.notify_all();
  return xEventGroup->bits;
}

EventBits_t xEventGroupGetBits( EventGroupHandle_t xEventGroup ) {
  std::lock_guard lk(xEventGroup->mutex);
  return xEventGroup->bits;
}
