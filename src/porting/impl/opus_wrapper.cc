#include "opus_wrapper.h"
#include <stdio.h>

#define BITRATE 64000
#define MAX_FRAME_SIZE 6*960
#define MAX_PACKET_SIZE (3*1276)

OpusEncoderWrapper::~OpusEncoderWrapper() {
  if (encoder) {
    opus_encoder_destroy(encoder);
  }
}

OpusEncoderWrapper::OpusEncoderWrapper(int sample_rate, int channels, int duration_ms)
: channels_(channels) {
  int err;
  encoder = opus_encoder_create(sample_rate, channels, OPUS_APPLICATION_AUDIO, &err);
  if (err<0) {
    fprintf(stderr, "failed to create an encoder: %s\n", opus_strerror(err));
    return;
  }

  err = opus_encoder_ctl(encoder, OPUS_SET_BITRATE(BITRATE));
  if (err<0) {
    opus_encoder_destroy(encoder);
    encoder = nullptr;
    fprintf(stderr, "failed to set bitrate: %s\n", opus_strerror(err));
    return;
  }
}

void OpusEncoderWrapper::Encode(std::vector<int16_t> &&data, std::function <void(std::vector<uint8_t> &&)> callback) {
  std::vector<uint8_t> data_bytes(MAX_PACKET_SIZE);
  int nbBytes = opus_encode(encoder, reinterpret_cast<const opus_int16*>(&data[0]), data.size() / channels_, reinterpret_cast<unsigned char*>(&data_bytes[0]), MAX_PACKET_SIZE);
  if (nbBytes <= 0) {
    fprintf(stderr, "encode failed: %s\n", opus_strerror(nbBytes));
    return;
  }

  data_bytes.resize(nbBytes);
  callback(std::move(data_bytes));
}

OpusDecoderWrapper::~OpusDecoderWrapper() {
  if (decoder)
    opus_decoder_destroy(decoder);
}

OpusDecoderWrapper::OpusDecoderWrapper(int sample_rate, int channels, int duration_ms)
: sample_rate_(sample_rate)
, channels_(channels)
, duration_ms_(duration_ms) {
  int err;
  /* Create a new decoder state. */
  decoder = opus_decoder_create(sample_rate, channels, &err);
  if (err<0) {
    fprintf(stderr, "failed to create decoder: %s\n", opus_strerror(err));
    return;
  }
}

bool OpusDecoderWrapper::Decode(std::vector<uint8_t> &&data, std::vector<int16_t> &pcm) {
  pcm.resize(MAX_FRAME_SIZE * channels_);

  int frame_size = opus_decode(decoder, reinterpret_cast<const unsigned char *>(&data[0]), data.size(), reinterpret_cast<opus_int16*>(&pcm[0]), MAX_FRAME_SIZE, 0);
  if (frame_size<0) {
    fprintf(stderr, "decoder failed: %s\n", opus_strerror(frame_size));
    return false;
  }
  pcm.resize(frame_size * channels_);
  return true;
}
