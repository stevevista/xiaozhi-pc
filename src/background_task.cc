#include "background_task.h"

#include <esp_log.h>
//#include <esp_task_wdt.h>

#define TAG "BackgroundTask"

BackgroundTask::BackgroundTask(uint32_t stack_size) {
    thread_ = std::thread([this] {
        BackgroundTaskLoop();
    });
}

BackgroundTask::~BackgroundTask() {
    if (thread_.joinable()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            quit_ = true;
            condition_variable_.notify_all();
        }
        thread_.join();
    }
}

void BackgroundTask::Schedule(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    active_tasks_++;
    main_tasks_.emplace_back([this, cb = std::move(callback)]() {
        cb();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            active_tasks_--;
            if (main_tasks_.empty() && active_tasks_ == 0) {
                condition_variable_.notify_all();
            }
        }
    });
    condition_variable_.notify_all();
}

void BackgroundTask::WaitForCompletion() {
    std::unique_lock<std::mutex> lock(mutex_);
    condition_variable_.wait(lock, [this]() {
        return main_tasks_.empty() && active_tasks_ == 0;
    });
}

void BackgroundTask::BackgroundTaskLoop() {
    ESP_LOGI(TAG, "background_task started");
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_variable_.wait(lock, [this]() { return !main_tasks_.empty() || quit_; });
        if (quit_) {
            return;
        }
        
        std::list<std::function<void()>> tasks = std::move(main_tasks_);
        lock.unlock();

        for (auto& task : tasks) {
            task();
        }
    }
}
