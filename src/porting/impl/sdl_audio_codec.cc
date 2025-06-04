#include "sdl_audio_codec.h"
#include "ui_thread.h"

SdlAudioCodec::~SdlAudioCodec() {
  if (stream_in) {
    const SDL_AudioDeviceID devid_in = SDL_GetAudioStreamDevice(stream_in);
    SDL_CloseAudioDevice(devid_in);  /* !!! FIXME: use SDL_OpenAudioDeviceStream instead so we can dump this. */
    SDL_DestroyAudioStream(stream_in);
  }
  
  if (stream_out) {
    const SDL_AudioDeviceID devid_out = SDL_GetAudioStreamDevice(stream_out);
    SDL_CloseAudioDevice(devid_out);
    SDL_DestroyAudioStream(stream_out);
  }
}

SdlAudioCodec::SdlAudioCodec(const char *devname, int input_sample_rate, int output_sample_rate) {
  SDL_AudioDeviceID want_device = SDL_AUDIO_DEVICE_DEFAULT_RECORDING;

  input_sample_rate_ = input_sample_rate;
  output_sample_rate_ = output_sample_rate;

  SDL_AudioDeviceID *devices = SDL_GetAudioRecordingDevices(NULL);
  for (int i = 0; devices[i] != 0; i++) {
        const char *name = SDL_GetAudioDeviceName(devices[i]);
        SDL_Log(" Recording device #%d: '%s'", i, name);
        if (devname && (SDL_strcmp(devname, name) == 0)) {
            want_device = devices[i];
        }
  }

  if (devname && (want_device == SDL_AUDIO_DEVICE_DEFAULT_RECORDING)) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Didn't see a recording device named '%s', using the system default instead.", devname);
    devname = NULL;
  }

  SDL_AudioSpec outspec;
  SDL_AudioSpec inspec;

  SDL_AudioSpec spec;
  spec.format = SDL_AUDIO_S16;
  spec.freq = input_sample_rate;
  spec.channels = 1;

  SDL_Log("Opening default playback device...");
  SDL_AudioDeviceID device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
  if (!device) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for playback: %s!", SDL_GetError());
    SDL_free(devices);
    return;
  }

  SDL_PauseAudioDevice(device);
  SDL_GetAudioDeviceFormat(device, &outspec, NULL);
  stream_out = SDL_CreateAudioStream(&spec, &outspec);
  if (!stream_out) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create an audio stream for playback: %s!", SDL_GetError());
    SDL_free(devices);
    return;
  } else if (!SDL_BindAudioStream(device, stream_out)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't bind an audio stream for playback: %s!", SDL_GetError());
    SDL_free(devices);
    return;
  }

  SDL_Log("Opening recording device %s%s%s...",
            devname ? "'" : "",
            devname ? devname : "[[default]]",
            devname ? "'" : "");

  device = SDL_OpenAudioDevice(want_device, NULL);
  if (!device) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for recording: %s!", SDL_GetError());
    SDL_free(devices);
    return;
  }
  SDL_free(devices);
  SDL_PauseAudioDevice(device);
  SDL_GetAudioDeviceFormat(device, &inspec, NULL);
  stream_in = SDL_CreateAudioStream(&inspec, &spec);
  if (!stream_in) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create an audio stream for recording: %s!", SDL_GetError());
    return;
  } else if (!SDL_BindAudioStream(device, stream_in)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't bind an audio stream for recording: %s!", SDL_GetError());
    return;
  }

  input_channels_ = spec.channels;
  output_channels_ = spec.channels;
  // SDL_SetAudioStreamFormat(stream_in, NULL, &outspec);  /* make sure we output at the playback format. */
}

void SdlAudioCodec::EnableInput(bool enable) {
  if (enable == input_enabled_) {
    return;
  }

  if (stream_in) {
    if (enable) {
      SDL_ResumeAudioStreamDevice(stream_in);
    } else {
      SDL_PauseAudioStreamDevice(stream_in);
      SDL_FlushAudioStream(stream_in);  /* so no samples are held back for resampling purposes. */
    }
  }

  AudioCodec::EnableInput(enable);
  UIThread::update_input_enable(enable);
}

void SdlAudioCodec::EnableOutput(bool enable) {
  if (enable == output_enabled_) {
    return;
  }

  if (stream_out) {
    if (enable) {
      SDL_ResumeAudioStreamDevice(stream_out);
    } else {
      SDL_PauseAudioStreamDevice(stream_out);
      SDL_FlushAudioStream(stream_out);  /* so no samples are held back for resampling purposes. */
    }
  }

  AudioCodec::EnableOutput(enable);
  UIThread::update_output_enable(enable);
}

int SdlAudioCodec::Read(int16_t* dest, int samples) {
  if (input_enabled_ && stream_in) {
    int br = SDL_GetAudioStreamData(stream_in, dest, samples * 2);
    if (br < 0) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to read from input audio stream: %s", SDL_GetError());
      return 0;
    }

    UIThread::update_sample_display(true, dest, br / 2);
    return br / 2;
  }
  return 0;
}

int SdlAudioCodec::Write(const int16_t* data, int samples) {
  if (output_enabled_ && stream_out) {
    UIThread::update_sample_display(false, data, samples);

    if (!SDL_PutAudioStreamData(stream_out, data, samples * 2)) {
      return 0;
    }
    return samples;
  }
  return 0;
}

void SdlAudioCodec::SetOutputFormat(int sample_rate, int channels) {
  if (output_enabled_) {
    SDL_PauseAudioStreamDevice(stream_out);
    SDL_FlushAudioStream(stream_out);
  }

  SDL_AudioSpec spec;
  spec.format = SDL_AUDIO_S16;
  spec.freq = sample_rate;
  spec.channels = channels;
  SDL_SetAudioStreamFormat(stream_in, &spec, NULL);

  if (output_enabled_) {
    SDL_ResumeAudioStreamDevice(stream_out);
  }
}
