/* qhttpserver.cpp - native esp_http_server wrapper (qymera-IDF branch) */

#include "qhttpserver.h"

#include "qhal.h"

#include <string.h>
#include <stdio.h>

static const char *status_phrase(int code) {
  switch (code) {
    case 200: return "OK";
    case 204: return "No Content";
    case 303: return "See Other";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 429: return "Too Many Requests";
    default:  return "";
  }
}

static void url_decode(char *dst, const char *src) {
  int a = 0, b = 0;
  for (; *src; src++) {
    if (*src == '%') {
      if (sscanf(src + 1, "%2x", &a) != 1) a = 0;
      b = (a >= 32) ? (int)(char)a : (int)(unsigned char)a;
      dst[0] = (char)b;
      src += 2;
    } else if (*src == '+') {
      dst[0] = ' ';
    } else {
      dst[0] = *src;
    }
    dst++;
  }
  dst[0] = '\0';
}

QymeraServer web_server_compat(80);

QymeraServer::QymeraServer(int port) : port_(port) {}

void QymeraServer::on(const char *uri, HttpMethod method, QymeraHandlerFn fn) {
  routes_.push_back({uri, method, fn});
}

void QymeraServer::begin() {
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.port = port_;
  cfg.max_open_sockets = 4;
  cfg.lru_purge_enable = true;
  cfg.stack_size = CONFIG_QYMERA_LOOP_STACK_SIZE;

  if (httpd_start(&handle_, &cfg) != ESP_OK) return;

  for (auto &r : routes_) {
    httpd_uri_t h = {};
    h.uri = r.uri;
    h.method = (r.method == HTTP_GET) ? HTTPD_GET
             : (r.method == HTTP_POST) ? HTTPD_POST
             : (r.method == HTTP_OPTIONS) ? HTTPD_OPTIONS : HTTPD_GET;
    h.handler = trampoline;
    h.user_ctx = &r;
    httpd_register_uri_handler(handle_, &h);
  }
}

// ================= internal: headers / args =================

std::string QymeraServer::headerValue(const char *name) {
  if (!req_) return "";
  char buf[128] = {0};
  if (httpd_req_get_hdr_value_str(req_, name, buf, sizeof(buf)) == ESP_OK) {
    return std::string(buf);
  }
  return std::string();
}

void QymeraServer::parseUrlArgs(const char *data, size_t len) {
  // split "a=b&c=d", url-decode keys and values
  size_t pos = 0;
  while (pos < len) {
    size_t amp = pos;
    while (amp < len && data[amp] != '&') amp++;
    size_t eq = pos;
    while (eq < amp && data[eq] != '=') eq++;

    std::string key_raw(data + pos, eq - pos);
    std::string val_raw(data + (eq < amp ? eq + 1 : eq), (eq < amp ? amp - eq - 1 : 0));

    char key[256], val[512];
    url_decode(key, key_raw.c_str());
    url_decode(val, val_raw.c_str());
    if (key[0]) args_.emplace_back(key, val);

    pos = amp + 1;
  }
}

// ================= request context =================

void QymeraServer::beginRequest(httpd_req_t *req) {
  req_ = req;
  args_.clear();
  method_ = HTTP_ANY;

  switch (req->method) {
    case HTTPD_GET:     method_ = HTTP_GET;     break;
    case HTTPD_POST:    method_ = HTTP_POST;    break;
    case HTTPD_OPTIONS: method_ = HTTP_OPTIONS; break;
    case HTTPD_HEAD:    method_ = HTTP_HEAD;    break;
    case HTTPD_PUT:     method_ = HTTP_PUT;     break;
    case HTTPD_DELETE:  method_ = HTTP_DELETE;  break;
    default: break;
  }

  // query string (httpd stores it in req->aux, without the leading '?')
  if (req->aux) {
    const char *query = static_cast<const char *>(req->aux);
    if (query[0]) parseUrlArgs(query, strlen(query));
  }
  // form body (POST)
  if (method_ == HTTP_POST && req->content_len > 0) {
    size_t cap = req->content_len > 4096 ? 4096 : req->content_len;
    std::string body(cap, '\0');
    int total = 0;
    while (total < (int)cap) {
      int r = httpd_req_recv(req, &body[total], cap - total);
      if (r <= 0) break;
      total += r;
    }
    body.resize(total);
    parseUrlArgs(body.data(), total);
  }

  status_ = 0;
  has_status_ = false;
  content_type_.clear();
  resp_headers_.clear();
  resp_buf_.clear();
  flushed_ = false;
  request_active_ = true;
}

void QymeraServer::endRequest() {
  if (!request_active_) return;
  if (!flushed_) doSend();
  request_active_ = false;
  req_ = nullptr;
}

// ================= request accessors =================

HttpMethod QymeraServer::method() {
  return method_;
}

bool QymeraServer::hasArg(const char *name) {
  for (auto &a : args_) {
    if (a.first == name) return true;
  }
  return false;
}

String QymeraServer::arg(const char *name) {
  for (auto &a : args_) {
    if (a.first == name) return String(a.second.c_str());
  }
  return String();
}

bool QymeraServer::hasHeader(const char *name) {
  return !headerValue(name).empty();
}

String QymeraServer::header(const char *name) {
  return String(headerValue(name).c_str());
}

// ================= response =================

void QymeraServer::sendHeader(const char *name, const char *value) {
  resp_headers_.emplace_back(name, value);
}

void QymeraServer::setContentLength(size_t /*len*/) {
}

void QymeraServer::send(int code, const char *content_type, const char *body) {
  if (!has_status_) {
    status_ = code;
    has_status_ = true;
  }
  if (content_type && content_type_[0] == '\0') content_type_ = content_type;
  if (body) resp_buf_ += body;
}

void QymeraServer::send(int code, const char *content_type, const std::string &body) {
  send(code, content_type, body.c_str());
}

void QymeraServer::send(int code, const char *content_type, const String &body) {
  send(code, content_type, body.c_str());
}

void QymeraServer::sendContent(const char *content) {
  if (content) resp_buf_ += content;
}

void QymeraServer::sendContent(const String &content) {
  resp_buf_ += content.c_str();
}

// ================= send / flush =================

void QymeraServer::doSend() {
  if (!req_ || flushed_) return;

  char status[32];
  const char *phrase = status_phrase(status_);
  if (phrase[0]) {
    snprintf(status, sizeof(status), "%d %s", status_, phrase);
  } else {
    snprintf(status, sizeof(status), "%d", status_);
  }
  httpd_resp_set_status(req_, status);
  if (!content_type_.empty()) httpd_resp_set_type(req_, content_type_.c_str());
  for (auto &h : resp_headers_) {
    httpd_resp_set_hdr(req_, h.first.c_str(), h.second.c_str());
  }

  // chunked to keep per-call copies small
  size_t pos = 0;
  const char *data = resp_buf_.data();
  size_t total = resp_buf_.size();
  while (pos < total) {
    size_t n = (total - pos > 512) ? 512 : (total - pos);
    httpd_resp_send_chunk(req_, data + pos, n);
    pos += n;
  }
  httpd_resp_send_chunk(req_, nullptr, 0);
  flushed_ = true;
}

void QymeraServer::flush() {
  doSend();
}

// ================= trampoline =================

esp_err_t QymeraServer::trampoline(httpd_req_t *req) {
  QhalLockGuard guard;   // serialize against main loop and other httpd tasks
  auto *r = static_cast<Route *>(req->user_ctx);
  if (!r || !r->fn) {
    httpd_resp_send_408(req);
    return ESP_OK;
  }
  web_server_compat.beginRequest(req);
  r->fn();
  web_server_compat.endRequest();
  return ESP_OK;
}