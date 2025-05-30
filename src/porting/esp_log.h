#pragma once

#include <stdio.h>


#define ESP_LOGI(TAG, fmt, ...) (printf("I[" TAG "] " fmt "\n", ##__VA_ARGS__))
#define ESP_LOGW(TAG, fmt, ...) (printf("W[" TAG "] " fmt "\n", ##__VA_ARGS__))
#define ESP_LOGE(TAG, fmt, ...) (printf("E[" TAG "] " fmt "\n", ##__VA_ARGS__))
