// NEXORA engine — HTTPS client + WebSocket client (TLS through the platform's libssl).
#pragma once
#include "sys.h"

namespace nx {

// ---- socket / TLS shims (resolved at runtime) ----
struct NetSockets {
  int (*socket)(int domain, int type, int protocol);
  int (*connect)(int fd, const void* addr, unsigned int len);
  long (*send)(int fd, const void* buf, size_t len, int flags);
  long (*recv)(int fd, void* buf, size_t len, int flags);
  int (*close)(int fd);
  int (*setsockopt)(int fd, int level, int optname, const void* optval, unsigned int optlen);
  int (*getaddrinfo)(const char* node, const char* service, const void* hints, void** res);
  void (*freeaddrinfo)(void* res);
  int (*poll)(void* fds, unsigned long nfds, int timeout);
  const char* (*gai_strerror)(int);
  bool ok = false;
};
extern NetSockets net;

bool netInit();

// ---- HTTP ----
struct HttpResult {
  int status = 0;         // HTTP status, 0 on transport failure
  char* body = nullptr;   // NUL-terminated, caller frees with free()
  size_t bodyLen = 0;
  char error[128] = {0};
  ~HttpResult() { if (body) free(body); }
  HttpResult() {}
  HttpResult(const HttpResult&) = delete;
  HttpResult& operator=(const HttpResult&) = delete;
};

class Http {
 public:
  // Synchronous; call from the network worker thread only.
  static bool request(const char* method, const char* url, const char* authToken, const char* jsonBody,
                      HttpResult& out, int timeoutMs = 20000);
};

// ---- WebSocket ----
struct WsMessage {
  int type = 0;         // 1 text, 2 binary, 8 close
  char* data = nullptr; // NUL-terminated
  size_t len = 0;
};

class WebSocket {
 public:
  bool connect(const char* url, const char* authToken);
  bool sendText(const char* text);
  bool sendBinary(const void* data, size_t len);
  int pollMessage(WsMessage& out, int timeoutMs);  // 1 message, 0 timeout, -1 closed
  void close();
  bool connected() const { return m_socket >= 0; }
  int state() const { return m_state; }

 private:
  int m_socket = -1;
  void* m_ssl = nullptr;
  void* m_sslCtx = nullptr;
  bool m_secure = false;
  int m_state = 0;  // 0 idle, 1 connecting, 2 open, 3 closed
  bool writeAll(const void* data, size_t len);
  int readSome(void* data, size_t cap);
};

// App-wide game API client: asynchronous JSON calls answered on the render thread.
enum NetOp {
  NET_REGISTER = 1,
  NET_LOGIN,
  NET_ME,
  NET_LOGOUT,
  NET_MAPS,
  NET_MY_MAPS,
  NET_MAP_DETAIL,
  NET_MAP_CREATE,
  NET_MAP_UPDATE,
  NET_MAP_PUBLISH,
  NET_MAP_JOIN,
  NET_MAP_FAVORITE,
  NET_MAP_LIKE,
  NET_MAP_DELETE,
  NET_MAP_INVITE,
  NET_FRIENDS,
  NET_FRIEND_REQUEST,
  NET_FRIEND_ACCEPT,
  NET_FRIEND_DECLINE,
  NET_FRIEND_REMOVE,
  NET_USER_PROFILE,
  NET_USER_SEARCH,
  NET_USER_FOLLOW,
  NET_USER_MAPS,
  NET_CHAT_HISTORY,
  NET_CHAT_THREADS,
  NET_NOTIFICATIONS,
  NET_NOTIF_READ,
  NET_SHOP,
  NET_SHOP_BUY,
  NET_INVENTORY,
  NET_EQUIP,
  NET_LEDGER,
  NET_ECONOMY,
  NET_SETTINGS,
  NET_PROFILE,
  NET_PASSWORD,
  NET_AI_GENERATE,
  NET_AI_JOB,
  NET_AI_MODIFY,
  NET_AI_PRESETS,
  NET_PLAY_HEARTBEAT,
  NET_PLAY_LEAVE,
  NET_PASS_BUY,
  NET_PASSES_MINE,
  NET_PASS_CREATE,
  NET_MAP_SHARE,
  NET_REPORT,
  NET_BLOCK,
  NET_BLOCKED,
  NET_ADMIN_OVERVIEW,
  NET_ADMIN_USERS,
  NET_ADMIN_BAN,
  NET_ADMIN_ROLE,
  NET_ADMIN_GRANT,
  NET_ADMIN_MAPS,
  NET_ADMIN_MAP_DELETE,
  NET_ADMIN_REPORTS,
  NET_ADMIN_REPORT_RESOLVE,
  NET_ADMIN_TX,
  NET_ADMIN_AUDIT,
  NET_ADMIN_SETTINGS,
  NET_ADMIN_SETTINGS_SAVE,
  NET_ADMIN_PASSES,
  NET_META,
  NET_CUSTOM = 900
};

struct ApiClient {
  char baseUrl[192] = "http://10.0.2.2:8080";
  char wsUrl[192] = "ws://10.0.2.2:8080/ws";
  char token[256] = {0};
  char username[64] = {0};
  int userId = 0;
  bool useTls = false;

  void start();
  void stop();
  // queue an async request; `tag` is returned with the result so screens can correlate
  void call(NetOp op, const char* method, const char* path, const char* body = nullptr, int tag = 0);
  void setBaseUrl(const char* http, const char* ws);
  static void freePayload(char* p) { if (p) free(p); }
};

extern ApiClient api;

// Result handed to the render thread: op, tag, status and JSON text.
struct ApiEvent {
  int op = 0;
  int tag = 0;
  int status = 0;
  char* body = nullptr;
  size_t len = 0;
};
bool popApiEvent(ApiEvent& out);

}  // namespace nx
