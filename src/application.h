#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <SDL3/SDL.h>
#include "audio_codec.h"
#include "audio_processor.h"
#include "impl/opus_wrapper.h"
#include "background_task.h"
#include "protocols/protocol.h"
#include "ota.h"
#include <functional>
#include <list>
#include <mutex>

#if CONFIG_USE_WAKE_WORD_DETECT
#include "wake_word_detect.h"
#endif

#define SCHEDULE_EVENT (1 << 0)
#define AUDIO_INPUT_READY_EVENT (1 << 1)
#define AUDIO_OUTPUT_READY_EVENT (1 << 2)
#define CHECK_NEW_VERSION_DONE_EVENT (1 << 3)


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
  void AbortSpeaking();

private: 
  Application();

  bool Init();
  void EventLoop();
  void DeInit();
  void ShowActivationCode();
  void ResetDecoder();
  void SetDecodeSampleRate(int sample_rate, int frame_duration);
  void CheckNewVersion();
  void OnClockTimer();
  void AudioLoop();
  void OnAudioInput();
  void OnAudioOutput();
  void AbortSpeaking(AbortReason reason);
  void SetListeningMode(ListeningMode mode);

  void AudioDisplay(bool input);
  void UpdateSampleDisplay(bool input, int16_t *samples, int size);

  std::unique_ptr<AudioProcessor> audio_processor_;
  Ota ota_;
  std::mutex mutex_;
  std::list<std::function<void()>> main_tasks_;
  std::atomic<uint32_t> last_output_timestamp_ = 0;
  std::unique_ptr<Protocol> protocol_;
  EventGroupHandle_t event_group_ = nullptr;
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

  AudioCodec *codec_ = nullptr;

  // Audio encode / decode
  TaskHandle_t audio_loop_task_handle_ = nullptr;
  BackgroundTask* background_task_ = nullptr;


  std::unique_ptr<OpusEncoderWrapper> opus_encoder_;
  std::unique_ptr<OpusDecoderWrapper> opus_decoder_;

  void MainEventLoop();
  void ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples);
};
