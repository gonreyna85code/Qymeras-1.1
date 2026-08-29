#pragma once
/*
  qhttpserver.h - native esp_http_server wrapper (qymera-IDF branch)

  Replaces Arduino's ESP8266WebServer / WebServer with a minimal, buffered
  wrapper over esp_http_server. Handlers keep the same call shape used by the
  historical web.cpp (send/arg/hasArg/sendContent...) so the HTTP logic ports
  with minimal churn.

  Threading: esp_http_server runs handlers on its own worker tasks. Each request
  is serialized against the Qymera main loop via the qhal recursive mutex, so
  handler bodies behave exactly like the historical single-threaded loop.
*/

#include <stdint.h>
#include <stddef.h>

#include "qstr.h"

#include <string>
#include <vector>

#include "esp_http_server.h"

// Arduino-compatible method constants used by web.cpp.
enum HttpMethod : uint8_t {
  HTTP_ANY = 0,
  HTTP_GET = 1,
  HTTP_HEAD = 2,
  HTTP_POST = 3,
  HTTP_PUT = 4,
  HTTP_DELETE = 5,
  HTTP_OPTIONS = 6,
  HTTP_PATCH = 7,
  HTTP_TRACE = 8,
  HTTP_CONNECT = 9
};

#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)

typedef void (*QymeraHandlerFn)();

class QymeraServer {
 public:
  explicit QymeraServer(int port = 80);

  void on(const char *uri, HttpMethod method, QymeraHandlerFn fn);

  // httpd owns processing; both are kept for source compatibility with the
  // historical loop(). handleClient() is a no-op.
  void begin();
  void handleClient() {}
  void close() { flush(); }
  void flush();                    // send buffered response now (e.g. before reset)

  // ---- request context (valid only inside a handler) ----
  HttpMethod method();
  bool hasArg(const char *name);
  String arg(const char *name);
  bool hasHeader(const char *name);
  String header(const char *name);

  // ---- response ----
  void sendHeader(const char *name, const char *value);
  void setContentLength(size_t len);          // accepted, ignored (buffered)
  void send(int code, const char *content_type = nullptr, const char *body = nullptr);
  void send(int code, const char *content_type, const std::string &body);
  void send(int code, const char *content_type, const String &body);
  void sendContent(const char *content);
  void sendContent(const String &content);
  void sendContent_P(const char *content) { sendContent(content); }

  // ---- internal (dispatcher trampoline) ----
  void beginRequest(httpd_req_t *req);   // populate context (caller holds lock)
  void endRequest();                     // flush leftover response

 private:
  struct Route {
    const char *uri;
    HttpMethod method;
    QymeraHandlerFn fn;
  };

  int port_;
  httpd_handle_t handle_ = nullptr;
  std::vector<Route> routes_;
  std::string uri_;

  httpd_req_t *req_ = nullptr;
  HttpMethod method_ = HTTP_ANY;
  bool request_active_ = false;

  int status_ = 0;
  bool has_status_ = false;
  std::string content_type_;
  std::vector<std::pair<std::string, std::string>> resp_headers_;
  std::string resp_buf_;
  bool flushed_ = false;

  std::vector<std::pair<std::string, std::string>> args_;

  std::string headerValue(const char *name);
  void parseUrlArgs(const char *data, size_t len);
  void doSend();
  static esp_err_t trampoline(httpd_req_t *req);
};

extern QymeraServer web_server_compat;