#pragma once
#include <SDL3/SDL.h>
#include "audio_codec.h"
#include "audio_processor.h"
#include "opus_wrapper.h"
#include "background_task.h"
#include <functional>
#include <list>
#include <mutex>

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
  
  void Start(bool force_update_ota);
  void Schedule(std::function<void()> callback);

private: 
  Application();

  bool Init();
  void EventLoop();
  void DeInit();
  bool QueryOTAConfig(bool force_update_ota);

  void AudioDisplay(bool input);
  void UpdateSampleDisplay(bool input, int16_t *samples, int size);

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
