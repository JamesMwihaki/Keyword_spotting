#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <ArduinoWebsockets.h>
#include <functional>

using namespace websockets;

class WebSocketManager {
public:
  typedef std::function<void(const char *msg)> TextCallback;
  typedef std::function<void(const uint8_t *data, size_t len)> BinaryCallback;

  void begin(const char *host, int port, const char *path);
  void update();
  bool isConnected();
  bool connect();
  void close();
  void sendBinary(const uint8_t *data, size_t len);
  void sendText(const char *msg);

  void onText(TextCallback cb);
  void onBinary(BinaryCallback cb);

private:
  WebsocketsClient client;
  const char *_host;
  int _port;
  const char *_path;
  volatile bool _connected = false;

  TextCallback _textCallback;
  BinaryCallback _binaryCallback;

  void _onMessage(WebsocketsMessage message);
};

#endif
