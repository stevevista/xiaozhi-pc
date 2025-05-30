#include "nvs_flash.h"
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

using KVMap = std::unordered_map<std::string, std::string>;

std::unordered_map<std::string, KVMap> nv_caches;

KVMap* 
getSection(const std::string& ns) noexcept {
  auto it = nv_caches.find(ns);
  if (it == nv_caches.end()) {
    auto [it1, ret] = nv_caches.emplace(ns, std::unordered_map<std::string, std::string>());
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

  for (auto &[sec, submap] : nv_caches) {
    for (auto &[key, value] : submap) {
      file << sec << "." << key << "=" << value << std::endl;
    }
  }
}

} // namespace


extern "C" void nvs_open(const char *ns, int flags, nvs_handle_t *h) {
  initNvRam();
  auto sec = getSection(ns);
  *h = reinterpret_cast<nvs_handle_t>(sec);
}

extern "C" int nvs_commit(nvs_handle_t) {
  commitNvRam();
  return ESP_OK;
}

extern "C" void nvs_close(nvs_handle_t) {

}

extern "C" int nvs_get_str(nvs_handle_t h, const char *key, char *buffer, size_t *length) {
  auto sec = reinterpret_cast<KVMap*>(h);
  if (sec) {
    auto it = sec->find(key);
    if (it != sec->end()) {
      if (buffer) {
        if (*length < it->second.size()) {
          return ESP_ERR_INVALID_ARG;
        }
        *length = it->second.size();
        memcpy(buffer, it->second.c_str(), it->second.size());
      } else {
        *length = it->second.size();
      }
      return ESP_OK;
    }
  }

  return ESP_ERR_NVS_NOT_FOUND;
}

extern "C" int nvs_set_str(nvs_handle_t h, const char *key, const char *value) {
  auto sec = reinterpret_cast<KVMap*>(h);
  if (sec) {
    sec->emplace(key, value);
    return ESP_OK;
  }

  return ESP_ERR_INVALID_ARG;
}

extern "C" int nvs_get_i32(nvs_handle_t h, const char *key, int32_t *value) {
  auto sec = reinterpret_cast<KVMap*>(h);
  if (sec) {
    auto it = sec->find(key);
    if (it != sec->end()) {
      *value = std::atoi(it->second.c_str());
      return ESP_OK;
    }
  }

  return ESP_ERR_NVS_NOT_FOUND;
}

extern "C" int nvs_set_i32(nvs_handle_t h, const char *key, int32_t value) {
  auto sec = reinterpret_cast<KVMap*>(h);
  if (sec) {
    sec->emplace(key, std::to_string(value));
    return ESP_OK;
  }

  return ESP_ERR_INVALID_ARG;
}

extern "C" int nvs_erase_key(nvs_handle_t h, const char *key) {
  auto sec = reinterpret_cast<KVMap*>(h);
  if (sec) {
    sec->erase(key);
    return ESP_OK;
  }

  return ESP_ERR_INVALID_ARG;
}

extern "C" int nvs_erase_all(nvs_handle_t h) {
  auto sec = reinterpret_cast<KVMap*>(h);
  if (sec) {
    sec->clear();
    return ESP_OK;
  }

  return ESP_ERR_INVALID_ARG;
}
