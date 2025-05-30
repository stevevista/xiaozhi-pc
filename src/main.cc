
#include "application.h"

int main(int argc, char* argv[]) {
  bool force_update_ota = false;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--update_ota") == 0) {
      force_update_ota = true;
    }
  }

  Application::GetInstance().Start(force_update_ota);

  return 0;
}
