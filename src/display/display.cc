#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>
#include <cstring>

#include "display.h"

#define TAG "Display"

Display::Display() {
    // Load theme from settings
}

Display::~Display() {
}

void Display::SetStatus(const char* status) {
    printf("Display: status %s\n", status);
}

void Display::ShowNotification(const std::string &notification, int duration_ms) {
    ShowNotification(notification.c_str(), duration_ms);
}

void Display::ShowNotification(const char* notification, int duration_ms) {
    printf("Display: notification %s\n", notification);
}

void Display::Update() {
}


void Display::SetEmotion(const char* emotion) {
    printf("Display: emotion %s\n", emotion);
}

void Display::SetIcon(const char* icon) {
    printf("Display: icon %s\n", icon);
}

void Display::SetChatMessage(const char* role, const char* content) {
    printf("Display: ChatMessage %s\n", content);
}

void Display::SetTheme(const std::string& theme_name) {
}
