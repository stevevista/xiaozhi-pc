#include "udp_client.h"

namespace {

void init() {
  static bool inited = false;
  if (!inited) {
    inited = true;
    WORD winsockVer = 0x0202;
    WSADATA wsd;
    WSAStartup(winsockVer, &wsd);
  }
}

}

UdpClient::~UdpClient() {
  Disconnect();
}

/*
message arrival: {
"type":"hello",
"version":3,
"session_id":"c7babd72",
"transport":"udp","udp":{
"server":"120.24.160.13",
"port":8846,"encryption":"aes-128-ctr","key":"0585a874cc7cde39af478c7c634c3bcb","nonce":"010000005cfd85170000000000000000"},"audio_params":{"format":"opus","sample_rate":24000,"channels":1,"frame_duration":60}}
*/
bool UdpClient::Connect(const std::string& host, int port) {
  init();

  std::lock_guard<std::mutex> lock(mutex_);
  if (connected_) return true;

  // Create socket
  socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_ == INVALID_SOCKET) {
    return false;
  }

        // Configure server address
  server_addr_.sin_family = AF_INET;
  server_addr_.sin_port = htons(port);
  if (inet_pton(AF_INET, host.c_str(), &server_addr_.sin_addr) <= 0) {
    closesocket(socket_);
    return false;
  }

  int rc = connect(socket_, (struct sockaddr *)&server_addr_, sizeof(server_addr_));

  // Start receiver thread
  connected_ = true;
  receiver_thread_ = std::thread(&UdpClient::ReceiveLoop, this);
  return true;
}

void UdpClient::Disconnect() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!connected_) return;

  connected_ = false;
        
        // Close socket to interrupt blocking recvfrom
  if (socket_ != INVALID_SOCKET) {
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
  }

  if (receiver_thread_.joinable()) {
    receiver_thread_.join();
  }
}

int UdpClient::Send(const std::string& data) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!connected_ || socket_ == -1) return -1;

  int sent = sendto(socket_, 
                            data.data(), 
                            static_cast<int>(data.size()), 
                            0,
                            reinterpret_cast<sockaddr*>(&server_addr_),
                            sizeof(server_addr_));
        
  return sent;
}

void UdpClient::ReceiveLoop() {
  char buffer[4096]; // Max UDP payload size
        
  while (true) {
    sockaddr_in from_addr{};
    socklen_t from_len = sizeof(from_addr);
            
    // Use mutex-protected socket descriptor
    SOCKET current_socket;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!connected_) break;
      current_socket = socket_;
    }

    int bytes = recv(current_socket,
                                   buffer,
                                   sizeof(buffer),
                                   0);

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!connected_) break;
    }

    if (bytes > 0) {
      if (message_callback_) {
        message_callback_(std::string(buffer, bytes));
      }
    } else if (bytes == -1) {
        if (errno == EBADF) break; // Socket closed
        if (errno == EINTR) continue; // Interrupted
        break; // Other errors
    }
  }

  // Ensure clean shutdown
  std::lock_guard<std::mutex> lock(mutex_);
  connected_ = false;
}
