#include "task.h"
#include <thread>

BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                            const char * const pcName,
                            const configSTACK_DEPTH_TYPE uxStackDepth,
                            void * const pvParameters,
                            UBaseType_t uxPriority,
                            TaskHandle_t * const pxCreatedTask ) {
  std::thread t(pxTaskCode, pvParameters);
  t.detach();
  if (pxCreatedTask)
    *pxCreatedTask = reinterpret_cast<TaskHandle_t>(t.native_handle());
  return pdPASS;
}

void vTaskDelete( TaskHandle_t xTaskToDelete ) {}

void vTaskDelay( const TickType_t xTicksToDelay ) {
  std::this_thread::sleep_for(std::chrono::milliseconds(portTICK_PERIOD_MS * xTicksToDelay));
}
