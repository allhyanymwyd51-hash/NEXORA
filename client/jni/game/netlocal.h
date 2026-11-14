// Bridge between the app layer and the private character/camera implementation.
#pragma once
#include "world/avatar.h"

namespace nx {
namespace app {

struct AvatarState {
  Vec3 pos;
  float heading;
  float animPhase;
  float animSpeed;
  bool grounded;
  bool crouching;
  float landingPulse;
  Appearance look;
};

void gameplayCreate(void** outPlayer, void** outCamera);
void gameplayDestroy(void* player, void* camera);
void gameplaySpawn(void* player, const Vec3& p, float face);
void gameplayUpdate(void* player, float dt, float fwd, float strafe, bool run, bool crouch, bool jump, float yaw,
                    const Terrain& terrain, GameWorld& world, bool locked);
Vec3 gameplayEye(void* player);
void gameplayAvatarState(void* player, AvatarState* out);
void gameplayCameraUpdate(void* cam, float dt, const Vec3& focus, const Terrain& terrain, float dx, float dy,
                          bool running);
Mat4 gameplayCameraView(void* cam);
Vec3 gameplayCameraForward(void* cam);
Vec3 gameplayCameraPosition(void* cam);

}  // namespace app
}  // namespace nx
