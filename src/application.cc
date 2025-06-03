#include "application.h"
#include "settings.h"
#include "impl/sdl_audio_codec.h"
#include "impl/sdl_audio_processor.h"
#include "protocols/mqtt_protocol.h"
#include <cjson/cJSON.h>
#include "board.h"
#include "display/display.h"
#include "httplib.h"
#include <esp_log.h>

#define TAG "Application"


static const char* const STATE_STRINGS[] = {
    "unknown",
    "starting",
    "configuring",
    "idle",
    "connecting",
    "listening",
    "speaking",
    "upgrading",
    "activating",
    "fatal_error",
    "invalid_state"
};


#undef PlaySound

namespace {

constexpr int WIDTH = 320;
constexpr int HEIGHT = 240;

constexpr int SAMPLE_RATE = 16000;
constexpr int CHANNELS = 1;

constexpr int AUDIO_OUT_BUF_SIZE = 960;

} // namespace

Application::Application() {
  background_task_ = new BackgroundTask(4096 * 8);
}

Application::~Application() {
    if (background_task_ != nullptr) {
        delete background_task_;
    }
}

bool Application::Init() {

  sample_array_in_.resize(SAMPLE_ARRAY_SIZE);
  sample_array_out_.resize(SAMPLE_ARRAY_SIZE);
  wwidth_ = WIDTH;
  wheight_ = HEIGHT;

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
    return false;
  }

  if (!SDL_CreateWindowAndRenderer("xiaozhi", wwidth_, wheight_, 0, &window_, &renderer_)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create SDL window and renderer: %s", SDL_GetError());
    return false;
  }

  SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
  SDL_RenderClear(renderer_);
  SDL_RenderPresent(renderer_);

  decoder_ = new OpusDecoderWrapper(SAMPLE_RATE, CHANNELS);
  encoder_ = new OpusEncoderWrapper(SAMPLE_RATE, CHANNELS);

  // codec_ = new SdlAudioCodec(nullptr, SAMPLE_RATE, SAMPLE_RATE);
  codec_ = Board::GetInstance().GetAudioCodec();
  audio_processor_ = new SdlAudioProcessor();

  audio_processor_->OnOutput([this](std::vector<int16_t>&& data) {
    if (!protocol_ || protocol_->IsAudioChannelBusy()) {
      return;
    }

    UpdateSampleDisplay(true, &data[0], data.size());
    encoder_->Encode(std::move(data), [this] (std::vector<uint8_t>&& opus) {
      AudioStreamPacket packet;
      packet.payload = std::move(opus);
      uint32_t last_output_timestamp_value = last_output_timestamp_.load();
      {
                    std::lock_guard<std::mutex> lock(timestamp_mutex_);
                    if (!timestamp_queue_.empty()) {
                        packet.timestamp = timestamp_queue_.front();
                        timestamp_queue_.pop_front();
                    } else {
                        packet.timestamp = 0;
                    }

                    if (timestamp_queue_.size() > 3) { // 限制队列长度3
                        timestamp_queue_.pop_front(); // 该包发送前先出队保持队列长度
                        return;
                    }
      }

      Schedule([this, last_output_timestamp_value, packet = std::move(packet)]() {
                    protocol_->SendAudio(packet);
                    // ESP_LOGI(TAG, "Send %zu bytes, timestamp %lu, last_ts %lu, qsize %zu",
                    //     packet.payload.size(), packet.timestamp, last_output_timestamp_value, timestamp_queue_.size());
                });

      //std::vector<int16_t> pcm;
      //if (decoder_->Decode(std::move(opus), pcm)) {
      //  UpdateSampleDisplay(false, &pcm[0], pcm.size());
      //  codec_->OutputData(pcm);
      //}
    });
  });

  audio_processor_->Initialize(codec_);

  return true;
}

void Application::DeInit() {
  SDL_Log("Shutting down.");

  delete audio_processor_;
  //delete codec_;
  delete Board::GetInstance().GetAudioCodec();
  delete decoder_;
  delete encoder_;

  SDL_DestroyRenderer(renderer_);
  SDL_DestroyWindow(window_);
  SDL_Quit();
}

void Application::EventLoop() {
  bool running = true;
  while (running) {
    SDL_Event event;
    while(SDL_PollEvent(&event) != 0) {
      if(event.type == SDL_EVENT_QUIT)
        running = false;
      else if(event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_ESCAPE) {
          running = false;
        }
        if (event.key.key == SDLK_T) {
          ToggleChatState();
        }
        if (event.key.key == SDLK_A) {
          Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
          });
        }
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == 1) {
          // codec_->EnableOutput(false);
          codec_->EnableInput(true);
        }
      } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.button.button == 1) {
          codec_->EnableInput(false);
          // codec_->EnableOutput(true);
        }
      }
    }

    AudioLoop();

    if (codec_->input_enabled()) {
      SDL_SetRenderDrawColor(renderer_, 0, 255, 0, 255);
    } else {
      SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255);
    }
    SDL_RenderClear(renderer_);

    AudioDisplay(true);
    AudioDisplay(false);

    SDL_RenderPresent(renderer_);
  }
}

void Application::Schedule(std::function<void()> callback) {
  background_task_->Schedule(callback);
}

void Application::Start() {
  CheckNewVersion();

  if (!Init()) {
    return;
  }

  auto& board = Board::GetInstance();
  SetDeviceState(kDeviceStateStarting);

  if (ota_.HasMqttConfig()) {
    protocol_ = std::make_unique<MqttProtocol>();
  } else {
    ESP_LOGW(TAG, "No protocol specified in the OTA config, using MQTT");
    protocol_ = std::make_unique<MqttProtocol>();
  }

  protocol_->OnNetworkError([this](const std::string& message) {
    SetDeviceState(kDeviceStateIdle);
    Alert("ERROR", message.c_str(), "sad", "P3_EXCLAMATION");
  });
  protocol_->OnIncomingAudio([this](AudioStreamPacket&& packet) {
        const int max_packets_in_queue = 600 / OPUS_FRAME_DURATION_MS;
        std::lock_guard<std::mutex> lock(mutex_);
        if (audio_decode_queue_.size() < max_packets_in_queue) {
            audio_decode_queue_.emplace_back(std::move(packet));
        }
  });
  protocol_->OnAudioChannelOpened([this, &board]() {
        board.SetPowerSaveMode(false);
        if (protocol_->server_sample_rate() != codec_->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec_->output_sample_rate());
        }
        SetDecodeSampleRate(protocol_->server_sample_rate(), protocol_->server_frame_duration());
        //auto& thing_manager = iot::ThingManager::GetInstance();
        //protocol_->SendIotDescriptors(thing_manager.GetDescriptorsJson());
        //std::string states;
        //if (thing_manager.GetStatesJson(states, false)) {
        //    protocol_->SendIotStates(states);
        //}
    });
  protocol_->OnAudioChannelClosed([this, &board]() {
        board.SetPowerSaveMode(true);
        Schedule([this]() {
            //auto display = Board::GetInstance().GetDisplay();
            //display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        });
    });
  protocol_->OnIncomingJson([this](const cJSON* root) {
        // Parse JSON data
        auto type = cJSON_GetObjectItem(root, "type");
        if (strcmp(type->valuestring, "tts") == 0) {
            auto state = cJSON_GetObjectItem(root, "state");
            if (strcmp(state->valuestring, "start") == 0) {
                Schedule([this]() {
                    aborted_ = false;
                    if (device_state_ == kDeviceStateIdle || device_state_ == kDeviceStateListening) {
                        SetDeviceState(kDeviceStateSpeaking);
                    }
                });
            } else if (strcmp(state->valuestring, "stop") == 0) {
                Schedule([this]() {
                    background_task_->WaitForCompletion();
                    if (device_state_ == kDeviceStateSpeaking) {
                        if (listening_mode_ == kListeningModeManualStop) {
                            SetDeviceState(kDeviceStateIdle);
                        } else {
                            SetDeviceState(kDeviceStateListening);
                        }
                    }
                });
            } else if (strcmp(state->valuestring, "sentence_start") == 0) {
                auto text = cJSON_GetObjectItem(root, "text");
                if (text != NULL) {
                    ESP_LOGI(TAG, "<< %s", text->valuestring);
                    //Schedule([this, display, message = std::string(text->valuestring)]() {
                    //    display->SetChatMessage("assistant", message.c_str());
                    //});
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                //Schedule([this, display, message = std::string(text->valuestring)]() {
                //    display->SetChatMessage("user", message.c_str());
                //});
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                //Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                 //   display->SetEmotion(emotion_str.c_str());
                //});
            }
        } else if (strcmp(type->valuestring, "iot") == 0) {
            auto commands = cJSON_GetObjectItem(root, "commands");
            if (commands != NULL) {
               // auto& thing_manager = iot::ThingManager::GetInstance();
               // for (int i = 0; i < cJSON_GetArraySize(commands); ++i) {
               //     auto command = cJSON_GetArrayItem(commands, i);
                //    thing_manager.Invoke(command);
               // }
            }
        } else if (strcmp(type->valuestring, "system") == 0) {
            auto command = cJSON_GetObjectItem(root, "command");
            if (command != NULL) {
                ESP_LOGI(TAG, "System command: %s", command->valuestring);
                if (strcmp(command->valuestring, "reboot") == 0) {
                    // Do a reboot if user requests a OTA update
                    //Schedule([this]() {
                   //     Reboot();
                    //});
                } else {
                    ESP_LOGW(TAG, "Unknown system command: %s", command->valuestring);
                }
            }
        } else if (strcmp(type->valuestring, "alert") == 0) {
            auto status = cJSON_GetObjectItem(root, "status");
            auto message = cJSON_GetObjectItem(root, "message");
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (status != NULL && message != NULL && emotion != NULL) {
              Alert(status->valuestring, message->valuestring, emotion->valuestring, "P3_VIBRATION");
            } else {
                ESP_LOGW(TAG, "Alert command requires status, message and emotion");
            }
        }
    });
    bool protocol_started = protocol_->Start();

  audio_processor_->Start();

  SetDeviceState(kDeviceStateIdle);

  if (protocol_started) {
        //std::string message = std::string(Lang::Strings::VERSION) + ota_.GetCurrentVersion();
        //display->ShowNotification(message.c_str());
        //display->SetChatMessage("system", "");
        // Play the success sound to indicate the device is ready
        ResetDecoder();
        PlaySound("P3_SUCCESS");
    }

  EventLoop();
  DeInit();
}

void Application::ShowActivationCode() {
}

void Application::Alert(const char* status, const char* message, const char* emotion, const std::string_view& sound) {
    ESP_LOGW(TAG, "Alert %s: %s [%s]", status, message, emotion);
    auto display = Board::GetInstance().GetDisplay();
    display->SetStatus(status);
    display->SetEmotion(emotion);
    display->SetChatMessage("system", message);
    if (!sound.empty()) {
        ResetDecoder();
        PlaySound(sound);
    }
}

void Application::DismissAlert() {
    if (device_state_ == kDeviceStateIdle) {
        auto display = Board::GetInstance().GetDisplay();
        display->SetStatus("STANDBY");
        display->SetEmotion("neutral");
        display->SetChatMessage("system", "");
    }
}

void Application::SetDeviceState(DeviceState state) {
    if (device_state_ == state) {
        return;
    }
    
    auto previous_state = device_state_;
    device_state_ = state;
    ESP_LOGI(TAG, "STATE: %s", STATE_STRINGS[device_state_]);
    // The state is changed, wait for all background tasks to finish
    background_task_->WaitForCompletion();

    auto& board = Board::GetInstance();
    auto display = board.GetDisplay();
    auto led = board.GetLed();
    led->OnStateChanged();
    switch (state) {
        case kDeviceStateUnknown:
        case kDeviceStateIdle:
            display->SetStatus("STANDBY");
            display->SetEmotion("neutral");
            audio_processor_->Stop();
            
#if CONFIG_USE_WAKE_WORD_DETECT
            wake_word_detect_.StartDetection();
#endif
            break;
        case kDeviceStateConnecting:
            display->SetStatus("CONNECTING");
            display->SetEmotion("neutral");
            display->SetChatMessage("system", "");
            timestamp_queue_.clear();
            last_output_timestamp_ = 0;
            break;
        case kDeviceStateListening:
            display->SetStatus("LISTENING");
            display->SetEmotion("neutral");
            // Update the IoT states before sending the start listening command
            UpdateIotStates();

            // Make sure the audio processor is running
            if (!audio_processor_->IsRunning()) {
                // Send the start listening command
                protocol_->SendStartListening(listening_mode_);
                if (listening_mode_ == kListeningModeAutoStop && previous_state == kDeviceStateSpeaking) {
                    // FIXME: Wait for the speaker to empty the buffer
                    // vTaskDelay(pdMS_TO_TICKS(120));
                }
                // opus_encoder_->ResetState();
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StopDetection();
#endif
                audio_processor_->Start();
            }
            break;
        case kDeviceStateSpeaking:
            display->SetStatus("SPEAKING");

            if (listening_mode_ != kListeningModeRealtime) {
                audio_processor_->Stop();
#if CONFIG_USE_WAKE_WORD_DETECT
                wake_word_detect_.StartDetection();
#endif
            }
            ResetDecoder();
            break;
        default:
            // Do nothing
            break;
    }
}

void Application::ResetDecoder() {
    std::lock_guard<std::mutex> lock(mutex_);
    // decoder_->ResetState();
    audio_decode_queue_.clear();
    audio_decode_cv_.notify_all();
    last_output_time_ = std::chrono::steady_clock::now();
    codec_->EnableOutput(true);
}

// The Audio Loop is used to input and output audio data
void Application::AudioLoop() {
    OnAudioInput();
    if (codec_->output_enabled()) {
      OnAudioOutput();
    }
}

void Application::OnAudioInput() {
  for (;;) {
        int samples = audio_processor_->GetFeedSize();
        if (samples <= 0) {
            break;
        }

        std::vector<int16_t> data(samples);
        if (!codec_->InputData(data)) {
          break;
        }

        audio_processor_->Feed(data);
    }
}

void Application::OnAudioOutput() {
    if (busy_decoding_audio_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto codec = Board::GetInstance().GetAudioCodec();
    const int max_silence_seconds = 10;

    std::unique_lock<std::mutex> lock(mutex_);
    if (audio_decode_queue_.empty()) {
        // Disable the output if there is no audio data for a long time
        if (device_state_ == kDeviceStateIdle) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_output_time_).count();
            if (duration > max_silence_seconds) {
                codec->EnableOutput(false);
            }
        }
        return;
    }

    if (device_state_ == kDeviceStateListening) {
        audio_decode_queue_.clear();
        audio_decode_cv_.notify_all();
        return;
    }

    auto packet = std::move(audio_decode_queue_.front());
    audio_decode_queue_.pop_front();
    lock.unlock();
    audio_decode_cv_.notify_all();

    busy_decoding_audio_ = true;
    background_task_->Schedule([this, codec, packet = std::move(packet)]() mutable {
        busy_decoding_audio_ = false;
        if (aborted_) {
            return;
        }

        std::vector<int16_t> pcm;
        if (!decoder_->Decode(std::move(packet.payload), pcm)) {
            return;
        }
        UpdateSampleDisplay(false, &pcm[0], pcm.size());
      
        // Resample if the sample rate is different
        //if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
        //    int target_size = output_resampler_.GetOutputSamples(pcm.size());
         //   std::vector<int16_t> resampled(target_size);
         //   output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
         //   pcm = std::move(resampled);
       // }
        codec->OutputData(pcm);
        {
            std::lock_guard<std::mutex> lock(timestamp_mutex_);
            timestamp_queue_.push_back(packet.timestamp);
            last_output_timestamp_ = packet.timestamp;
        }
        last_output_time_ = std::chrono::steady_clock::now();
    });
}

void Application::SetListeningMode(ListeningMode mode) {
    listening_mode_ = mode;
    SetDeviceState(kDeviceStateListening);
}

void Application::ToggleChatState() {
    if (device_state_ == kDeviceStateActivating) {
        SetDeviceState(kDeviceStateIdle);
        return;
    }

    if (!protocol_) {
        ESP_LOGE(TAG, "Protocol not initialized");
        return;
    }

    if (device_state_ == kDeviceStateIdle) {
        Schedule([this]() {
            SetDeviceState(kDeviceStateConnecting);
            if (!protocol_->OpenAudioChannel()) {
                return;
            }

            SetListeningMode(realtime_chat_enabled_ ? kListeningModeRealtime : kListeningModeAutoStop);
        });
    } else if (device_state_ == kDeviceStateSpeaking) {
        Schedule([this]() {
            AbortSpeaking(kAbortReasonNone);
        });
    } else if (device_state_ == kDeviceStateListening) {
        Schedule([this]() {
            protocol_->CloseAudioChannel();
        });
    }
}

void Application::AbortSpeaking(AbortReason reason) {
    ESP_LOGI(TAG, "Abort speaking");
    aborted_ = true;
    protocol_->SendAbortSpeaking(reason);
}

void Application::OnClockTimer() {
}

void Application::SetDecodeSampleRate(int sample_rate, int frame_duration) {
}

void Application::CheckNewVersion() {
  ota_.CheckVersion();
}

void Application::PlaySound(const std::string_view& sound) {
}

void Application::UpdateIotStates() {
}


namespace {

int compute_mod(int a, int b) {
  return a < 0 ? a%b + b : a%b;
}

void fill_rectangle(SDL_Renderer *renderer, int x, int y, int w, int h) {
  SDL_FRect rect;
  rect.x = x;
  rect.y = y;
  rect.w = w;
  rect.h = h;
  if (w && h)
    SDL_RenderFillRect(renderer, &rect);
}

} // namespace

void Application::UpdateSampleDisplay(bool input, int16_t *samples, int size) {
  std::vector<int16_t> *sample_array = input ? &sample_array_in_ : &sample_array_out_;

  while (size > 0) {
    int len = sample_array->size() - sample_array_index_;
    if (len > size)
      len = size;
    memcpy(&((*sample_array)[0]) + sample_array_index_, samples, len * sizeof(int16_t));
    samples += len;
    sample_array_index_ += len;
    if (sample_array_index_ >= sample_array->size())
      sample_array_index_ = 0;
    size -= len;
  }
}

void Application::AudioDisplay(bool input) {
  int i, i_start, x, y, y1, ys, delay, n, nb_display_channels;
  int ch, channels, h, h2;

  const int ytop = input ? (ytop_ + wheight_ / 2) : ytop_;
  const int wheight = wheight_ / 2;
  const std::vector<int16_t> *sample_array = input ? &sample_array_in_ : &sample_array_out_;

  /* compute display index : center on currently output samples */
  channels = CHANNELS;
  nb_display_channels = channels;

  if (codec_->output_enabled()) {
    int data_used = WIDTH;
    n = 2 * channels;
    delay = AUDIO_OUT_BUF_SIZE;
    delay /= n;

    delay += 2 * data_used;
    if (delay < data_used)
      delay = data_used;

    i_start = x = compute_mod(sample_array_index_ - delay * channels, sample_array->size());
    h = INT_MIN;
    for (i = 0; i < 1000; i += channels) {
      int idx = (sample_array->size() + x - i) % sample_array->size();
      int a = (*sample_array)[idx];
      int b = (*sample_array)[(idx + 4 * channels) % sample_array->size()];
      int c = (*sample_array)[(idx + 5 * channels) % sample_array->size()];
      int d = (*sample_array)[(idx + 9 * channels) % sample_array->size()];
      int score = a - d;
      if (h < score && (b ^ c) < 0) {
        h = score;
        i_start = idx;
      }
    }
  }

  if (input)
    SDL_SetRenderDrawColor(renderer_, 0, 0, 255, 255);
  else
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);

  /* total height for one channel */
  h = wheight / nb_display_channels;

  /* graph height / 2 */
  h2 = (h * 9) / 20;
  for (ch = 0; ch < nb_display_channels; ch++) {
    i = i_start + ch;
    y1 = ytop + ch * h + (h / 2); /* position of center line */
    for (x = 0; x < WIDTH; x++) {
      y = ((*sample_array)[i] * h2) >> 15;
      if (y < 0) {
        y = -y;
        ys = y1 - y;
      } else {
        ys = y1;
      }
      fill_rectangle(renderer_, xleft_ + x, ys, 1, y);
      i += channels;
      if (i >= sample_array->size())
        i -= sample_array->size();
    }
  }

  SDL_SetRenderDrawColor(renderer_, 0, 0, 255, 255);

  for (ch = 1; ch < nb_display_channels; ch++) {
    y = ytop + ch * h;
    fill_rectangle(renderer_, xleft_, y, WIDTH, 1);
  }
}
