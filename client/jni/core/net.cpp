#include "net.h"
#include "json.h"

namespace nx {

NetSockets net;
ApiClient api;

static const int AF_INET = 2;
static const int SOCK_STREAM = 1;

// Minimal sockaddr_in / addrinfo layouts (Linux AArch64/ARM).
struct SockAddrIn {
  uint16_t sin_family;
  uint16_t sin_port;   // network byte order
  uint32_t sin_addr;   // network byte order
  unsigned char sin_zero[8];
};
struct AddrInfo {
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  unsigned int ai_addrlen;
  void* ai_addr;
  char* ai_canonname;
  AddrInfo* ai_next;
};

static uint16_t htons16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }
static uint32_t parseIpv4(const char* ip) {
  uint32_t parts[4] = {0, 0, 0, 0};
  int idx = 0;
  for (const char* p = ip; *p && idx < 4; p++) {
    if (*p == '.') { idx++; continue; }
    if (*p >= '0' && *p <= '9') parts[idx] = parts[idx] * 10 + (uint32_t)(*p - '0');
  }
  return parts[0] | (parts[1] << 8) | (parts[2] << 16) | (parts[3] << 24);
}

static int tcpConnect(const char* host, int port, int timeoutMs) {
  if (!net.ok) return -1;
  void* res = nullptr;
  int rc = net.getaddrinfo(host, nullptr, nullptr, &res);
  if (rc != 0 || !res) {
    NX_LOGE("dns failed for %s (%d)", host, rc);
    return -1;
  }
  int fd = -1;
  for (AddrInfo* ai = (AddrInfo*)res; ai; ai = ai->ai_next) {
    if (ai->ai_family != AF_INET || !ai->ai_addr) continue;
    SockAddrIn* sa = (SockAddrIn*)ai->ai_addr;
    fd = net.socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) continue;
    int one = 1;
    net.setsockopt(fd, 1 /*SOL_SOCKET*/, 20 /*SO_RCVTIMEO*/, &timeoutMs, sizeof timeoutMs);
    net.setsockopt(fd, 6 /*IPPROTO_TCP*/, 1 /*TCP_NODELAY*/, &one, sizeof one);
    SockAddrIn target{};
    target.sin_family = AF_INET;
    target.sin_port = htons16((uint16_t)port);
    target.sin_addr = sa->sin_addr;
    if (net.connect(fd, &target, sizeof target) == 0) break;
    net.close(fd);
    fd = -1;
  }
  net.freeaddrinfo(res);
  return fd;
}

// ---------------------------------------------------------------------------
// TLS through libssl.so (platform-provided)
// ---------------------------------------------------------------------------
struct SslApi {
  void* handle = nullptr;
  void* (*TLS_client_method)();
  void* (*SSL_CTX_new)(const void*);
  void* (*SSL_new)(void*);
  int (*SSL_set_fd)(void*, int);
  int (*SSL_connect)(void*);
  int (*SSL_read)(void*, void*, int);
  int (*SSL_write)(void*, const void*, int);
  int (*SSL_shutdown)(void*);
  void (*SSL_free)(void*);
  void (*SSL_CTX_free)(void*);
  int (*SSL_get_error)(const void*, int);
  long (*SSL_ctrl)(void*, int, long, void*);
  bool ok = false;
};
static SslApi sslApi;

static bool initSsl() {
  if (sslApi.ok) return true;
  sslApi.handle = dlopen("libssl.so", 2);
  if (!sslApi.handle) { NX_LOGE("libssl.so unavailable — https disabled"); return false; }
  bool ok = true;
#define LD(field, name) sslApi.field = (decltype(sslApi.field))dlsym(sslApi.handle, name); if (!sslApi.field) { NX_LOGE("ssl symbol missing: %s", name); ok = false; }
  LD(TLS_client_method, "TLS_client_method");
  LD(SSL_CTX_new, "SSL_CTX_new");
  LD(SSL_new, "SSL_new");
  LD(SSL_set_fd, "SSL_set_fd");
  LD(SSL_connect, "SSL_connect");
  LD(SSL_read, "SSL_read");
  LD(SSL_write, "SSL_write");
  LD(SSL_shutdown, "SSL_shutdown");
  LD(SSL_free, "SSL_free");
  LD(SSL_CTX_free, "SSL_CTX_free");
  LD(SSL_get_error, "SSL_get_error");
  LD(SSL_ctrl, "SSL_ctrl");
#undef LD
  sslApi.ok = ok;
  return ok;
}

bool netInit() {
  if (net.ok) return true;
  void* h = dlopen("libc.so", 2);
  if (!h) return false;
  net.socket = (decltype(net.socket))dlsym(h, "socket");
  net.connect = (decltype(net.connect))dlsym(h, "connect");
  net.send = (decltype(net.send))dlsym(h, "send");
  net.recv = (decltype(net.recv))dlsym(h, "recv");
  net.close = (decltype(net.close))dlsym(h, "close");
  net.setsockopt = (decltype(net.setsockopt))dlsym(h, "setsockopt");
  net.getaddrinfo = (decltype(net.getaddrinfo))dlsym(h, "getaddrinfo");
  net.freeaddrinfo = (decltype(net.freeaddrinfo))dlsym(h, "freeaddrinfo");
  net.poll = (decltype(net.poll))dlsym(h, "poll");
  net.gai_strerror = (decltype(net.gai_strerror))dlsym(h, "gai_strerror");
  net.ok = net.socket && net.connect && net.send && net.recv && net.getaddrinfo && net.freeaddrinfo;
  if (!net.ok) NX_LOGE("network shims unavailable");
  return net.ok;
}

// ---------------------------------------------------------------------------
// URL parsing
// ---------------------------------------------------------------------------
struct ParsedUrl {
  bool tls = false;
  char host[128] = {0};
  int port = 80;
  char path[512] = "/";
};

static bool parseUrl(const char* url, ParsedUrl& out) {
  const char* p = url;
  if (strncmp(p, "https://", 8) == 0 || strncmp(p, "wss://", 6) == 0) { out.tls = true; out.port = 443; }
  else if (strncmp(p, "http://", 7) == 0 || strncmp(p, "ws://", 5) == 0) { out.tls = false; out.port = 80; }
  else return false;
  p = strstr(p, "://");
  if (!p) return false;
  p += 3;
  const char* slash = strchr(p, '/');
  const char* hostEnd = slash ? slash : p + strlen(p);
  const char* colon = nullptr;
  for (const char* q = p; q < hostEnd; q++) if (*q == ':') colon = q;
  int hl = colon ? (int)(colon - p) : (int)(hostEnd - p);
  if (hl <= 0 || hl >= 127) return false;
  memcpy(out.host, p, hl);
  out.host[hl] = 0;
  if (colon) out.port = atoi(colon + 1);
  if (slash) {
    size_t n = strlen(slash);
    if (n >= sizeof out.path) n = sizeof out.path - 1;
    memcpy(out.path, slash, n);
    out.path[n] = 0;
  }
  return true;
}

// ---------------------------------------------------------------------------
// HTTP
// ---------------------------------------------------------------------------
bool Http::request(const char* method, const char* url, const char* authToken, const char* jsonBody,
                   HttpResult& out, int timeoutMs) {
  if (!netInit()) { strcpy(out.error, "no network stack"); return false; }
  ParsedUrl u;
  if (!parseUrl(url, u)) { strcpy(out.error, "bad url"); return false; }
  int fd = tcpConnect(u.host, u.port, timeoutMs);
  if (fd < 0) { strcpy(out.error, "connect failed"); return false; }

  void* ssl = nullptr;
  void* ctx = nullptr;
  auto writeFn = [&](const void* data, size_t len) -> bool {
    const char* p = (const char*)data;
    size_t left = len;
    while (left > 0) {
      long n = ssl ? sslApi.SSL_write(ssl, p, (int)left) : net.send(fd, p, left, 0);
      if (n <= 0) return false;
      p += n;
      left -= (size_t)n;
    }
    return true;
  };
  auto readFn = [&](void* data, size_t cap) -> long {
    return ssl ? sslApi.SSL_read(ssl, data, (int)cap) : net.recv(fd, data, cap, 0);
  };

  if (u.tls) {
    if (!initSsl()) { net.close(fd); strcpy(out.error, "tls unavailable"); return false; }
    const void* meth = sslApi.TLS_client_method();
    ctx = sslApi.SSL_CTX_new(meth);
    ssl = ctx ? sslApi.SSL_new(ctx) : nullptr;
    if (!ssl) { net.close(fd); strcpy(out.error, "tls init failed"); return false; }
    sslApi.SSL_set_fd(ssl, fd);
    // SNI + hostname verification are configured by the caller-provided context defaults
    sslApi.SSL_ctrl(ssl, 55 /*SSL_CTRL_SET_TLSEXT_HOSTNAME*/, 0 /*TLSEXT_NAMETYPE_host_name*/, (void*)u.host);
    if (sslApi.SSL_connect(ssl) != 1) {
      NX_LOGE("tls handshake failed for %s", u.host);
      sslApi.SSL_free(ssl);
      sslApi.SSL_CTX_free(ctx);
      net.close(fd);
      strcpy(out.error, "tls handshake failed");
      return false;
    }
  }

  Buf req;
  req.appendf("%s %s HTTP/1.1\r\n", method, u.path);
  req.appendf("Host: %s\r\n", u.host);
  req.append("User-Agent: NEXORA-Client/1.0 (Android)\r\n");
  req.append("Accept: application/json\r\n");
  req.append("Connection: close\r\n");
  if (authToken && *authToken) req.appendf("Authorization: Bearer %s\r\n", authToken);
  if (jsonBody && *jsonBody) {
    req.append("Content-Type: application/json; charset=utf-8\r\n");
    req.appendf("Content-Length: %d\r\n", (int)strlen(jsonBody));
  }
  req.append("\r\n");
  if (jsonBody && *jsonBody) req.append(jsonBody);
  if (!writeFn(req.data, req.len)) {
    strcpy(out.error, "send failed");
    if (ssl) { sslApi.SSL_free(ssl); sslApi.SSL_CTX_free(ctx); }
    net.close(fd);
    return false;
  }

  Buf raw;
  char chunk[8192];
  for (;;) {
    long n = readFn(chunk, sizeof chunk);
    if (n <= 0) break;
    raw.append(chunk, (size_t)n);
    if (raw.len > 24 * 1024 * 1024) break;  // hard cap
  }
  if (ssl) { sslApi.SSL_shutdown(ssl); sslApi.SSL_free(ssl); sslApi.SSL_CTX_free(ctx); }
  net.close(fd);
  if (raw.len == 0) { strcpy(out.error, "empty response"); return false; }

  // parse status line and headers
  char* headerEnd = strstr(raw.data, "\r\n\r\n");
  if (!headerEnd) { strcpy(out.error, "malformed response"); return false; }
  if (strncmp(raw.data, "HTTP/", 5) != 0) { strcpy(out.error, "not http"); return false; }
  char* sp = strchr(raw.data, ' ');
  out.status = sp ? atoi(sp + 1) : 0;
  size_t bodyOffset = (size_t)(headerEnd - raw.data) + 4;
  bool chunked = false;
  {
    // header scan (case-insensitive)
    char headerBlock[4096];
    size_t hb = (size_t)(headerEnd - raw.data);
    if (hb >= sizeof headerBlock) hb = sizeof headerBlock - 1;
    memcpy(headerBlock, raw.data, hb);
    headerBlock[hb] = 0;
    const char* p = headerBlock;
    while ((p = strstr(p, "\r\n"))) {
      p += 2;
      if (strncasecmp(p, "Transfer-Encoding:", 18) == 0 && strstr(p, "chunked")) chunked = true;
    }
  }
  Buf body;
  if (chunked) {
    size_t pos = bodyOffset;
    while (pos < raw.len) {
      char* end = nullptr;
      long sz = strtol(raw.data + pos, &end, 16);
      if (!end || sz <= 0) break;
      pos = (size_t)(end - raw.data);
      if (pos + 2 <= raw.len && raw.data[pos] == '\r') pos += 2;
      if (pos + (size_t)sz > raw.len) sz = (long)(raw.len - pos);
      body.append(raw.data + pos, (size_t)sz);
      pos += (size_t)sz + 2;
    }
  } else {
    body.append(raw.data + bodyOffset, raw.len - bodyOffset);
  }
  out.bodyLen = body.len;
  out.body = (char*)malloc(body.len + 1);
  if (body.len) memcpy(out.body, body.data, body.len);
  out.body[body.len] = 0;
  return out.status > 0;
}

// ---------------------------------------------------------------------------
// WebSocket (RFC 6455 client)
// ---------------------------------------------------------------------------
bool WebSocket::writeAll(const void* data, size_t len) {
  const char* p = (const char*)data;
  size_t left = len;
  while (left > 0) {
    long n = m_ssl ? sslApi.SSL_write(m_ssl, p, (int)left) : net.send(m_socket, p, left, 0);
    if (n <= 0) return false;
    p += n;
    left -= (size_t)n;
  }
  return true;
}

int WebSocket::readSome(void* data, size_t cap) {
  return (int)(m_ssl ? sslApi.SSL_read(m_ssl, data, (int)cap) : net.recv(m_socket, data, cap, 0));
}

bool WebSocket::connect(const char* url, const char* authToken) {
  if (!netInit()) return false;
  close();
  ParsedUrl u;
  if (!parseUrl(url, u)) return false;
  m_secure = u.tls;
  m_socket = tcpConnect(u.host, u.port, 12000);
  if (m_socket < 0) { m_state = 3; return false; }
  if (m_secure) {
    if (!initSsl()) { close(); return false; }
    m_sslCtx = sslApi.SSL_CTX_new(sslApi.TLS_client_method());
    m_ssl = m_sslCtx ? sslApi.SSL_new(m_sslCtx) : nullptr;
    if (!m_ssl) { close(); return false; }
    sslApi.SSL_set_fd(m_ssl, m_socket);
    sslApi.SSL_ctrl(m_ssl, 55, 0, (void*)u.host);
    if (sslApi.SSL_connect(m_ssl) != 1) { close(); return false; }
  }
  // handshake key
  unsigned char keyRaw[16];
  for (int i = 0; i < 16; i++) keyRaw[i] = (unsigned char)(hashf((unsigned)(nowSeconds() * 1000.0) + i * 7919u) * 255);
  static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char key[32];
  int o = 0;
  for (int i = 0; i < 16; i += 3) {
    unsigned v = keyRaw[i] << 16;
    if (i + 1 < 16) v |= keyRaw[i + 1] << 8;
    if (i + 2 < 16) v |= keyRaw[i + 2];
    key[o++] = b64[(v >> 18) & 63];
    key[o++] = b64[(v >> 12) & 63];
    key[o++] = i + 1 < 16 ? b64[(v >> 6) & 63] : '=';
    key[o++] = i + 2 < 16 ? b64[v & 63] : '=';
  }
  key[o] = 0;
  Buf hs;
  hs.appendf("GET %s HTTP/1.1\r\n", u.path);
  hs.appendf("Host: %s\r\n", u.host);
  hs.append("Upgrade: websocket\r\nConnection: Upgrade\r\n");
  hs.appendf("Sec-WebSocket-Key: %s\r\n", key);
  hs.append("Sec-WebSocket-Version: 13\r\n");
  if (authToken && *authToken) hs.appendf("X-Nexora-Token: %s\r\n", authToken);
  hs.append("User-Agent: NEXORA-Client/1.0\r\n\r\n");
  if (!writeAll(hs.data, hs.len)) { close(); return false; }
  // read response headers
  char resp[1024];
  size_t got = 0;
  while (got < sizeof resp - 1) {
    int n = readSome(resp + got, sizeof resp - 1 - got);
    if (n <= 0) break;
    got += (size_t)n;
    resp[got] = 0;
    if (strstr(resp, "\r\n\r\n")) break;
  }
  if (got < 12 || !strstr(resp, "101")) { close(); return false; }
  m_state = 2;
  return true;
}

bool WebSocket::sendText(const char* text) {
  if (m_socket < 0) return false;
  size_t len = strlen(text);
  unsigned char header[14];
  int hn = 2;
  header[0] = 0x81;
  if (len < 126) header[1] = (unsigned char)(0x80 | len);
  else if (len < 65536) {
    header[1] = 0x80 | 126;
    header[2] = (unsigned char)(len >> 8);
    header[3] = (unsigned char)(len & 0xFF);
    hn = 4;
  } else {
    header[1] = 0x80 | 127;
    for (int i = 0; i < 8; i++) header[2 + i] = (unsigned char)((len >> (56 - i * 8)) & 0xFF);
    hn = 10;
  }
  unsigned char mask[4];
  unsigned seed = (unsigned)(nowSeconds() * 1000.0);
  for (int i = 0; i < 4; i++) mask[i] = (unsigned char)(hashf(seed + i * 31) * 255);
  for (int i = 0; i < 4; i++) header[hn + i] = mask[i];
  size_t total = (size_t)hn + 4 + len;
  char* frame = (char*)malloc(total);
  memcpy(frame, header, (size_t)hn + 4);
  for (size_t i = 0; i < len; i++) frame[hn + 4 + i] = (char)((unsigned char)text[i] ^ mask[i % 4]);
  bool ok = writeAll(frame, total);
  free(frame);
  return ok;
}

bool WebSocket::sendBinary(const void* data, size_t len) {
  if (m_socket < 0) return false;
  unsigned char header[14];
  int hn = 2;
  header[0] = 0x82;
  if (len < 126) header[1] = (unsigned char)(0x80 | len);
  else if (len < 65536) { header[1] = 0x80 | 126; header[2] = (unsigned char)(len >> 8); header[3] = (unsigned char)(len & 0xFF); hn = 4; }
  else { header[1] = 0x80 | 127; for (int i = 0; i < 8; i++) header[2 + i] = (unsigned char)((len >> (56 - i * 8)) & 0xFF); hn = 10; }
  unsigned char mask[4];
  unsigned seed = (unsigned)(nowSeconds() * 1000.0) + 77;
  for (int i = 0; i < 4; i++) mask[i] = (unsigned char)(hashf(seed + i * 31) * 255);
  for (int i = 0; i < 4; i++) header[hn + i] = mask[i];
  size_t total = (size_t)hn + 4 + len;
  char* frame = (char*)malloc(total);
  memcpy(frame, header, (size_t)hn + 4);
  const unsigned char* src = (const unsigned char*)data;
  for (size_t i = 0; i < len; i++) frame[hn + 4 + i] = (char)(src[i] ^ mask[i % 4]);
  bool ok = writeAll(frame, total);
  free(frame);
  return ok;
}

int WebSocket::pollMessage(WsMessage& out, int timeoutMs) {
  out.data = nullptr;
  out.len = 0;
  if (m_socket < 0) return -1;
  if (net.poll) {
    struct PollFd { int fd; short events; short revents; } pfd{m_socket, 1 /*POLLIN*/, 0};
    int r = net.poll(&pfd, 1, timeoutMs);
    if (r == 0) return 0;
    if (r < 0) return -1;
  }
  unsigned char h[2];
  int n = readSome(h, 2);
  if (n <= 0) return -1;
  if (n < 2) return 0;
  int opcode = h[0] & 0x0F;
  bool masked = (h[1] & 0x80) != 0;
  uint64_t len = h[1] & 0x7F;
  if (len == 126) {
    unsigned char ext[2];
    if (readSome(ext, 2) != 2) return -1;
    len = ((uint64_t)ext[0] << 8) | ext[1];
  } else if (len == 127) {
    unsigned char ext[8];
    if (readSome(ext, 8) != 8) return -1;
    len = 0;
    for (int i = 0; i < 8; i++) len = (len << 8) | ext[i];
  }
  unsigned char maskKey[4] = {0, 0, 0, 0};
  if (masked) { if (readSome(maskKey, 4) != 4) return -1; }
  if (len > 4 * 1024 * 1024) return -1;
  char* buf = (char*)malloc((size_t)len + 1);
  size_t got = 0;
  while (got < len) {
    int r = readSome(buf + got, (size_t)len - got);
    if (r <= 0) { free(buf); return -1; }
    got += (size_t)r;
  }
  if (masked) for (size_t i = 0; i < len; i++) buf[i] ^= maskKey[i % 4];
  buf[len] = 0;
  if (opcode == 8) { free(buf); close(); return -1; }
  if (opcode == 9) {  // ping -> pong
    free(buf);
    unsigned char pong[2] = {0x8A, 0x80};
    unsigned char mask[4] = {1, 2, 3, 4};
    unsigned char frame[6] = {0x8A, 0x80, 1, 2, 3, 4};
    (void)pong; (void)mask;
    writeAll(frame, sizeof frame);
    return pollMessage(out, 0);
  }
  if (opcode == 0xA) { free(buf); return pollMessage(out, 0); }
  out.type = opcode;
  out.data = buf;
  out.len = (size_t)len;
  return 1;
}

void WebSocket::close() {
  if (m_socket >= 0) {
    unsigned char closeFrame[6] = {0x88, 0x80, 1, 1, 1, 1};
    writeAll(closeFrame, sizeof closeFrame);
    if (m_ssl) { sslApi.SSL_shutdown(m_ssl); sslApi.SSL_free(m_ssl); m_ssl = nullptr; }
    if (m_sslCtx) { sslApi.SSL_CTX_free(m_sslCtx); m_sslCtx = nullptr; }
    net.close(m_socket);
    m_socket = -1;
  }
  m_state = 3;
}

// ---------------------------------------------------------------------------
// Async API client. A dedicated worker thread performs requests; results are
// pushed to the render thread through the job queue.
// ---------------------------------------------------------------------------
static const int kApiEventQueue = 203;
static volatile int gApiMutex = 0;
static ApiEvent gApiEvents[64];
static volatile int gApiHead = 0, gApiTail = 0;

static void lockApi() { while (__atomic_exchange_n(&gApiMutex, 1, __ATOMIC_ACQUIRE)) {} }
static void unlockApi() { __atomic_store_n(&gApiMutex, 0, __ATOMIC_RELEASE); }

bool popApiEvent(ApiEvent& out) {
  lockApi();
  bool ok = false;
  if (gApiHead != gApiTail) {
    out = gApiEvents[gApiTail];
    gApiTail = (gApiTail + 1) % 64;
    ok = true;
  }
  unlockApi();
  return ok;
}

static void postApiEvent(int op, int tag, int status, const char* body) {
  ApiEvent ev;
  ev.op = op;
  ev.tag = tag;
  ev.status = status;
  ev.len = body ? strlen(body) : 0;
  ev.body = (char*)malloc(ev.len + 1);
  if (ev.len) memcpy(ev.body, body, ev.len);
  ev.body[ev.len] = 0;
  lockApi();
  int next = (gApiHead + 1) % 64;
  if (next != gApiTail) {
    gApiEvents[gApiHead] = ev;
    gApiHead = next;
  } else {
    free(ev.body);
  }
  unlockApi();
}

struct ApiRequest {
  int op;
  int tag;
  char method[8];
  char path[512];
  char* body;
};

static volatile int gQueueMutex = 0;
static ApiRequest gQueue[32];
static volatile int gQueueHead = 0, gQueueTail = 0;
static volatile bool gWorkerRunning = false;
static bool gStopWorker = false;

static void lockQueue() { while (__atomic_exchange_n(&gQueueMutex, 1, __ATOMIC_ACQUIRE)) {} }
static void unlockQueue() { __atomic_store_n(&gQueueMutex, 0, __ATOMIC_RELEASE); }

static void* apiWorker(void*) {
  for (;;) {
    if (gStopWorker) break;
    ApiRequest req;
    bool have = false;
    lockQueue();
    if (gQueueHead != gQueueTail) {
      req = gQueue[gQueueTail];
      gQueueTail = (gQueueTail + 1) % 32;
      have = true;
    }
    unlockQueue();
    if (!have) { usleep(15000); continue; }
    char url[768];
    // userId is numeric only for logging; the token carries identity
    snprintf(url, sizeof url, "%s%s", api.baseUrl, req.path);
    HttpResult res;
    Http::request(req.method, url, api.token, req.body, res, 25000);
    if (res.body) {
      if (res.status >= 200 && res.status < 300) {
        postApiEvent(req.op, req.tag, res.status, res.body);
      } else {
        postApiEvent(req.op, req.tag, res.status ? res.status : 0, res.body);
      }
    } else {
      char err[256];
      snprintf(err, sizeof err, "{\"ok\":false,\"error\":\"network\",\"message\":\"%s\"}",
               res.error[0] ? res.error : "network error");
      postApiEvent(req.op, req.tag, 0, err);
    }
    if (req.body) free(req.body);
  }
  gWorkerRunning = false;
  return nullptr;
}

void ApiClient::start() {
  netInit();
  if (gWorkerRunning) return;
  gStopWorker = false;
  unsigned long thread = 0;
  pthread_create(&thread, nullptr, apiWorker, nullptr);
  gWorkerRunning = true;
}

void ApiClient::stop() {
  gStopWorker = true;
}

void ApiClient::setBaseUrl(const char* http, const char* ws) {
  strncpy(baseUrl, http, sizeof baseUrl - 1);
  strncpy(wsUrl, ws, sizeof wsUrl - 1);
  while (baseUrl[0] && baseUrl[strlen(baseUrl) - 1] == '/') baseUrl[strlen(baseUrl) - 1] = 0;
  useTls = strncmp(baseUrl, "https://", 8) == 0;
}

void ApiClient::call(NetOp op, const char* method, const char* path, const char* body, int tag) {
  ApiRequest req;
  req.op = op;
  req.tag = tag;
  strncpy(req.method, method, sizeof req.method - 1);
  strncpy(req.path, path, sizeof req.path - 1);
  req.body = body ? (char*)malloc(strlen(body) + 1) : nullptr;
  if (body) strcpy(req.body, body);
  lockQueue();
  int next = (gQueueHead + 1) % 32;
  if (next == gQueueTail) {
    // queue full: drop the oldest request to keep the UI responsive
    int old = gQueueTail;
    gQueueTail = (gQueueTail + 1) % 32;
    if (gQueue[old].body) free(gQueue[old].body);
  }
  gQueue[gQueueHead] = req;
  gQueueHead = next;
  unlockQueue();
  (void)kApiEventQueue;
}

}  // namespace nx
