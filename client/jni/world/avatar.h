// Shared avatar appearance + mesh builder (used by gameplay, shop preview and remote players).
#pragma once
#include "world.h"

namespace nx {

struct Appearance {
  Color skin = Color::hex(0xC98E6A);
  Color hair = Color::hex(0x241A16);
  Color shirt = Color::hex(0x5B6BFF);
  Color pants = Color::hex(0x2B2F3D);
  Color shoes = Color::hex(0xE6E9F2);
  int bodyType = 0;
  void applyJson(JsonValue* appearance);
};

// Builds the full avatar (torso, head, limbs, shoes, landing ring) into the batcher.
void buildCharacterMesh(MeshBatcher& b, const Vec3& pos, float heading, const Appearance& look, float animPhase,
                        float animSpeed, bool grounded, bool crouching, float landingPulse);

}  // namespace nx
