#pragma once

#include "http.h"
#include "httplib.h"

class HttpClient: public Http {
public:
  ~HttpClient() override = default;

  void SetHeader(const std::string& key, const std::string& value) override;

  bool Open(const std::string& method, const std::string& url, const std::string& content = "") override;

  void Close() override;

  int GetStatusCode() const override;

  std::string GetResponseHeader(const std::string& key) const override;

  size_t GetBodyLength() const override;

  const std::string& GetBody() override;

  int Read(char* buffer, size_t buffer_size) override;

  void SetTimeout(int timeout_ms) override;

private:
  httplib::Headers headers_;
  httplib::Result result_;
  int timeout_ms_ = 0;
};
