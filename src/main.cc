
#include "application.h"
#include "impl/http_client.h"

int main(int argc, char* argv[]) {
  Application::GetInstance().Start();

  return 0;
}
