#include "system_info.h"

//#include <freertos/task.h>
#include <esp_log.h>
//#include <esp_flash.h>
//#include <esp_mac.h>
//#include <esp_system.h>
//#include <esp_partition.h>
//#include <esp_app_desc.h>
//#include <esp_ota_ops.h>
#if CONFIG_IDF_TARGET_ESP32P4
#include "esp_wifi_remote.h"
#endif

#define TAG "SystemInfo"

/*
{
    "flash_size": 16777216,
    "minimum_free_heap_size": 8318916,
    "mac_address": "c1:63:f4:3d:b4:ba",
    "chip_model_name": "esp32s3",
    "chip_info": {
        "model": 9,
        "cores": 2,
        "revision": 2,
        "features": 18
    },
    "application": {
        "name": "xiaozhi",
        "version": "0.9.9",
        "compile_time": "Jan 22 2025T20:40:23Z",
        "idf_version": "v5.3.2-dirty", "elf_sha256": "22986216df095587c42f8aeb06b239781c68ad8df80321e260556da7fcf5f522"}, "partition_table": [{"label": "nvs", "type": 1, "subtype": 2, "address": 36864, "size": 16384}, {"label": "otadata", "type": 1, "subtype": 0, "address": 53248, "size": 8192}, {"label": "phy_init", "type": 1, "subtype": 1, "address": 61440, "size": 4096}, {"label": "model", "type": 1, "subtype": 130, "address": 65536, "size": 983040}, {"label": "storage", "type": 1, "subtype": 130, "address": 1048576, "size": 1048576}, {"label": "factory", "type": 0, "subtype": 0, "address": 2097152, "size": 4194304}, {"label": "ota_0", "type": 0, "subtype": 16, "address": 6291456, "size": 4194304}, {"label": "ota_1", "type": 0, "subtype": 17, "address": 10485760, "size": 4194304}], "ota": {"label": "factory"}, "board": {"type": "bread-compact-wifi", "ssid": "mzy", "rssi": -58, "channel": 6, "ip": "192.168.124.38", "mac": "cc:ba:97:20:b4:bc"}})xxx";
*/

size_t SystemInfo::GetFlashSize() {
    return 16777216;
}

size_t SystemInfo::GetMinimumFreeHeapSize() {
    return 8318916;
}

size_t SystemInfo::GetFreeHeapSize() {
    return 8318916;
}

std::string SystemInfo::GetMacAddress() {
    return "c1:63:f4:3d:b4:ba";
}

std::string SystemInfo::GetChipModelName() {
    return "esp32s3";
}

esp_err_t SystemInfo::PrintRealTimeStats(TickType_t xTicksToWait) {
    return 0;
}
