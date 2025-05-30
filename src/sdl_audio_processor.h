#pragma once
#include "audio_processor.h"
#include "sdl_audio_codec.h"
#include <queue>
#include <mutex>
#include <condition_variable>

class SdlAudioProcessor : public AudioProcessor {
public:
  ~SdlAudioProcessor() override;

  void Initialize(AudioCodec* codec) override;
  void Feed(const std::vector<int16_t>& data) override;
  void Start() override;
  void Stop() override;
  bool IsRunning() override;
  size_t GetFeedSize() override;
  void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override;
  void OnVadStateChange(std::function<void(bool speaking)> callback) override;

private:
  static int SDLCALL task(void *data);

  SdlAudioCodec* codec_ = nullptr;
  SDL_Thread *thread_ = nullptr;
  std::function<void(std::vector<int16_t>&& data)> output_callback_;
  std::function<void(bool speaking)> vad_state_change_callback_;
  bool is_running_ = false;
  
  std::queue<std::vector<int16_t>> qeue_;
  std::mutex mutex_;
  std::condition_variable cond_;
};
