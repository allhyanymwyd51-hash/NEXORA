// NEXORA realtime client — one background socket thread, frames parsed on the render thread.
#include "ws.h"

#include "core/audio.h"
#include "core/engine.h"
#include "game/gameplay.h"
#include "app/screens.h"
#include "app/state.h"

namespace nx {
namespace ws {

namespace {

WebSocket gSocket;
volatile bool gWantConnect = false;
volatile bool gConnected = false;
volatile bool gRunning = false;
bool gThreadStarted = false;
int gLatency = 0;
char gLastError[96] = {0};
RoomState gRoom;
double gLastPing = 0;
double gPingSentAt = 0;

// frames queued by the socket thread for the render thread
struct Frame {
  char* data;
  int type;
};
const int kMaxFrames = 128;
Frame gFrames[kMaxFrames];
volatile int gHead = 0, gTail = 0;
volatile int gLock = 0;

void lock() { while (__atomic_exchange_n(&gLock, 1, __ATOMIC_ACQUIRE)) {} }
void unlock() { __atomic_store_n(&gLock, 0, __ATOMIC_RELEASE); }

void queue(int type, const char* data, size_t len) {
  lock();
  int next = (gHead + 1) % kMaxFrames;
  if (next != gTail) {
    char* copy = (char*)malloc(len + 1);
    memcpy(copy, data, len);
    copy[len] = 0;
    gFrames[gHead].data = copy;
    gFrames[gHead].type = type;
    gHead = next;
  }
  unlock();
}

void send(const char* json) {
  if (!gConnected) return;
  gSocket.sendText(json);
}

void socketMain() {
  while (gRunning) {
    if (gWantConnect && !gConnected) {
      char url[256];
      snprintf(url, sizeof url, "%s", api.wsUrl);
      if (gSocket.connect(url, api.token)) {
        gConnected = true;
        gLastError[0] = 0;
        // authenticate immediately; the server answers with `authed`
        if (api.token[0]) {
          char msg[420];
          snprintf(msg, sizeof msg, "{\"type\":\"auth\",\"token\":\"%s\"}", api.token);
          gSocket.sendText(msg);
        }
      } else {
        snprintf(gLastError, sizeof gLastError, "reconnect");
        gConnected = false;
      }
    }
    if (gConnected) {
      WsMessage m;
      int r = gSocket.pollMessage(m, 200);
      if (r == 1) {
        queue(m.type, m.data, m.len);
      } else if (r < 0) {
        gConnected = false;
        snprintf(gLastError, sizeof gLastError, "disconnected");
      }
      // keepalive + latency probe every 15 s
      double now = nowSeconds();
      if (now - gLastPing > 15.0) {
        gLastPing = now;
        gPingSentAt = now;
        gSocket.sendText("{\"type\":\"ping\"}");
      }
    } else {
      usleep(30000);
      // back off, then retry while the app still wants a connection
      static double lastTry = 0;
      if (gWantConnect && nowSeconds() - lastTry > 2.0) {
        lastTry = nowSeconds();
        gWantConnect = true;
      }
    }
  }
  gSocket.close();
  gConnected = false;
}

void jsonEscape(const char* in, char* out, size_t cap) {
  size_t o = 0;
  for (const char* p = in; *p && o + 8 < cap; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
    else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
    else if (c == '\r') { out[o++] = '\\'; out[o++] = ' '; }
    else if (c < 0x20) { out[o++] = ' '; }
    else out[o++] = (char)c;
  }
  out[o] = 0;
}

}  // namespace

static unsigned long gThread = 0;

void init() {
  if (gThreadStarted) return;
  gThreadStarted = true;
  gRunning = true;
  gWantConnect = false;
  pthread_create(&gThread, nullptr,
                 [](void*) -> void* {
                   socketMain();
                   return nullptr;
                 },
                 nullptr);
}

void shutdown() {
  gWantConnect = false;
  gRunning = false;
  gConnected = false;
  usleep(5000);
  lock();
  while (gTail != gHead) {
    if (gFrames[gTail].data) free(gFrames[gTail].data);
    gTail = (gTail + 1) % kMaxFrames;
  }
  unlock();
}

void connect() {
  snprintf(gLastError, sizeof gLastError, "");
  gWantConnect = true;
}
void disconnect() {
  gWantConnect = false;
  gConnected = false;
  gSocket.close();
}
bool connected() { return gConnected; }
int latencyMs() { return gLatency; }
RoomState& room() { return gRoom; }
const char* lastError() { return gLastError; }

void authenticate(const char* token) {
  char msg[420];
  snprintf(msg, sizeof msg, "{\"type\":\"auth\",\"token\":\"%s\"}", token ? token : "");
  send(msg);
}

void joinMap(const char* mapId, float x, float y, float z, float heading) {
  char msg[256];
  snprintf(msg, sizeof msg,
           "{\"type\":\"join_map\",\"mapId\":\"%s\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"heading\":%.2f}",
           mapId && *mapId ? mapId : "", (double)x, (double)y, (double)z, (double)heading);
  send(msg);
}

void leaveMap() {
  send("{\"type\":\"leave_map\"}");
  gRoom = RoomState{};
}

void sendMove(float x, float y, float z, float heading, bool moving) {
  char msg[192];
  snprintf(msg, sizeof msg,
           "{\"type\":\"move\",\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,\"heading\":%.2f,\"moving\":%s}", (double)x,
           (double)y, (double)z, (double)heading, moving ? "true" : "false");
  send(msg);
}

void sendChat(const char* body, const char* toUserId) {
  if (!body || !*body) return;
  char escaped[900];
  jsonEscape(body, escaped, sizeof escaped);
  char msg[1024];
  if (toUserId && *toUserId) snprintf(msg, sizeof msg, "{\"type\":\"chat\",\"channel\":\"dm\",\"to\":\"%s\",\"body\":\"%s\"}", toUserId, escaped);
  else snprintf(msg, sizeof msg, "{\"type\":\"chat\",\"channel\":\"map\",\"body\":\"%s\"}", escaped);
  send(msg);
}

void sendSpeaking(bool on) {
  char msg[64];
  snprintf(msg, sizeof msg, "{\"type\":\"speaking\",\"on\":%s}", on ? "true" : "false");
  send(msg);
}

void voiceJoin() { send("{\"type\":\"voice_join\"}"); }
void voiceLeave() { send("{\"type\":\"voice_leave\"}"); }

void sendVoice(const short* samples, int count) {
  // Voice frames ride the binary channel: 2-byte header + PCM16 payload.
  if (!gConnected || count <= 0) return;
  static unsigned char buf[2048];
  int bytes = count * 2;
  if (bytes > (int)sizeof(buf) - 2) bytes = sizeof(buf) - 2;
  buf[0] = (unsigned char)(count & 0xFF);
  buf[1] = (unsigned char)((count >> 8) & 0xFF);
  memcpy(buf + 2, samples, bytes);
  gSocket.sendBinary(buf, bytes + 2);
}

void inviteFriend(const char* userId, const char* mapId) {
  char msg[200];
  snprintf(msg, sizeof msg, "{\"type\":\"invite\",\"friendId\":\"%s\",\"mapId\":\"%s\"}", userId, mapId);
  send(msg);
}

// ---------------------------------------------------------------------------
// pump: parse queued frames and dispatch
// ---------------------------------------------------------------------------
void pump() {
  for (;;) {
    Frame f;
    bool have = false;
    lock();
    if (gTail != gHead) {
      f = gFrames[gTail];
      gTail = (gTail + 1) % kMaxFrames;
      have = true;
    }
    unlock();
    if (!have) break;
    if (f.type == 2) {
      // binary voice frame from a peer: [u8 peerIdx][pcm16...]
      if (f.data && strlen(f.data) > 2) {
        const int peer = (unsigned char)f.data[0];
        audioPushVoicePeer(peer, (const short*)(f.data + 2), (int)(strlen(f.data) - 2) / 2);
      }
      free(f.data);
      continue;
    }
    Json j;
    if (f.data && j.parse(f.data)) {
      JsonValue* root = j.root();
      const char* type = Json::string(Json::get(root, "type"));
      if (strcmp(type, "hello") == 0 || strcmp(type, "authed") == 0) {
        if (strcmp(type, "authed") == 0) gConnected = true;
      } else if (strcmp(type, "pong") == 0) {
        gLatency = (int)((nowSeconds() - gPingSentAt) * 1000.0);
      } else if (strcmp(type, "room_state") == 0) {
        snprintf(gRoom.mapId, sizeof gRoom.mapId, "%s", Json::string(Json::get(root, "mapId")));
        gRoom.count = Json::integer(Json::get(root, "count"), 0);
        gRoom.nameCount = 0;
        JsonValue* players = Json::get(root, "players");
        for (int i = 0; i < Json::length(players) && gRoom.nameCount < 16; i++) {
          JsonValue* pl = Json::at(players, i);
          snprintf(gRoom.names[gRoom.nameCount++], 40, "%s", Json::string(Json::get(pl, "username"), "?"));
        }
        if (game::Gameplay::get().active()) game::Gameplay::get().onRoomState(root);
      } else if (strcmp(type, "room_tick") == 0) {
        if (game::Gameplay::get().active()) game::Gameplay::get().onRoomTick(root);
      } else if (strcmp(type, "player_joined") == 0) {
        if (game::Gameplay::get().active()) game::Gameplay::get().onPlayerJoined(root);
        app::screens::onWsEvent(root);
      } else if (strcmp(type, "player_left") == 0) {
        if (game::Gameplay::get().active()) game::Gameplay::get().onPlayerLeft(root);
      } else if (strcmp(type, "chat") == 0) {
        if (game::Gameplay::get().active()) game::Gameplay::get().onChat(root);
        app::screens::onWsEvent(root);
      } else if (strcmp(type, "notification") == 0) {
        JsonValue* n = Json::get(root, "notification");
        ui::toast(Json::string(Json::get(n, "title"), "إشعار جديد"), 3.5f);
        app::screens::onWsEvent(root);
      } else if (strcmp(type, "presence") == 0 || strcmp(type, "presence_list") == 0) {
        app::screens::onWsEvent(root);
      } else if (strcmp(type, "voice_peers") == 0 || strcmp(type, "voice_peer_available") == 0 ||
                 strcmp(type, "voice_peer_left") == 0 || strcmp(type, "voice_signal") == 0) {
        app::screens::onWsEvent(root);
      } else if (strcmp(type, "error") == 0) {
        const char* code = Json::string(Json::get(root, "error"), "");
        if (strcmp(code, "bad_token") == 0 || strcmp(code, "no_token") == 0) {
          gLastError[0] = 0;
          snprintf(gLastError, sizeof gLastError, "%s", code);
          gWantConnect = false;
        } else if (strcmp(code, "room_full") == 0) {
          ui::toast("الماب ممتلئ");
        } else if (strcmp(code, "map_private") == 0) {
          ui::toast("هذا الماب غير منشور");
        }
        app::screens::onWsEvent(root);
      }
    }
    if (f.data) free(f.data);
  }
}

}  // namespace ws
}  // namespace nx
