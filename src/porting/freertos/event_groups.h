#pragma once
#include <freertos/FreeRTOS.h>

typedef TickType_t           EventBits_t;


typedef struct EventGroupHandle *  EventGroupHandle_t;

EventGroupHandle_t xEventGroupCreate( void );
void vEventGroupDelete( EventGroupHandle_t xEventGroup );
EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup,
                                 const EventBits_t uxBitsToWaitFor,
                                 const BaseType_t xClearOnExit,
                                 const BaseType_t xWaitForAllBits,
                                 TickType_t xTicksToWait );
EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup,
                                const EventBits_t uxBitsToSet);
EventBits_t xEventGroupGetBits( EventGroupHandle_t xEventGroup );
