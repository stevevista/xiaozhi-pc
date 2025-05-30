#include "fake_board.h"
#include "sdl_audio_codec.h"
#include "http_client.h"
#include "paho_mqtt.h"
#include "udp_client.h"

DECLARE_BOARD(FakeBoard)

AudioCodec* FakeBoard::GetAudioCodec() {
  static auto codec = new SdlAudioCodec(nullptr, 16000, 16000);
  return codec;
}

Http* FakeBoard::CreateHttp() {
  return new HttpClient();
}

WebSocket* FakeBoard::CreateWebSocket() {
  return nullptr;
}

Mqtt* FakeBoard::CreateMqtt() {
  return new PahoMqtt();
}

Udp* FakeBoard::CreateUdp() {
  return new UdpClient();
}
  
void FakeBoard::StartNetwork() {}
  
const char* FakeBoard::GetNetworkStateIcon() {
  return nullptr;
}
  
void FakeBoard::SetPowerSaveMode(bool enabled) {

}
  
std::string FakeBoard::GetBoardJson() {
  return "";
}
