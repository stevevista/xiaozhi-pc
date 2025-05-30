#include "ota.h"
#include "system_info.h"
#include "settings.h"

#include <cjson/cJSON.h>
#include <esp_log.h>

#include <cstring>
#include <vector>
#include <sstream>
#include <algorithm>

#define TAG "Ota"

namespace {

constexpr const char * CONFIG_OTA_URL = "https://api.tenclass.net/xiaozhi/ota/";

}


Ota::Ota() {
    {
        Settings settings("wifi", false);
        check_version_url_ = settings.GetString("ota_url");
        if (check_version_url_.empty()) {
            check_version_url_ = CONFIG_OTA_URL;
        }
    }

#ifdef ESP_EFUSE_BLOCK_USR_DATA
    // Read Serial Number from efuse user_data
    uint8_t serial_number[33] = {0};
    if (esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA, serial_number, 32 * 8) == ESP_OK) {
        if (serial_number[0] == 0) {
            has_serial_number_ = false;
        } else {
            serial_number_ = std::string(reinterpret_cast<char*>(serial_number), 32);
            has_serial_number_ = true;
        }
    }
#endif
}

Ota::~Ota() {
}

void Ota::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

Http* Ota::SetupHttp() {
    auto& board = Board::GetInstance();
    //auto app_desc = esp_app_get_description();

    auto http = board.CreateHttp();
    for (const auto& header : headers_) {
        http->SetHeader(header.first, header.second);
    }

    // http->SetHeader("Activation-Version", has_serial_number_ ? "2" : "1");
    http->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    http->SetHeader("Client-Id", board.GetUuid());
    // http->SetHeader("User-Agent", std::string(BOARD_NAME "/") + app_desc->version);
    // http->SetHeader("Accept-Language", Lang::CODE);
    http->SetHeader("Content-Type", "application/json");

    return http;
}

bool Ota::CheckVersion() {
    auto& board = Board::GetInstance();
    // auto app_desc = esp_app_get_description();

    // Check if there is a new firmware version available
    // current_version_ = app_desc->version;
    // ESP_LOGI(TAG, "Current version: %s", current_version_.c_str());

    if (check_version_url_.length() < 10) {
        ESP_LOGE(TAG, "Check version URL is not properly set");
        return false;
    }

    auto http = SetupHttp();

    std::string data = board.GetJson();
    std::string method = data.length() > 0 ? "POST" : "GET";
    if (!http->Open(method, check_version_url_, data)) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        delete http;
        return false;
    }

    data = http->GetBody();
    delete http;

    // Response: { "firmware": { "version": "1.0.0", "url": "http://" } }
    // Parse the JSON response and check if the version is newer
    // If it is, set has_new_version_ to true and store the new version and URL
    
    cJSON *root = cJSON_Parse(data.c_str());
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }

    has_activation_code_ = false;
    has_activation_challenge_ = false;
    cJSON *activation = cJSON_GetObjectItem(root, "activation");
    if (activation != NULL) {
        cJSON* message = cJSON_GetObjectItem(activation, "message");
        if (message != NULL) {
            activation_message_ = message->valuestring;
        }
        cJSON* code = cJSON_GetObjectItem(activation, "code");
        if (code != NULL) {
            activation_code_ = code->valuestring;
            has_activation_code_ = true;
        }
        cJSON* challenge = cJSON_GetObjectItem(activation, "challenge");
        if (challenge != NULL) {
            activation_challenge_ = challenge->valuestring;
            has_activation_challenge_ = true;
        }
        cJSON* timeout_ms = cJSON_GetObjectItem(activation, "timeout_ms");
        if (timeout_ms != NULL) {
            activation_timeout_ms_ = timeout_ms->valueint;
        }
    }

    has_mqtt_config_ = false;
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (mqtt != NULL) {
        Settings settings("mqtt", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, mqtt) {
            if (item->type == cJSON_String) {
                if (settings.GetString(item->string) != item->valuestring) {
                    settings.SetString(item->string, item->valuestring);
                }
            }
        }
        has_mqtt_config_ = true;
    } else {
        ESP_LOGI(TAG, "No mqtt section found !");
    }

    has_websocket_config_ = false;
    cJSON *websocket = cJSON_GetObjectItem(root, "websocket");
    if (websocket != NULL) {
        Settings settings("websocket", true);
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, websocket) {
            if (item->type == cJSON_String) {
                settings.SetString(item->string, item->valuestring);
            } else if (item->type == cJSON_Number) {
                settings.SetInt(item->string, item->valueint);
            }
        }
        has_websocket_config_ = true;
    } else {
        ESP_LOGI(TAG, "No websocket section found!");
    }

    has_server_time_ = false;
    cJSON *server_time = cJSON_GetObjectItem(root, "server_time");
    if (server_time != NULL) {
        cJSON *timestamp = cJSON_GetObjectItem(server_time, "timestamp");
        cJSON *timezone_offset = cJSON_GetObjectItem(server_time, "timezone_offset");
        
        if (timestamp != NULL) {
            // 设置系统时间
            //struct timeval tv;
            //double ts = timestamp->valuedouble;
            
            // 如果有时区偏移，计算本地时间
            //if (timezone_offset != NULL) {
            //    ts += (timezone_offset->valueint * 60 * 1000); // 转换分钟为毫秒
            //}
            
            //tv.tv_sec = (time_t)(ts / 1000);  // 转换毫秒为秒
            //tv.tv_usec = (suseconds_t)((long long)ts % 1000) * 1000;  // 剩余的毫秒转换为微秒
            //settimeofday(&tv, NULL);
            has_server_time_ = true;
        }
    } else {
        ESP_LOGW(TAG, "No server_time section found!");
    }

    has_new_version_ = false;
    cJSON *firmware = cJSON_GetObjectItem(root, "firmware");
    if (firmware != NULL) {
        cJSON *version = cJSON_GetObjectItem(firmware, "version");
        if (version != NULL) {
            firmware_version_ = version->valuestring;
        }
        cJSON *url = cJSON_GetObjectItem(firmware, "url");
        if (url != NULL) {
            firmware_url_ = url->valuestring;
        }

        if (version != NULL && url != NULL) {
            // Check if the version is newer, for example, 0.1.0 is newer than 0.0.1
            has_new_version_ = IsNewVersionAvailable(current_version_, firmware_version_);
            if (has_new_version_) {
                ESP_LOGI(TAG, "New version available: %s", firmware_version_.c_str());
            } else {
                ESP_LOGI(TAG, "Current is the latest version");
            }
            // If the force flag is set to 1, the given version is forced to be installed
            cJSON *force = cJSON_GetObjectItem(firmware, "force");
            if (force != NULL && force->valueint == 1) {
                has_new_version_ = true;
            }
        }
    } else {
        ESP_LOGW(TAG, "No firmware section found!");
    }

    cJSON_Delete(root);
    return true;
}

bool Ota::IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion) {
    return false;
}

esp_err_t Ota::Activate() {
  return 0;
}
