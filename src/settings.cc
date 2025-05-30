#include "settings.h"

#include <esp_log.h>
// #include <nvs_flash.h>

#include <fstream>
#include <iostream>
#include <sstream>

#define TAG "Settings"

namespace simulate_nv {

std::unordered_map<std::string, 
            std::unordered_map<std::string, std::string>> nvs;

std::unordered_map<std::string, std::string>* 
getSection(const std::string& ns) {
  auto it = nvs.find(ns);
  if (it == nvs.end()) {
    auto [it1, ret] = nvs.emplace(ns, std::unordered_map<std::string, std::string>());
    return &it1->second;
  } else {
    return &it->second;
  }
}


bool parseLine(const std::string& line, 
               std::string& x, 
               std::string& y, 
               std::string& z) 
{
  std::istringstream iss(line);
  std::string token;
  std::vector<std::string> parts;

  auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t"));
        s.erase(s.find_last_not_of(" \t") + 1);
  };

  if (std::getline(iss, token, '.')) {
    x = std::move(token);
    trim(x);
  } else {
    return false;
  }

  if (std::getline(iss, token, '=')) {
    y = std::move(token);
    trim(y);
  } else {
    return false;
  }

  std::getline(iss, token, '\n');
  z = std::move(token);
  trim(z);

  return true;
}

void initNvRam() {
  static bool inited = false;
  if (!inited) {
    inited = true;

    std::ifstream file(".xiaozhi_config");
    if (!file.is_open()) {
      std::cerr << "cannnot open .xiaozhi_config" << std::endl;
      return;
    }

    std::string line;
    while (std::getline(file, line)) {
      if (line.empty() || line[0] == '#') continue;
      std::string x, y, z;
      if (parseLine(line, x, y, z)) {
        auto sec = getSection(x);
        (*sec)[y] = z;
      }
    }
  }
}

void commitNvRam() {
  std::ofstream file(".xiaozhi_config");
  if (!file.is_open()) {
    std::cerr << "cannnot open .xiaozhi_config" << std::endl;
    return;
  }

  for (auto &[sec, submap] : nvs) {
    for (auto &[key, value] : submap) {
      file << sec << "." << key << "=" << value << std::endl;
    }
  }
}

} // namespace simulate_nv

Settings::Settings(const std::string& ns, bool read_write) : ns_(ns), read_write_(read_write) {
  simulate_nv::initNvRam();
  nvs_handle_ = simulate_nv::getSection(ns);
}

Settings::~Settings() {
  if (read_write_ && dirty_) {
    simulate_nv::commitNvRam();
  }
}

std::string Settings::GetString(const std::string& key, const std::string& default_value) {
    if (nvs_handle_ == 0) {
        return default_value;
    }

    auto it = nvs_handle_->find(key);
    if (it == nvs_handle_->end()) {
        return default_value;
    }

    return it->second;
}

void Settings::SetString(const std::string& key, const std::string& value) {
    if (nvs_handle_ && read_write_) {
        nvs_handle_->emplace(key, value);
        dirty_ = true;
    } else {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

int32_t Settings::GetInt(const std::string& key, int32_t default_value) {
    if (nvs_handle_ == 0) {
        return default_value;
    }

    auto it = nvs_handle_->find(key);
    if (it == nvs_handle_->end()) {
        return default_value;
    }

    return std::atoi(it->second.c_str());
}

void Settings::SetInt(const std::string& key, int32_t value) {
    if (nvs_handle_ && read_write_) {
        nvs_handle_->emplace(key, std::to_string(value));
        dirty_ = true;
    } else {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

void Settings::EraseKey(const std::string& key) {
    if (nvs_handle_ && read_write_) {
        nvs_handle_->erase(key);
    } else {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}

void Settings::EraseAll() {
    if (nvs_handle_ && read_write_) {
       nvs_handle_->clear();
    } else {
        ESP_LOGW(TAG, "Namespace %s is not open for writing", ns_.c_str());
    }
}
