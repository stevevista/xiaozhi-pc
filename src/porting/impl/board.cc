#include "board.h"
#include "system_info.h"
#include "settings.h"
#include <random>
#include <esp_log.h>

#define TAG "Board"

Board::Board() {
    Settings settings("board", true);
    uuid_ = settings.GetString("uuid");
    if (uuid_.empty()) {
        uuid_ = GenerateUuid();
        settings.SetString("uuid", uuid_);
    }
    ESP_LOGI(TAG, "UUID=%s SKU=%s", uuid_.c_str(), "FAKE_BORAD");
}

std::string Board::GenerateUuid() {
  static std::random_device rd;
  static std::mt19937 engine(rd());

    // UUID v4 需要 16 字节的随机数据
    uint8_t uuid[16];
    
    // 使用 ESP32 的硬件随机数生成器
    for (int i = 0; i < 16; ++i) {
      uuid[i] = static_cast<uint8_t>(engine());
    }
    
    // 设置版本 (版本 4) 和变体位
    uuid[6] = (uuid[6] & 0x0F) | 0x40;    // 版本 4
    uuid[8] = (uuid[8] & 0x3F) | 0x80;    // 变体 1
    
    // 将字节转换为标准的 UUID 字符串格式
    char uuid_str[37];
    snprintf(uuid_str, sizeof(uuid_str),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid[0], uuid[1], uuid[2], uuid[3],
        uuid[4], uuid[5], uuid[6], uuid[7],
        uuid[8], uuid[9], uuid[10], uuid[11],
        uuid[12], uuid[13], uuid[14], uuid[15]);
    
    return std::string(uuid_str);
}

bool Board::GetBatteryLevel(int &level, bool& charging, bool& discharging) {
    return false;
}

Display* Board::GetDisplay() {
    return nullptr;
}

Led* Board::GetLed() {
    static NoLed led;
    return &led;
}

std::string Board::GetJson() {
  /* 
        {
            "version": 2,
            "flash_size": 4194304,
            "psram_size": 0,
            "minimum_free_heap_size": 123456,
            "mac_address": "00:00:00:00:00:00",
            "uuid": "00000000-0000-0000-0000-000000000000",
            "chip_model_name": "esp32s3",
            "chip_info": {
                "model": 1,
                "cores": 2,
                "revision": 0,
                "features": 0
            },
            "application": {
                "name": "my-app",
                "version": "1.0.0",
                "compile_time": "2021-01-01T00:00:00Z"
                "idf_version": "4.2-dev"
                "elf_sha256": ""
            },
            "partition_table": [
                "app": {
                    "label": "app",
                    "type": 1,
                    "subtype": 2,
                    "address": 0x10000,
                    "size": 0x100000
                }
            ],
            "ota": {
                "label": "ota_0"
            },
            "board": {
                ...
            }
        }
    */

  std::string json = R"xxx({"flash_size": )xxx";
  json += std::to_string(SystemInfo::GetFlashSize());
  json += R"xxx(, "minimum_free_heap_size": )xxx";
  json += std::to_string(SystemInfo::GetMinimumFreeHeapSize());
  json += R"xxx(, "mac_address": ")xxx";
  json += SystemInfo::GetMacAddress();
  json += R"xxx(", "chip_model_name": ")xxx";
  json += SystemInfo::GetChipModelName();
  json += R"xxx(", "chip_info": {"model": 9, "cores": 2, "revision": 2, "features": 18}, "application": {"name": "xiaozhi", "version": "0.9.9", "compile_time": "Jan 22 2025T20:40:23Z", "idf_version": "v5.3.2-dirty", "elf_sha256": "22986216df095587c42f8aeb06b239781c68ad8df80321e260556da7fcf5f522"}, "partition_table": [{"label": "nvs", "type": 1, "subtype": 2, "address": 36864, "size": 16384}, {"label": "otadata", "type": 1, "subtype": 0, "address": 53248, "size": 8192}, {"label": "phy_init", "type": 1, "subtype": 1, "address": 61440, "size": 4096}, {"label": "model", "type": 1, "subtype": 130, "address": 65536, "size": 983040}, {"label": "storage", "type": 1, "subtype": 130, "address": 1048576, "size": 1048576}, {"label": "factory", "type": 0, "subtype": 0, "address": 2097152, "size": 4194304}, {"label": "ota_0", "type": 0, "subtype": 16, "address": 6291456, "size": 4194304}, {"label": "ota_1", "type": 0, "subtype": 17, "address": 10485760, "size": 4194304}], "ota": {"label": "factory"}, "board": {"type": "bread-compact-wifi", "ssid": "mzy", "rssi": -58, "channel": 6, "ip": "192.168.124.38", "mac": "cc:ba:97:20:b4:bc"}})xxx";
  return json;
}
