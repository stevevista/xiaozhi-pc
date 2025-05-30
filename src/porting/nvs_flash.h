#pragma once

#include "esp_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void * nvs_handle_t;

#define NVS_READWRITE 1
#define NVS_READONLY 0

void nvs_open(const char *ns, int flags, nvs_handle_t *);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
int nvs_get_str(nvs_handle_t h, const char *key, char *buffer, size_t *length);
int nvs_set_str(nvs_handle_t h, const char *key, const char *value);
int nvs_get_i32(nvs_handle_t h, const char *key, int32_t *value);
int nvs_set_i32(nvs_handle_t h, const char *key, int32_t value);
int nvs_erase_key(nvs_handle_t h, const char *key);
int nvs_erase_all(nvs_handle_t h);

#ifdef __cplusplus
}
#endif
