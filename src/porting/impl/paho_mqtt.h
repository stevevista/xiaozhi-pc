#include "mqtt.h"
#include "MQTTAsync.h"

class PahoMqtt : public Mqtt {
public:
  PahoMqtt();
  ~PahoMqtt() override;

  bool Connect(const std::string broker_address, int broker_port, const std::string client_id, const std::string username, const std::string password) override;
  void Disconnect() override;
  bool Publish(const std::string topic, const std::string payload, int qos = 0) override;
  bool Subscribe(const std::string topic, int qos = 0) override;
  bool Unsubscribe(const std::string topic) override;
  bool IsConnected() override { return connected_; }

private:
  bool Connect();

  static void connlost(void *context, char *cause);
  static void onConnect(void* context, MQTTAsync_successData* response);
  static void onDisconnect(void* context, MQTTAsync_successData* response);
  static void onConnectFailure(void* context, MQTTAsync_failureData* response);
  static void onDisconnectFailure(void* context, MQTTAsync_failureData* response);
  static void onSend(void* context, MQTTAsync_successData* response);
  static void onSendFailure(void* context, MQTTAsync_failureData* response);
  static int messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* m);
  static void onSubscribe(void* context, MQTTAsync_successData* response);
  static void onUnSubscribe(void* context, MQTTAsync_successData* response);

  /** The underlying C-lib client. */
  MQTTAsync cli_{nullptr};

  std::string username_;
  std::string password_;
  bool connected_ = false;

};
