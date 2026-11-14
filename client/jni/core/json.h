// NEXORA engine — minimal JSON DOM (parse + serialize). No exceptions, fixed pools.
#pragma once
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "math.h"

extern "C" int snprintf(char* buf, size_t cap, const char* fmt, ...);

namespace nx {

struct JsonValue;
struct JsonMember {
  char* key;
  JsonValue* value;
};

enum class JType { Null, Bool, Number, String, Array, Object };

struct JsonValue {
  JType type = JType::Null;
  bool b = false;
  double num = 0;
  char* str = nullptr;      // for String (also raw for numbers)
  int count = 0;
  JsonValue** items = nullptr;   // Array
  JsonMember* members = nullptr; // Object
};

class Json {
 public:
  Json() {}
  ~Json() { reset(); }
  Json(const Json&) = delete;
  Json& operator=(const Json&) = delete;

  bool parse(const char* text, size_t len) {
    reset();
    data_ = (char*)malloc(len + 1);
    if (!data_) return false;
    memcpy(data_, text, len);
    data_[len] = 0;
    len_ = len;
    pos_ = 0;
    root_ = parseValue();
    return root_ != nullptr;
  }
  bool parse(const char* text) { return text && parse(text, strlen(text)); }

  JsonValue* root() { return root_; }

  // ---- accessors (safe on null/wrong type) ----
  static JsonValue* get(JsonValue* v, const char* key) {
    if (!v || v->type != JType::Object) return nullptr;
    for (int i = 0; i < v->count; i++) {
      if (v->members[i].key && strcmp(v->members[i].key, key) == 0) return v->members[i].value;
    }
    return nullptr;
  }
  static JsonValue* at(JsonValue* v, int index) {
    if (!v || v->type != JType::Array || index < 0 || index >= v->count) return nullptr;
    return v->items[index];
  }
  static int length(JsonValue* v) { return v ? v->count : 0; }
  static const char* string(JsonValue* v, const char* fallback = "") {
    return (v && v->type == JType::String && v->str) ? v->str : fallback;
  }
  static double number(JsonValue* v, double fallback = 0) {
    if (!v) return fallback;
    if (v->type == JType::Number) return v->num;
    if (v->type == JType::Bool) return v->b ? 1 : 0;
    if (v->type == JType::String && v->str) return atof(v->str);
    return fallback;
  }
  static int integer(JsonValue* v, int fallback = 0) { return (int)number(v, fallback); }
  static float flt(JsonValue* v, float fallback = 0) { return (float)number(v, fallback); }
  static bool boolean(JsonValue* v, bool fallback = false) {
    if (!v) return fallback;
    if (v->type == JType::Bool) return v->b;
    if (v->type == JType::Number) return v->num != 0;
    return fallback;
  }
  // Nested path helpers: getPath(root, "user", "avatar")
  static JsonValue* path(JsonValue* v, const char* a, const char* b = nullptr, const char* c = nullptr) {
    JsonValue* cur = get(v, a);
    if (b) cur = get(cur, b);
    if (c) cur = get(cur, c);
    return cur;
  }
  static float pathf(JsonValue* v, const char* a, const char* b, const char* c, float fallback = 0) {
    return flt(path(v, a, b, c), fallback);
  }
  static const char* paths(JsonValue* v, const char* a, const char* b, const char* c, const char* fb = "") {
    return string(path(v, a, b, c), fb);
  }
  // copies a string field into a fixed buffer
  static void copyStr(JsonValue* v, const char* key, char* dst, size_t cap, const char* fallback = "") {
    const char* s = string(get(v, key), fallback);
    size_t n = strlen(s);
    if (n >= cap) n = cap - 1;
    memcpy(dst, s, n);
    dst[n] = 0;
  }

  // ---- writer ----
  struct Writer {
    char* buf = nullptr;
    size_t cap = 0, len = 0;
    bool first[32];
    int depth = 0;

    void ensure(size_t extra) {
      if (len + extra + 1 <= cap) return;
      size_t ncap = cap ? cap * 2 : 1024;
      while (ncap < len + extra + 1) ncap *= 2;
      buf = (char*)realloc(buf, ncap);
      cap = ncap;
    }
    void raw(const char* s) {
      size_t n = strlen(s);
      ensure(n);
      memcpy(buf + len, s, n);
      len += n;
      buf[len] = 0;
    }
    void ch(char c) { ensure(1); buf[len++] = c; buf[len] = 0; }
    void beginObj() { comma(); ch('{'); first[++depth] = true; }
    void endObj() { ch('}'); depth--; }
    void beginArr() { comma(); ch('['); first[++depth] = true; }
    void endArr() { ch(']'); depth--; }
    void comma() {
      if (depth < 0) return;
      if (!first[depth]) ch(',');
      first[depth] = false;
    }
    void key(const char* k) {
      comma();
      quote(k);
      ch(':');
      first[depth] = true;  // value follows without comma
    }
    void afterValue() { first[depth] = false; }
    void quote(const char* s) {
      ch('"');
      for (const char* p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
          case '"': raw("\\\""); break;
          case '\\': raw("\\\\"); break;
          case '\n': raw("\\n"); break;
          case '\r': raw("\\r"); break;
          case '\t': raw("\\t"); break;
          default:
            if (c < 0x20) {
              char tmp[8];
              snprintf(tmp, sizeof tmp, "\\u%04x", c);
              raw(tmp);
            } else ch((char)c);
        }
      }
      ch('"');
    }
    void str(const char* s) { comma(); quote(s ? s : ""); }
    void str(const char* k, const char* v) { key(k); quote(v ? v : ""); }
    void num(double v) {
      comma();
      char tmp[40];
      snprintf(tmp, sizeof tmp, "%.6g", v);
      raw(tmp);
    }
    void num(const char* k, double v) { key(k); num(v); }
    void boolean(bool v) { comma(); raw(v ? "true" : "false"); }
    void boolean(const char* k, bool v) { key(k); boolean(v); }
    void null(const char* k) { key(k); comma(); raw("null"); }
  };

 private:
  char* data_ = nullptr;
  size_t len_ = 0, pos_ = 0;
  JsonValue* root_ = nullptr;

  void reset() {
    if (data_) { free(data_); data_ = nullptr; }
    len_ = pos_ = 0;
    root_ = nullptr;
  }
  void skipWs() {
    while (pos_ < len_) {
      char c = data_[pos_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') pos_++;
      else break;
    }
  }
  char peek() { skipWs(); return pos_ < len_ ? data_[pos_] : 0; }
  bool consume(const char* lit) {
    size_t n = strlen(lit);
    if (pos_ + n > len_) return false;
    if (memcmp(data_ + pos_, lit, n) != 0) return false;
    pos_ += n;
    return true;
  }
  char* parseStringRaw() {
    // assumes current char is '"'
    pos_++;
    size_t start = pos_;
    size_t cap = 32;
    char* out = (char*)malloc(cap);
    size_t n = 0;
    while (pos_ < len_) {
      char c = data_[pos_];
      if (c == '"') { pos_++; break; }
      if (n + 8 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
      if (c == '\\' && pos_ + 1 < len_) {
        char e = data_[pos_ + 1];
        pos_ += 2;
        switch (e) {
          case 'n': out[n++] = '\n'; break;
          case 't': out[n++] = '\t'; break;
          case 'r': out[n++] = '\r'; break;
          case 'b': out[n++] = '\b'; break;
          case 'f': out[n++] = '\f'; break;
          case 'u': {
            if (pos_ + 4 <= len_) {
              char hex[5] = {data_[pos_], data_[pos_ + 1], data_[pos_ + 2], data_[pos_ + 3], 0};
              unsigned cp = (unsigned)strtoul(hex, nullptr, 16);
              pos_ += 4;
              // surrogate pair
              if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 6 <= len_ && data_[pos_] == '\\' && data_[pos_ + 1] == 'u') {
                char hex2[5] = {data_[pos_ + 2], data_[pos_ + 3], data_[pos_ + 4], data_[pos_ + 5], 0};
                unsigned lo = (unsigned)strtoul(hex2, nullptr, 16);
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                  cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                  pos_ += 6;
                }
              }
              // UTF-8 encode
              if (cp < 0x80) out[n++] = (char)cp;
              else if (cp < 0x800) {
                out[n++] = (char)(0xC0 | (cp >> 6));
                out[n++] = (char)(0x80 | (cp & 0x3F));
              } else if (cp < 0x10000) {
                out[n++] = (char)(0xE0 | (cp >> 12));
                out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                out[n++] = (char)(0x80 | (cp & 0x3F));
              } else {
                out[n++] = (char)(0xF0 | (cp >> 18));
                out[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                out[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                out[n++] = (char)(0x80 | (cp & 0x3F));
              }
            }
            break;
          }
          default: out[n++] = e;
        }
      } else {
        out[n++] = c;
        pos_++;
      }
    }
    (void)start;
    out[n] = 0;
    return out;
  }
  JsonValue* parseValue() {
    char c = peek();
    if (!c) return nullptr;
    if (c == '{') return parseObject();
    if (c == '[') return parseArray();
    if (c == '"') {
      JsonValue* v = newValue(JType::String);
      v->str = parseStringRaw();
      return v;
    }
    if (consume("true")) { JsonValue* v = newValue(JType::Bool); v->b = true; return v; }
    if (consume("false")) { JsonValue* v = newValue(JType::Bool); v->b = false; return v; }
    if (consume("null")) return newValue(JType::Null);
    // number
    size_t start = pos_;
    while (pos_ < len_) {
      char d = data_[pos_];
      if ((d >= '0' && d <= '9') || d == '-' || d == '+' || d == '.' || d == 'e' || d == 'E') pos_++;
      else break;
    }
    if (pos_ == start) { pos_++; return nullptr; }
    char tmp[48];
    size_t n = pos_ - start;
    if (n >= sizeof tmp) n = sizeof tmp - 1;
    memcpy(tmp, data_ + start, n);
    tmp[n] = 0;
    JsonValue* v = newValue(JType::Number);
    v->num = atof(tmp);
    return v;
  }
  JsonValue* parseArray() {
    pos_++;  // [
    JsonValue* v = newValue(JType::Array);
    int cap = 8;
    v->items = (JsonValue**)malloc(sizeof(JsonValue*) * cap);
    v->count = 0;
    if (peek() == ']') { pos_++; return v; }
    for (;;) {
      JsonValue* item = parseValue();
      if (!item) break;
      if (v->count == cap) { cap *= 2; v->items = (JsonValue**)realloc(v->items, sizeof(JsonValue*) * cap); }
      v->items[v->count++] = item;
      char c = peek();
      if (c == ',') { pos_++; continue; }
      if (c == ']') { pos_++; break; }
      break;
    }
    return v;
  }
  JsonValue* parseObject() {
    pos_++;  // {
    JsonValue* v = newValue(JType::Object);
    int cap = 8;
    v->members = (JsonMember*)malloc(sizeof(JsonMember) * cap);
    v->count = 0;
    if (peek() == '}') { pos_++; return v; }
    for (;;) {
      if (peek() != '"') break;
      char* key = parseStringRaw();
      if (peek() != ':') { free(key); break; }
      pos_++;
      JsonValue* val = parseValue();
      if (v->count == cap) { cap *= 2; v->members = (JsonMember*)realloc(v->members, sizeof(JsonMember) * cap); }
      v->members[v->count].key = key;
      v->members[v->count].value = val;
      v->count++;
      char c = peek();
      if (c == ',') { pos_++; continue; }
      if (c == '}') { pos_++; break; }
      break;
    }
    return v;
  }
  JsonValue* newValue(JType t) {
    JsonValue* v = (JsonValue*)calloc(1, sizeof(JsonValue));
    v->type = t;
    return v;
  }
};

// URL-encode helper for query strings
inline void urlEncode(const char* in, char* out, size_t cap) {
  static const char* hex = "0123456789ABCDEF";
  size_t o = 0;
  for (const unsigned char* p = (const unsigned char*)in; *p && o + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      out[o++] = (char)c;
    } else {
      out[o++] = '%';
      out[o++] = hex[c >> 4];
      out[o++] = hex[c & 0xF];
    }
  }
  out[o] = 0;
}

}  // namespace nx
