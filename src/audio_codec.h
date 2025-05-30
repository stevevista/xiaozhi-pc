#pragma once
#include <cstdint>
#include <vector>

class AudioCodec {
public:
  AudioCodec() = default;
  virtual ~AudioCodec() = default;

  void Start();
  void OutputData(std::vector<int16_t>& data);
  bool InputData(std::vector<int16_t>& data);

  virtual void SetOutputVolume(int volume);
  virtual void EnableInput(bool enable);
  virtual void EnableOutput(bool enable);

  inline bool duplex() const { return duplex_; }
  inline bool input_reference() const { return input_reference_; }
  inline int input_sample_rate() const { return input_sample_rate_; }
  inline int output_sample_rate() const { return output_sample_rate_; }
  inline int input_channels() const { return input_channels_; }
  inline int output_channels() const { return output_channels_; }
  inline int output_volume() const { return output_volume_; }
  inline bool input_enabled() const { return input_enabled_; }
  inline bool output_enabled() const { return output_enabled_; }
  
protected:
  bool input_enabled_ = false;
  bool output_enabled_ = false;
  int input_sample_rate_ = 0;
  int output_sample_rate_ = 0;
  int input_channels_ = 1;
  int output_channels_ = 1;
  int output_volume_ = 70;
  bool input_reference_ = false;
  bool duplex_ = false;

  virtual int Read(int16_t* dest, int samples) = 0;
  virtual int Write(const int16_t* data, int samples) = 0;
};
