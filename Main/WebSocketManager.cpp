#include "WebSocketManager.h"

void WebSocketManager::begin(const char *host, int port, const char *path) {
  _host = host;
  _port = port;
  _path = path;

  // Bind the internal callback to our wrapper
  client.onMessage([this](WebsocketsMessage msg) { this->_onMessage(msg); });
}

bool WebSocketManager::connect() {
  bool result = client.connect(_host, _port, _path);
  _connected = result;
  return result;
}

void WebSocketManager::update() {
  if (!_connected) return;
  if (client.available()) {
    client.poll();
  }
}

bool WebSocketManager::isConnected() {
  if (!_connected) return false;
  return client.available();
}

void WebSocketManager::close() {
  _connected = false;
  client.close();
}

void WebSocketManager::sendBinary(const uint8_t *data, size_t len) {
  client.sendBinary((const char *)data, len);
}

void WebSocketManager::sendText(const char *msg) { client.send(msg); }

void WebSocketManager::onText(TextCallback cb) { _textCallback = cb; }

void WebSocketManager::onBinary(BinaryCallback cb) { _binaryCallback = cb; }

void WebSocketManager::_onMessage(WebsocketsMessage message) {
  if (message.isBinary()) {
    if (_binaryCallback) {
      // Use rawData() which returns std::string - properly handles binary data
      // with embedded nulls
      auto &rawData = message.rawData();
      _binaryCallback((const uint8_t *)rawData.data(), rawData.length());
    }
  } else {
    if (_textCallback) {
      _textCallback(message.data().c_str());
    }
  }
}
