#pragma once
#include <opus.h>
#include <cstdint>
#include <vector>
#include <functional>

class OpusEncoderWrapper {
public:
  OpusEncoderWrapper(int sample_rate, int channels, int duration_ms);
  ~OpusEncoderWrapper();

  void Encode(std::vector<int16_t> &&data, std::function <void(std::vector<uint8_t> &&)> callback);

private:
  OpusEncoder *encoder = nullptr;
  const int channels_;
};

class OpusDecoderWrapper {
public:
  OpusDecoderWrapper(int sample_rate, int channels, int duration_ms);
  ~OpusDecoderWrapper();

  bool Decode(std::vector<uint8_t> &&data, std::vector<int16_t> &pcm);

  void ResetState() {}

private:
  OpusDecoder *decoder = nullptr;
  const int channels_;
};
