#include "http_client.h"

namespace {

std::string empty{};

} // namespace

bool HttpClient::Open(const std::string& method, const std::string& url, const std::string& content) {
  std::string protocol;
  std::string host;
  std::string path;

  size_t protocol_end = url.find("://");
  if (protocol_end != std::string::npos) {
    protocol = url.substr(0, protocol_end);
    size_t host_start = protocol_end + 3;
    size_t path_start = url.find("/", host_start);
    if (path_start != std::string::npos) {
      host = url.substr(host_start, path_start - host_start);
      path = url.substr(path_start);
    } else {
      host = url.substr(host_start);
      path = "/";
    }
  } else {
    return false;
  }

  if (protocol == "https") {
    httplib::SSLClient cli(host);
    cli.enable_server_certificate_verification(false);
    if (timeout_ms_ > 0)
      cli.set_max_timeout(std::chrono::milliseconds(timeout_ms_));

    if (method == "POST") {
      result_ = cli.Post(path, headers_, content.c_str(), content.size(), "application/json");
    } else {
      result_ = cli.Get(path, headers_);
    }
  } else {
    httplib::Client cli(host);
    if (timeout_ms_ > 0)
      cli.set_max_timeout(std::chrono::milliseconds(timeout_ms_));

    if (method == "POST") {
      result_ = cli.Post(path, headers_, content.c_str(), content.size(), "application/json");
    } else {
      result_ = cli.Get(path, headers_);
    }
  }
 
  return result_;
}

void HttpClient::SetHeader(const std::string& key, const std::string& value) {
  headers_.emplace(key, value);
}

void HttpClient::Close() {

}

int HttpClient::GetStatusCode() const {
  return !!result_ ? result_->status : 400;
}

std::string HttpClient::GetResponseHeader(const std::string& key) const {
  if (result_) {
    auto it = result_->headers.find(key);
    if (it != result_->headers.end()) {
      return it->second;
    }
  }

  return std::string();
}

size_t HttpClient::GetBodyLength() const {
  return !!result_ ? result_->body.size() : 0;
}

const std::string& HttpClient::GetBody() {
  return !!result_ ? result_->body : empty;
}

int HttpClient::Read(char* buffer, size_t buffer_size) {
  if (result_) {
    size_t size = result_->body.size();
    if (size > buffer_size) {
      size = buffer_size;
    }
    memcpy(buffer, result_->body.data(), size);
    return (int)size;
  }

  return 0;
}

void HttpClient::SetTimeout(int timeout_ms) {
  timeout_ms_ = timeout_ms;
}

