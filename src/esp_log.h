#pragma once

#include <stdio.h>


#define ESP_LOGI(TAG, ...) (printf(__VA_ARGS__))
#define ESP_LOGW(TAG, ...) (printf(__VA_ARGS__))
#define ESP_LOGE(TAG, ...) (printf(__VA_ARGS__))
