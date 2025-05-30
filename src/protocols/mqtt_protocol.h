#pragma once
#include "protocol.h"
#include "mqtt.h"
#include "udp.h"
#include <cjson/cJSON.h>
#include <mbedtls/aes.h>
#include <mutex>
#include <condition_variable>

class MqttProtocol : public Protocol {
public:
  MqttProtocol();
  ~MqttProtocol();
  
  bool Start() override;
  bool OpenAudioChannel() override;
  bool SendText(const std::string& text) override;
  void SendAudio(const AudioStreamPacket& packet) override;
  void CloseAudioChannel() override;
    bool IsAudioChannelOpened() const override;

private:
  bool StartMqttClient(bool report_error=false);
  void ParseServerHello(const cJSON* root);
  std::string DecodeHexString(const std::string& hex_string);


  std::string endpoint_;
  std::string client_id_;
  std::string username_;
  std::string password_;
  std::string publish_topic_;
  Mqtt* mqtt_ = nullptr;
  Udp* udp_ = nullptr;

  std::mutex channel_mutex_;
    mbedtls_aes_context aes_ctx_;
    std::string aes_nonce_;
    std::string udp_server_;
    int udp_port_;
    uint32_t local_sequence_;
    uint32_t remote_sequence_;

  std::mutex server_hello_mtx_;
  std::condition_variable cnd_server_hello_;
  bool hello_responsed_;
};
