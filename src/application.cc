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


} // namespace

Application::Application() {
  event_group_ = xEventGroupCreate();
  background_task_ = new BackgroundTask(4096 * 8);

  audio_processor_ = std::make_unique<SdlAudioProcessor>();
}

Application::~Application() {
    //if (clock_timer_handle_ != nullptr) {
    //    esp_timer_stop(clock_timer_handle_);
    //    esp_timer_delete(clock_timer_handle_);
    //}
    if (background_task_ != nullptr) {
        delete background_task_;
    }
    vEventGroupDelete(event_group_);
}

void Application::CheckNewVersion() {
  ota_.CheckVersion();
}

bool Application::Init() {

  // codec_ = new SdlAudioCodec(nullptr, SAMPLE_RATE, SAMPLE_RATE);
  codec_ = Board::GetInstance().GetAudioCodec();

  return true;
}

void Application::DeInit() {
  //delete codec_;
  delete Board::GetInstance().GetAudioCodec();
}

void Application::EventLoop() {
  bool running = true;
  while (running) {
    SDL_Event event;
    while(SDL_PollEvent(&event) != 0) {
      if(event.type == SDL_EVENT_QUIT)
        running = false;
       else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
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
  }
}

void Application::AbortSpeaking() {
  Schedule([this]() {
              AbortSpeaking(kAbortReasonNone);
          });
}

void Application::Start() {
  if (!Init()) {
    return;
  }

  auto& board = Board::GetInstance();
  SetDeviceState(kDeviceStateStarting);

  /* Setup the display */
  auto display = board.GetDisplay();

  /* Setup the audio codec */
  auto codec = board.GetAudioCodec();
  opus_decoder_ = std::make_unique<OpusDecoderWrapper>(codec->output_sample_rate(), 1, OPUS_FRAME_DURATION_MS);
  opus_encoder_ = std::make_unique<OpusEncoderWrapper>(16000, 1, OPUS_FRAME_DURATION_MS);

  codec->Start();

  xTaskCreate([](void* arg) {
        Application* app = (Application*)arg;
        app->AudioLoop();
        vTaskDelete(NULL);
    }, "audio_loop", 4096 * 2, this, 8, &audio_loop_task_handle_);

  // Check for new firmware version or get the MQTT broker address
  CheckNewVersion();

  // Initialize the protocol
  display->SetStatus("LOADING_PROTOCOL");

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
  protocol_->OnAudioChannelOpened([this, &board, codec]() {
        board.SetPowerSaveMode(false);
        if (protocol_->server_sample_rate() != codec->output_sample_rate()) {
            ESP_LOGW(TAG, "Server sample rate %d does not match device output sample rate %d, resampling may cause distortion",
                protocol_->server_sample_rate(), codec->output_sample_rate());
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
            auto display = Board::GetInstance().GetDisplay();
            display->SetChatMessage("system", "");
            SetDeviceState(kDeviceStateIdle);
        });
    });
  protocol_->OnIncomingJson([this, display](const cJSON* root) {
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
                    Schedule([this, display, message = std::string(text->valuestring)]() {
                        display->SetChatMessage("assistant", message.c_str());
                    });
                }
            }
        } else if (strcmp(type->valuestring, "stt") == 0) {
            auto text = cJSON_GetObjectItem(root, "text");
            if (text != NULL) {
                ESP_LOGI(TAG, ">> %s", text->valuestring);
                Schedule([this, display, message = std::string(text->valuestring)]() {
                    display->SetChatMessage("user", message.c_str());
                });
            }
        } else if (strcmp(type->valuestring, "llm") == 0) {
            auto emotion = cJSON_GetObjectItem(root, "emotion");
            if (emotion != NULL) {
                Schedule([this, display, emotion_str = std::string(emotion->valuestring)]() {
                    display->SetEmotion(emotion_str.c_str());
                });
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

  audio_processor_->Initialize(codec);
  audio_processor_->OnOutput([this](std::vector<int16_t>&& data) {
    background_task_->Schedule([this, data = std::move(data)]() mutable {
      if (protocol_->IsAudioChannelBusy()) {
        return;
      }
      opus_encoder_->Encode(std::move(data), [this](std::vector<uint8_t>&& opus) {
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
                    ESP_LOGI(TAG, "Send %zu bytes, timestamp %lu, last_ts %lu, qsize %zu",
                         packet.payload.size(), packet.timestamp, last_output_timestamp_value, timestamp_queue_.size());
                });
      });
    });
  });

  audio_processor_->Start();

  SetDeviceState(kDeviceStateIdle);

  if (protocol_started) {
        std::string message = "version xxx";
        display->ShowNotification(message.c_str());
        display->SetChatMessage("system", "");
        // Play the success sound to indicate the device is ready
        ResetDecoder();
        PlaySound("P3_SUCCESS");
    }

  // Enter the main event loop
  MainEventLoop();
}

// Add a async task to MainLoop
void Application::Schedule(std::function<void()> callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        main_tasks_.push_back(std::move(callback));
    }
    xEventGroupSetBits(event_group_, SCHEDULE_EVENT);
}

// The Main Event Loop controls the chat state and websocket connection
// If other tasks need to access the websocket or chat state,
// they should use Schedule to call this function
void Application::MainEventLoop() {
    while (true) {
        auto bits = xEventGroupWaitBits(event_group_, SCHEDULE_EVENT, pdTRUE, pdFALSE, portMAX_DELAY);

        if (bits & SCHEDULE_EVENT) {
            std::unique_lock<std::mutex> lock(mutex_);
            std::list<std::function<void()>> tasks = std::move(main_tasks_);
            lock.unlock();
            for (auto& task : tasks) {
                task();
            }
        }
    }
}

// The Audio Loop is used to input and output audio data
void Application::AudioLoop() {
    auto codec = Board::GetInstance().GetAudioCodec();
    while (true) {
        OnAudioInput();
        if (codec->output_enabled()) {
            OnAudioOutput();
        }
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
        if (!opus_decoder_->Decode(std::move(packet.payload), pcm)) {
            return;
        }
        // Resample if the sample rate is different
        //if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
        //    int target_size = output_resampler_.GetOutputSamples(pcm.size());
        //    std::vector<int16_t> resampled(target_size);
        //    output_resampler_.Process(pcm.data(), pcm.size(), resampled.data());
        //    pcm = std::move(resampled);
        //}
        codec->OutputData(pcm);
        {
            std::lock_guard<std::mutex> lock(timestamp_mutex_);
            timestamp_queue_.push_back(packet.timestamp);
            last_output_timestamp_ = packet.timestamp;
        }
        last_output_time_ = std::chrono::steady_clock::now();
    });
}

void Application::OnAudioInput() {
    if (audio_processor_->IsRunning()) {
        std::vector<int16_t> data;
        int samples = audio_processor_->GetFeedSize();
        if (samples > 0) {
            ReadAudio(data, 16000, samples);
            audio_processor_->Feed(data);
            return;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(30));
}

void Application::ReadAudio(std::vector<int16_t>& data, int sample_rate, int samples) {
    auto codec = Board::GetInstance().GetAudioCodec();
    {
        data.resize(samples);
        if (!codec->InputData(data)) {
            return;
        }
    }
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
    opus_decoder_->ResetState();
    audio_decode_queue_.clear();
    audio_decode_cv_.notify_all();
    last_output_time_ = std::chrono::steady_clock::now();
    auto codec = Board::GetInstance().GetAudioCodec();
    codec->EnableOutput(true);
}

void Application::SetDecodeSampleRate(int sample_rate, int frame_duration) {
    if (opus_decoder_->sample_rate() == sample_rate && opus_decoder_->duration_ms() == frame_duration) {
        return;
    }

    opus_decoder_.reset();
    opus_decoder_ = std::make_unique<OpusDecoderWrapper>(sample_rate, 1, frame_duration);

    auto codec = Board::GetInstance().GetAudioCodec();
    if (opus_decoder_->sample_rate() != codec->output_sample_rate()) {
        ESP_LOGI(TAG, "Resampling audio from %d to %d", opus_decoder_->sample_rate(), codec->output_sample_rate());
        static_cast<SdlAudioCodec*>(codec)->SetOutputFormat(opus_decoder_->sample_rate(), 1);
        //output_resampler_.Configure(opus_decoder_->sample_rate(), codec->output_sample_rate());
    }
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

void Application::PlaySound(const std::string_view& sound) {
}

void Application::UpdateIotStates() {
}
