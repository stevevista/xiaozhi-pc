#pragma once
#include "board.h"

class FakeBoard : public Board {
public:
  ~FakeBoard() override = default;

  std::string GetBoardType() override { return "emulator"; }
  AudioCodec* GetAudioCodec() override;
  Http* CreateHttp() override;
  WebSocket* CreateWebSocket() override;
  Mqtt* CreateMqtt() override;
  Udp* CreateUdp() override;
  void StartNetwork() override;
  const char* GetNetworkStateIcon() override;
  void SetPowerSaveMode(bool enabled) override;
  std::string GetBoardJson() override;
};
