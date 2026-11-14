// NEXORA app — shared session/profile state and settings persisted on the device.
#pragma once
#include "core/engine.h"
#include "core/json.h"

namespace nx {
namespace app {

enum class Role { Player, Moderator, Owner };

struct State {
  // session
  char token[256] = {0};
  int userId = 0;
  char uid[48] = {0};        // server-side id string (usr_…), used for DMs and routes
  char server[96] = {0};     // host[:port] the player pointed the client at ("" = build default)
  char username[64] = {0};
  char roleName[24] = "player";
  Role role = Role::Player;
  int miy = 0;
  int level = 1;
  int friends = 0;
  int unread = 0;
  char bio[240] = {0};

  // device preferences
  int language = 0;          // 0 = Arabic (RTL), 1 = English (LTR)
  int quality = 4;           // 0 low .. 3 ultra, 4 auto
  bool music = true;
  bool sfx = true;
  bool voice = true;
  bool haptics = true;
  float musicVolume = 0.55f;
  float sfxVolume = 0.9f;

  // cached lists used across screens
  int loadedMaps = 0;
};

extern State S;

void loadLocalSettings();
void saveLocalSettings();
bool isOwner();
bool isRtl();

// session transitions
void applyLogin(JsonValue* root);
void applyMe(JsonValue* root);
void signOut();

}  // namespace app
}  // namespace nx
