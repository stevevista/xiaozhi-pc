#include "sdl_audio_processor.h"

SdlAudioProcessor::~SdlAudioProcessor() {
  if (thread_) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      while (!qeue_.empty()) {
        qeue_.pop();
      }
      qeue_.push({});
      cond_.notify_one();
    }
    SDL_WaitThread(thread_, NULL);
  }
}

void SdlAudioProcessor::Initialize(AudioCodec* codec) {
  codec_ = static_cast<SdlAudioCodec*>(codec);

  thread_ = SDL_CreateThread(SdlAudioProcessor::task, "Audio Task", this);
  if (!thread_) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create thread: %s", SDL_GetError());
  }
}

size_t SdlAudioProcessor::GetFeedSize() {
  if (!codec_) {
    return 0;
  }

  size_t framesize = 60 * codec_->input_sample_rate() / 1000;

  int bytes = SDL_GetAudioStreamAvailable(codec_->stream_in);
  if (bytes <= framesize * sizeof(int16_t)) {
    return 0;
  }

  return framesize;
}

void SdlAudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = callback;
}

int SDLCALL SdlAudioProcessor::task(void *data) {
  auto self = reinterpret_cast<SdlAudioProcessor*>(data);

  for (;;) {
    std::vector<int16_t> data;
    {
      std::unique_lock<std::mutex> lock(self->mutex_);
      self->cond_.wait(lock, [self] { return !self->qeue_.empty(); });
      data = std::move(self->qeue_.front());
      self->qeue_.pop();

      if (!self->is_running_ && data.size()) {
        continue;
      }
    }

    // an empty data to quit
    if (data.empty()) {
      break;
    }

    if (self->output_callback_) {
      self->output_callback_(std::move(data));
    }
  }

  return 0;
}

void SdlAudioProcessor::Feed(const std::vector<int16_t>& data) {
  if (thread_) {
    std::lock_guard<std::mutex> lock(mutex_);
    qeue_.push(data);
    cond_.notify_one();
  }
}

void SdlAudioProcessor::OnVadStateChange(std::function<void(bool speaking)> callback) {
  vad_state_change_callback_ = callback;
}

void SdlAudioProcessor::Start() {
  std::unique_lock<std::mutex> lock(mutex_);
  is_running_ = true;
}

void SdlAudioProcessor::Stop() {
  std::unique_lock<std::mutex> lock(mutex_);
  is_running_ = false;
}

bool SdlAudioProcessor::IsRunning() {
  std::unique_lock<std::mutex> lock(mutex_);
  return is_running_;
}
