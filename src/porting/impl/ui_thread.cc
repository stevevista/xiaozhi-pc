#include "ui_thread.h"
#include <SDL3/SDL.h>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <atomic>
#include "application.h"

namespace {

constexpr int WIDTH = 320;
constexpr int HEIGHT = 240;
constexpr int CHANNELS = 1;
constexpr int AUDIO_OUT_BUF_SIZE = 960;


std::thread thread;
std::mutex mtx_init;
bool inited = false;
bool init_success = false;
std::condition_variable cond_init_done;

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;

constexpr int xleft_ = 0;
constexpr int ytop_ = 0;
constexpr int wheight_ = HEIGHT;

std::atomic<bool> output_enabled = false;
std::atomic<bool> input_enabled = false;

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
  /* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (4 * 65536)
std::vector<int16_t> sample_array_in_(SAMPLE_ARRAY_SIZE);
std::vector<int16_t> sample_array_out_(SAMPLE_ARRAY_SIZE);

int sample_array_index_ = 0;

std::mutex sample_mtx;

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

void AudioDisplay(bool input) {
  std::lock_guard lk(sample_mtx);

  int i, i_start =0, x, y, y1, ys, delay, n, nb_display_channels;
  int ch, channels, h, h2;

  const int ytop = input ? (ytop_ + wheight_ / 2) : ytop_;
  const int wheight = wheight_ / 2;
  const std::vector<int16_t> *sample_array = input ? &sample_array_in_ : &sample_array_out_;

  /* compute display index : center on currently output samples */
  channels = CHANNELS;
  nb_display_channels = channels;

  if (output_enabled) {
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
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
  else
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

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
      fill_rectangle(renderer, xleft_ + x, ys, 1, y);
      i += channels;
      if (i >= sample_array->size())
        i -= sample_array->size();
    }
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

  for (ch = 1; ch < nb_display_channels; ch++) {
    y = ytop + ch * h;
    fill_rectangle(renderer, xleft_, y, WIDTH, 1);
  }
}

void ui_routine() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
    
    std::lock_guard lk(mtx_init);
    inited = true;
    init_success = false;
    cond_init_done.notify_one();
    return;
  }

  if (!SDL_CreateWindowAndRenderer("xiaozhi", WIDTH, HEIGHT, 0, &window, &renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create SDL window and renderer: %s", SDL_GetError());
    SDL_Quit();

    std::lock_guard lk(mtx_init);
    inited = true;
    init_success = false;
    cond_init_done.notify_one();
    return;
  }

  {
    std::lock_guard lk(mtx_init);
    inited = true;
    init_success = true;
    cond_init_done.notify_one();
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);

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
          Application::GetInstance().ToggleChatState();
        }
        if (event.key.key == SDLK_A) {
          Application::GetInstance().AbortSpeaking();
        }
      }
    }

    if (input_enabled) {
      SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    } else {
      SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    }

    SDL_RenderClear(renderer);

    AudioDisplay(true);
    AudioDisplay(false);

    SDL_RenderPresent(renderer);
  }

  SDL_Log("Shutting down ui.");

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  //SDL_Quit();
  exit(0);
}

} // namespace

void UIThread::update_input_enable(bool b) {
  input_enabled = b;
}

void UIThread::update_output_enable(bool b) {
  output_enabled = b;
}

void UIThread::update_sample_display(bool input, const int16_t *samples, int size) {
  std::lock_guard lk(sample_mtx);

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

bool UIThread::start() {
  thread = std::thread(ui_routine);

  std::unique_lock lk(mtx_init);
  cond_init_done.wait(lk, [] { return inited; });

  if (!init_success) {
    thread.join();
  }

  return init_success;
}
