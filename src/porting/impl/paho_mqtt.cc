#include "paho_mqtt.h"

PahoMqtt::PahoMqtt() {

}

PahoMqtt::~PahoMqtt() {
  if (cli_) {
    MQTTAsync_destroy(&cli_);
  }
}

void PahoMqtt::connlost(void *context, char *cause)
{
  auto self = reinterpret_cast<PahoMqtt*>(context);

	printf("\nConnection lost\n");
	if (cause)
		printf("     cause: %s\n", cause);

	printf("Reconnecting\n");
  self->Connect();
}


int PahoMqtt::messageArrived(void* context, char* topicName, int topicLen, MQTTAsync_message* m)
{
  auto self = reinterpret_cast<PahoMqtt*>(context);

  if (self->on_message_callback_) {
    self->on_message_callback_(topicName, std::string((const char *)m->payload, m->payloadlen));
  }

	/* not expecting any messages */
  MQTTAsync_freeMessage(&m);
  MQTTAsync_free(topicName);
	return 1;
}

void PahoMqtt::onConnect(void* context, MQTTAsync_successData* response)
{
  auto self = reinterpret_cast<PahoMqtt*>(context);
  self->connected_ = true;

  if (self->on_connected_callback_) {
    self->on_connected_callback_();
  }
}

void PahoMqtt::onConnectFailure(void* context, MQTTAsync_failureData* response)
{
	printf("Connect failed, rc %d\n", response ? response->code : 0);
}

bool PahoMqtt::Connect() {
  MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
  MQTTAsync_SSLOptions sslopts = MQTTAsync_SSLOptions_initializer;
  sslopts.enableServerCertAuth = 0;
  sslopts.verify = 1;
  //sslopts.sslVersion = 2;
  conn_opts.username = username_.c_str();
	conn_opts.password = password_.c_str();
  conn_opts.ssl = &sslopts;
  // conn_opts.connectTimeout = 200000000000000;

  conn_opts.keepAliveInterval = keep_alive_seconds_;
	conn_opts.cleansession = 1;
	conn_opts.onSuccess = onConnect;
	conn_opts.onFailure = onConnectFailure;
	conn_opts.context = this;

  int rc;
  if ((rc = MQTTAsync_connect(cli_, &conn_opts)) != MQTTASYNC_SUCCESS) {
    printf("Failed to start connect, return code %d\n", rc);
		return false;
  }
  return true;
}

bool PahoMqtt::Connect(const std::string broker_address, int broker_port, const std::string client_id, const std::string username, const std::string password) {
  username_ = username;
  password_ = password;

  int rc;
  std::string uri = "mqtts://" + broker_address + ":" + std::to_string(broker_port);

  if (cli_) {
    MQTTAsync_destroy(&cli_);
  }

  if ((rc = MQTTAsync_create(&cli_, uri.c_str(), client_id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to create client object, return code %d\n", rc);
		return false;
	}

  if ((rc = MQTTAsync_setCallbacks(cli_, this, connlost, messageArrived, NULL)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to set callback, return code %d\n", rc);
		return false;
	}

  return Connect();
}

namespace {

struct SendContext {
  std::string payload;
  PahoMqtt *self{nullptr};
};

}

void PahoMqtt::onSend(void* context, MQTTAsync_successData* response)
{
  auto ctx = reinterpret_cast<SendContext*>(context);

  printf("Message with token value %d delivery confirmed\n", response->token);

  delete ctx;
}

void PahoMqtt::onSendFailure(void* context, MQTTAsync_failureData* response)
{
  auto ctx = reinterpret_cast<SendContext*>(context);

  printf("Message send failed token %d error code %d\n", response->token, response->code);

  delete ctx;
}

bool PahoMqtt::Publish(const std::string topic, const std::string payload, int qos) {
	MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
	int rc;

  auto context = new SendContext{};
  context->payload = payload;

	opts.onSuccess = onSend;
	opts.onFailure = onSendFailure;
	opts.context = context;
	pubmsg.payload = (void*)context->payload.c_str();
	pubmsg.payloadlen = (int)context->payload.size();
	pubmsg.qos = qos;
	pubmsg.retained = 0;
	if ((rc = MQTTAsync_sendMessage(cli_, topic.c_str(), &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
	{
		printf("Failed to start sendMessage, return code %d\n", rc);
		return false;
	}
  return true;
}

void PahoMqtt::onDisconnect(void* context, MQTTAsync_successData* response)
{
  auto self = reinterpret_cast<PahoMqtt*>(context);
  self->connected_ = false;

  if (self->on_disconnected_callback_) {
    self->on_disconnected_callback_();
  }
	printf("Successful disconnection\n");
}

void PahoMqtt::onDisconnectFailure(void* context, MQTTAsync_failureData* response)
{
	printf("Disconnect failed\n");
}

void PahoMqtt::Disconnect() {
	MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
	int rc;

	opts.onSuccess = onDisconnect;
	opts.onFailure = onDisconnectFailure;
	opts.context = this;
	if ((rc = MQTTAsync_disconnect(cli_, &opts)) != MQTTASYNC_SUCCESS) {
		printf("Failed to start disconnect, return code %d\n", rc);
	}
}

void PahoMqtt::onSubscribe(void* context, MQTTAsync_successData* response)
{
	auto self = reinterpret_cast<PahoMqtt*>(context);
	printf("Successful subscription\n");
}

void PahoMqtt::onUnSubscribe(void* context, MQTTAsync_successData* response) {
  auto self = reinterpret_cast<PahoMqtt*>(context);
	printf("Successful unsubscription\n");
}

bool PahoMqtt::Subscribe(const std::string topic, int qos) {
  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  int rc;

  opts.onSuccess = onSubscribe;
	opts.context = this;

	if ((rc = MQTTAsync_subscribe(cli_, topic.c_str(), qos, &opts)) != MQTTASYNC_SUCCESS) {
    printf("Failed to subscribe %s, return code %d\n", topic.c_str(), rc);
    return false;
  }

  return true;
}
  
bool PahoMqtt::Unsubscribe(const std::string topic) {
  MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
  int rc;

  opts.onSuccess = onSubscribe;
	opts.context = this;

	if ((rc = MQTTAsync_unsubscribe(cli_, topic.c_str(), &opts)) != MQTTASYNC_SUCCESS) {
    printf("Failed to subscribe %s, return code %d\n", topic.c_str(), rc);
    return false;
  }

  return true;
}
