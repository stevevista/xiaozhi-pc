#include "application.h"
#include "settings.h"
#include "impl/sdl_audio_codec.h"
#include "impl/sdl_audio_processor.h"
#include <cjson/cJSON.h>
#include "board.h"
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

  // codec_ = new SdlAudioCodec(nullptr, SAMPLE_RATE, SAMPLE_RATE);
  codec_ = Board::GetInstance().GetAudioCodec();
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

void Application::Start() {
  CheckNewVersion();

  if (!Init()) {
    return;
  }

  EventLoop();
  DeInit();
}

void Application::CheckNewVersion() {
  ota_.CheckVersion();
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
