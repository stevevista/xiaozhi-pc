#pragma once
#include <cstdint>

class UIThread {
public:
  static bool start();
  static void update_input_enable(bool);
  static void update_output_enable(bool);
  static void update_sample_display(bool input, const int16_t *samples, int size);
};
