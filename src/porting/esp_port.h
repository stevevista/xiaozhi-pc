#pragma once

#include "stdint.h"
#include "assert.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_OK 0
#define ESP_ERR -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_NVS_NOT_FOUND -3
#define ESP_ERR_NVS_NO_FREE_PAGES -4
#define ESP_ERR_NVS_NEW_VERSION_FOUND -5

#define ESP_ERROR_CHECK(expr) expr

#ifdef __cplusplus
}
#endif
