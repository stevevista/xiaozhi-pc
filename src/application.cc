#include "application.h"
#include "settings.h"
#include "sdl_audio_codec.h"
#include "sdl_audio_processor.h"
#include <cjson/cJSON.h>

#include "httplib.h"

namespace {

constexpr int WIDTH = 320;
constexpr int HEIGHT = 240;

constexpr int SAMPLE_RATE = 16000;
constexpr int CHANNELS = 1;

constexpr int AUDIO_OUT_BUF_SIZE = 960;

constexpr const char* OTA_HOST = "api.tenclass.net";
constexpr const char* OTA_PATH = "/xiaozhi/ota/";
constexpr const char* MAC_ADDR = "c1:63:f4:3d:b4:ba";

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

  codec_ = new SdlAudioCodec(nullptr, SAMPLE_RATE, SAMPLE_RATE);
  audio_processor_ = new SdlAudioProcessor();

  audio_processor_->OnOutput([this](std::vector<int16_t>&& data) {
    UpdateSampleDisplay(true, &data[0], data.size());
    encoder_->Encode(std::move(data), [this] (std::vector<uint8_t>&& encoded) {
      std::vector<int16_t> pcm;
      if (decoder_->Decode(std::move(encoded), pcm)) {
        UpdateSampleDisplay(false, &pcm[0], pcm.size());
        codec_->OutputData(pcm);
      }
    });
  });

  audio_processor_->Initialize(codec_);
  codec_->EnableOutput(true);

  audio_processor_->Start();

  return true;
}

void Application::DeInit() {
  SDL_Log("Shutting down.");

  delete audio_processor_;
  delete codec_;
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
          Schedule([] {
            printf("TALK....\n");
          });
        }
        if (event.key.key == SDLK_A) {
          Schedule([] {
            printf("STOP....\n");
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

void Application::Start(bool force_update_ota) {
  if (!Init()) {
    return;
  }

  if (!QueryOTAConfig(force_update_ota)) {
    DeInit();
    return;
  }

  //settings.SetString("endpoint", "mqtt.xiaozhi.me");
  //settings.SetString("username", "xiaozhi");
  //settings.SetString("password", "123456");

  EventLoop();
  DeInit();
}

bool Application::QueryOTAConfig(bool force_update_ota) {
  Settings settings("mqtt", true);
  if (!force_update_ota) {
    if (!settings.GetString("endpoint").empty()) {
      SDL_Log("Use local config.");
      return true;
    }
  }

  httplib::SSLClient cli(OTA_HOST);
  cli.enable_server_certificate_verification(false);

  httplib::Headers header = {
    {"Device-Id", MAC_ADDR},
    {"Content-Type", "application/json"},
  };

  std::string body = R"xxx({"flash_size": 16777216, "minimum_free_heap_size": 8318916, "mac_address": "c1:63:f4:3d:b4:ba", "chip_model_name": "esp32s3", "chip_info": {"model": 9, "cores": 2, "revision": 2, "features": 18}, "application": {"name": "xiaozhi", "version": "0.9.9", "compile_time": "Jan 22 2025T20:40:23Z", "idf_version": "v5.3.2-dirty", "elf_sha256": "22986216df095587c42f8aeb06b239781c68ad8df80321e260556da7fcf5f522"}, "partition_table": [{"label": "nvs", "type": 1, "subtype": 2, "address": 36864, "size": 16384}, {"label": "otadata", "type": 1, "subtype": 0, "address": 53248, "size": 8192}, {"label": "phy_init", "type": 1, "subtype": 1, "address": 61440, "size": 4096}, {"label": "model", "type": 1, "subtype": 130, "address": 65536, "size": 983040}, {"label": "storage", "type": 1, "subtype": 130, "address": 1048576, "size": 1048576}, {"label": "factory", "type": 0, "subtype": 0, "address": 2097152, "size": 4194304}, {"label": "ota_0", "type": 0, "subtype": 16, "address": 6291456, "size": 4194304}, {"label": "ota_1", "type": 0, "subtype": 17, "address": 10485760, "size": 4194304}], "ota": {"label": "factory"}, "board": {"type": "bread-compact-wifi", "ssid": "mzy", "rssi": -58, "channel": 6, "ip": "192.168.124.38", "mac": "cc:ba:97:20:b4:bc"}})xxx";

  auto res = cli.Post(OTA_PATH, header, body.c_str(), body.size(), "application/json");
  if (!res) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Query OTA failed");
    return false;
  }

  std::cout << res->status << std::endl;
  std::cout << res->body << std::endl;

  /*
  { 
    "mqtt":{
      "endpoint":"mqtt.xiaozhi.me",
      "client_id":"GID_test@@@c1_63_f4_3d_b4_ba@@@",
      "username":"eyJpcCI6IjU4LjM0LjIxNy4yMjIifQ==",
      "password":"y9xIzfOZk38X/MAGZFNoUiLwjOUd9liWZJNrpnehzEk=",
      "publish_topic":"device-server",
      "subscribe_topic":"null"
    },
    "websocket":{"url":"wss://api.tenclass.net/xiaozhi/v1/","token":"test-token"},"server_time":{"timestamp":1748499162137,"timezone_offset":480},"firmware":{"version":"1.6.2","url":"https://xiaozhi-voice-assistant.oss-cn-shenzhen.aliyuncs.com/firmwares/v1.6.2_bread-compact-wifi/xiaozhi.bin"}}
  */
  cJSON* root = cJSON_Parse(res->body.c_str());
  if (root == nullptr) {
           //  ESP_LOGE(TAG, "Failed to parse json message %s", payload.c_str());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to parse OTA body");
    return false;
  }

  auto mqtt = cJSON_GetObjectItem(root, "mqtt");
  if (mqtt == nullptr) {
           //  ESP_LOGE(TAG, "Failed to parse json message %s", payload.c_str());
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OTA body contains not mqtt config");
    return false;
  }

  auto jendpoint = cJSON_GetObjectItem(mqtt, "endpoint");
  if (jendpoint == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OTA body contains no endpoint");
    return false;
  }
  std::string endpoint = jendpoint->valuestring;

  auto jclient_id = cJSON_GetObjectItem(mqtt, "client_id");
  if (jclient_id == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OTA body contains no client_id");
    return false;
  }
  std::string client_id = jclient_id->valuestring;

  auto jusername = cJSON_GetObjectItem(mqtt, "username");
  if (jusername == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OTA body contains no username");
    return false;
  }
  std::string username = jusername->valuestring;

  auto jpassword = cJSON_GetObjectItem(mqtt, "password");
  if (jpassword == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OTA body contains no password");
    return false;
  }
  std::string password = jpassword->valuestring;

  auto jpublish_topic = cJSON_GetObjectItem(mqtt, "publish_topic");
  if (jpublish_topic == nullptr) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OTA body contains no publish_topic");
    return false;
  }
  std::string publish_topic = jpublish_topic->valuestring;

  std::string subscribe_topic;
  auto jsubscribe_topic = cJSON_GetObjectItem(mqtt, "subscribe_topic");
  if (jsubscribe_topic) {
    subscribe_topic = jsubscribe_topic->valuestring;
  }

  cJSON_Delete(root);

  settings.SetString("endpoint", endpoint);
  settings.SetString("client_id", client_id);
  settings.SetString("username", username);
  settings.SetString("password", password);
  settings.SetString("publish_topic", publish_topic);
  settings.SetString("subscribe_topic", subscribe_topic);

  return true;
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
