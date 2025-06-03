#pragma once
#include <SDL3/SDL.h>
#include "audio_codec.h"
#include "audio_processor.h"
#include "opus_wrapper.h"
#include "background_task.h"
#include "protocols/protocol.h"
#include "ota.h"
#include <functional>
#include <list>
#include <mutex>

enum DeviceState {
    kDeviceStateUnknown,
    kDeviceStateStarting,
    kDeviceStateWifiConfiguring,
    kDeviceStateIdle,
    kDeviceStateConnecting,
    kDeviceStateListening,
    kDeviceStateSpeaking,
    kDeviceStateUpgrading,
    kDeviceStateActivating,
    kDeviceStateFatalError
};

#define OPUS_FRAME_DURATION_MS 60


class Application {
public:
  static Application& GetInstance() {
    static Application instance;
    return instance;
  }
  // 删除拷贝构造函数和赋值运算符
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;

  ~Application();
  
  void Start();
  void Schedule(std::function<void()> callback);
  void SetDeviceState(DeviceState state);
  void Alert(const char* status, const char* message, const char* emotion = "", const std::string_view& sound = "");
  void DismissAlert();
  void PlaySound(const std::string_view& sound);
  void UpdateIotStates();
  void ToggleChatState();

private: 
  Application();

  bool Init();
  void EventLoop();
  void DeInit();
  void SetDecodeSampleRate(int sample_rate, int frame_duration);
  void CheckNewVersion();
  void ShowActivationCode();
  void ResetDecoder();
  void OnClockTimer();
  void AudioLoop();
  void OnAudioInput();
  void OnAudioOutput();
  void AbortSpeaking(AbortReason reason);
  void SetListeningMode(ListeningMode mode);

  void AudioDisplay(bool input);
  void UpdateSampleDisplay(bool input, int16_t *samples, int size);

  Ota ota_;
  std::mutex mutex_;
  std::atomic<uint32_t> last_output_timestamp_ = 0;
  std::unique_ptr<Protocol> protocol_;
  std::chrono::steady_clock::time_point last_output_time_;
  std::list<AudioStreamPacket> audio_decode_queue_;
  std::condition_variable audio_decode_cv_;
  bool aborted_ = false;
  volatile DeviceState device_state_ = kDeviceStateUnknown;
  ListeningMode listening_mode_ = kListeningModeAutoStop;

  std::list<uint32_t> timestamp_queue_;
    std::mutex timestamp_mutex_;
    bool busy_decoding_audio_ = false;
    bool realtime_chat_enabled_ = false;

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;

  AudioCodec *codec_ = nullptr;
  AudioProcessor *audio_processor_ = nullptr;
  OpusDecoderWrapper *decoder_ = nullptr;
  OpusEncoderWrapper *encoder_ = nullptr;

  BackgroundTask* background_task_ = nullptr;

  /* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
  /* TODO: We assume that a decoded and resampled frame fits into this buffer */
  #define SAMPLE_ARRAY_SIZE (4 * 65536)
  std::vector<int16_t> sample_array_in_;
  std::vector<int16_t> sample_array_out_;

  int sample_array_index_ = 0;
  int xleft_ = 0;
  int ytop_ = 0;
  int wwidth_ = 0;
  int wheight_ = 0;
};
