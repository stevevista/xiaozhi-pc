#ifndef _OTA_H
#define _OTA_H

#include <functional>
#include <string>
#include <map>

#include <esp_err.h>
#include "board.h"

class Ota {
public:
    Ota();
    ~Ota();

    void SetHeader(const std::string& key, const std::string& value);
    bool CheckVersion();
    esp_err_t Activate();
    bool HasMqttConfig() { return has_mqtt_config_; }

    const std::string& GetCheckVersionUrl() const { return check_version_url_; }

private:
    std::string check_version_url_;
    std::string activation_message_;
    std::string activation_code_;
    bool has_new_version_ = false;
    bool has_mqtt_config_ = false;
    bool has_websocket_config_ = false;
    bool has_server_time_ = false;
    bool has_activation_code_ = false;
    bool has_serial_number_ = false;
    bool has_activation_challenge_ = false;
    std::string current_version_;
    std::string firmware_version_;
    std::string firmware_url_;
    std::string activation_challenge_;
    std::string serial_number_;
    int activation_timeout_ms_ = 30000;
    std::map<std::string, std::string> headers_;

    bool IsNewVersionAvailable(const std::string& currentVersion, const std::string& newVersion);
    Http* SetupHttp();
};

#endif // _OTA_H
