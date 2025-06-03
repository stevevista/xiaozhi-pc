#pragma once
#include "board.h"

class FakeBoard : public Board {
public:
  FakeBoard();
  ~FakeBoard() override = default;

  std::string GetBoardType() override { return "emulator"; }
  AudioCodec* GetAudioCodec() override;
  Display* GetDisplay() override;
  Http* CreateHttp() override;
  WebSocket* CreateWebSocket() override;
  Mqtt* CreateMqtt() override;
  Udp* CreateUdp() override;
  void StartNetwork() override;
  const char* GetNetworkStateIcon() override;
  void SetPowerSaveMode(bool enabled) override;
  std::string GetBoardJson() override;

private:
  Display* display_ = nullptr;
};
