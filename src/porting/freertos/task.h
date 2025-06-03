#pragma once
#include <freertos/FreeRTOS.h>

typedef void (* TaskFunction_t)( void * arg );
typedef void *TaskHandle_t;

BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                            const char * const pcName,
                            const configSTACK_DEPTH_TYPE uxStackDepth,
                            void * const pvParameters,
                            UBaseType_t uxPriority,
                            TaskHandle_t * const pxCreatedTask );
void vTaskDelete( TaskHandle_t xTaskToDelete );
void vTaskDelay( const TickType_t xTicksToDelay );
