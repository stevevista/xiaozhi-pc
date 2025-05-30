#pragma once
#include  "udp.h"


#if defined(_WIN32) || defined(_WIN64)
#include <errno.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define MAXHOSTNAMELEN 256
#define poll WSAPoll
#if !defined(SSLSOCKET_H)
#undef EAGAIN
#define EAGAIN WSAEWOULDBLOCK
#undef EINTR
#define EINTR WSAEINTR
#undef EINPROGRESS
#define EINPROGRESS WSAEINPROGRESS
#undef EWOULDBLOCK
#define EWOULDBLOCK WSAEWOULDBLOCK
#undef ENOTCONN
#define ENOTCONN WSAENOTCONN
#undef ECONNRESET
#define ECONNRESET WSAECONNRESET
#undef ETIMEDOUT
#define ETIMEDOUT WAIT_TIMEOUT
#endif
#else
#define INVALID_SOCKET SOCKET_ERROR
#include <sys/socket.h>
#if !defined(_WRS_KERNEL)
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <poll.h>
#include <sys/uio.h>
#else
#include <selectLib.h>
#endif
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#define SOCKET int
#endif

#include <mutex>


class UdpClient : public Udp {
public:
  ~UdpClient() override;

  bool Connect(const std::string& host, int port) override;
  void Disconnect() override;
  int Send(const std::string& data) override;

private:
  void ReceiveLoop();

  SOCKET socket_ = INVALID_SOCKET;
  sockaddr_in server_addr_{};
  std::thread receiver_thread_;
  std::mutex mutex_;
  bool connected_ = false;
};
