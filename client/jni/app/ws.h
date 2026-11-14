// NEXORA realtime client: authenticated WebSocket session for presence, map rooms,
// chat and voice signalling. Runs on its own thread; frames are parsed on the render thread.
#pragma once
#include "core/json.h"
#include "core/net.h"

namespace nx {
namespace ws {

// lifecycle
void init();
void shutdown();
void connect();
void disconnect();
bool connected();
int latencyMs();

// protocol
void authenticate(const char* token);
void joinMap(const char* mapId, float x, float y, float z, float heading);
void leaveMap();
void sendMove(float x, float y, float z, float heading, bool moving);
void sendChat(const char* body, const char* toUserId = nullptr);
void sendSpeaking(bool on);
void voiceJoin();
void voiceLeave();
void sendVoice(const short* samples, int count);
void inviteFriend(const char* userId, const char* mapId);

// pump frames received since the last call; dispatches into the app + gameplay
void pump();

// room state shared with the UI
struct RoomState {
  char mapId[48] = {0};
  int count = 0;
  char names[16][40] = {};
  int nameCount = 0;
};
RoomState& room();
const char* lastError();

}  // namespace ws
}  // namespace nx
