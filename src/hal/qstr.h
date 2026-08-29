#pragma once
/*
  qstr.h - Qymera native string type (qymera-IDF branch)

  Minimal `String`-compatible class used by the deterministic core modules.
  Implemented natively over std::string (libstdc++ ships with every ESP-IDF
  toolchain), so no Arduino runtime is needed. It only implements the subset of
  methods the Qymera sources use - do not treat it as a general-purpose String.

  The historical modules used Arduino `String`. Keeping the same type name here
  lets the deterministic core (sensors/automations/web/rules) compile with near
  zero logic changes while the whole stack runs on native ESP-IDF.
*/

#include <string>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

class String {
 public:
  String() {}
  String(const char *c) { if (c) s_ = c; }
  String(const std::string &x) : s_(x) {}
  String(char c) { s_ = c; }
  String(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); s_ = b; }
  String(unsigned int v) { char b[16]; snprintf(b, sizeof(b), "%u", v); s_ = b; }
  String(long v) { char b[16]; snprintf(b, sizeof(b), "%ld", v); s_ = b; }
  String(unsigned long v) { char b[16]; snprintf(b, sizeof(b), "%lu", v); s_ = b; }
  String(uint32_t v, uint8_t base = 10) {
    char b[16];
    if (base == 16) snprintf(b, sizeof(b), "%lX", (unsigned long)v);
    else            snprintf(b, sizeof(b), "%lu", (unsigned long)v);
    s_ = b;
  }

  String &operator=(const char *c)  { s_ = c ? c : ""; return *this; }
  String &operator=(const std::string &x) { s_ = x; return *this; }
  String &operator=(const String &o) { s_ = o.s_; return *this; }

  size_t length() const { return s_.size(); }
  bool empty() const { return s_.empty(); }
  const char *c_str() const { return s_.c_str(); }
  void reserve(size_t n) { s_.reserve(n); }

  char operator[](size_t i) const { return s_[i]; }
  char &operator[](size_t i) { return s_[i]; }

  String substring(size_t begin, size_t end) const {
    if (begin >= s_.size()) return String();
    if (end > s_.size()) end = s_.size();
    if (begin >= end) return String();
    return String(s_.substr(begin, end - begin));
  }

  int indexOf(char c) const {
    size_t p = s_.find(c);
    return (p == std::string::npos) ? -1 : (int)p;
  }

  bool startsWith(const char *p) const {
    return p && (s_.rfind(p, 0) == 0);
  }

  long toInt() const {
    if (s_.empty()) return 0;
    char *end = nullptr;
    long v = strtol(s_.c_str(), &end, 10);
    if (end == s_.c_str()) return 0;
    return v;
  }

  // ================= append =================
  String &concat(const char *c) { s_ += (c ? c : ""); return *this; }
  String &concat(const String &o) { s_ += o.s_; return *this; }
  String &concat(char c) { s_ += c; return *this; }

  String &operator+=(const char *c)  { return concat(c); }
  String &operator+=(const String &o) { return concat(o); }
  String &operator+=(char c)          { s_ += c; return *this; }
  String &operator+=(int v)           { char b[16]; snprintf(b, sizeof(b), "%d", v); s_ += b; return *this; }
  String &operator+=(unsigned int v)  { char b[16]; snprintf(b, sizeof(b), "%u", v); s_ += b; return *this; }
  String &operator+=(long v)          { char b[16]; snprintf(b, sizeof(b), "%ld", v); s_ += b; return *this; }
  String &operator+=(unsigned long v) { char b[16]; snprintf(b, sizeof(b), "%lu", v); s_ += b; return *this; }
  String &operator+=(bool b)          { s_ += (b ? "1" : "0"); return *this; }
  String &operator+=(float v)         { char b[32]; snprintf(b, sizeof(b), "%.4f", (double)v); s_ += b; return *this; }

  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator==(const char *c) const { return s_ == (c ? c : ""); }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  bool operator!=(const char *c) const { return s_ != (c ? c : ""); }

 private:
  std::string s_;
};

inline String operator+(const char *c, const String &o) {
  String r(c);
  r += o;
  return r;
}
inline String operator+(const String &o, const char *c) { return o + String(c); }
inline String operator+(const String &a, const String &b) {
  String r(a);
  r += b;
  return r;
}

inline bool operator==(const char *c, const String &o) { return o == c; }