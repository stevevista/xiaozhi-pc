#include "impl/ui_thread.h"
#include <cstdio>

extern "C" void app_main(void);

int main() {
  if (!UIThread::start()) {
    printf("Failed to start ui thread\n");
    return 1;
  }

  app_main();
  return 0;
}
