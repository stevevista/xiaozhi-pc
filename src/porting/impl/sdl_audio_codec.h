#pragma once

#include "audio_codec.h"
#include <SDL3/SDL.h>

class SdlAudioCodec : public AudioCodec {
public:
  SdlAudioCodec(const char *devname, int input_sample_rate, int output_sample_rate);
  ~SdlAudioCodec() override;

  void EnableInput(bool enable) override;
  void EnableOutput(bool enable) override;

  int Read(int16_t* dest, int samples) override;
  int Write(const int16_t* data, int samples) override;

private:
  SDL_AudioStream *stream_in = nullptr;
  SDL_AudioStream *stream_out = nullptr;

  friend class SdlAudioProcessor;
};
