// Third-person character: input, physics, collision, animation and the local avatar mesh.
#include "world.h"
#include "avatar.h"
#include "core/audio.h"
#include "game/netlocal.h"

namespace nx {

// Mesh builders shared by the local avatar, remote players and the shop preview.
void buildCharacterMesh(MeshBatcher& b, const Vec3& pos, float heading, const Appearance& look, float animPhase,
                        float animSpeed, bool grounded, bool crouching, float landingPulse) {
  float bodyH = crouching ? 1.05f : 1.45f;
  float headR = 0.24f;
  float legLen = crouching ? 0.42f : 0.72f;
  bool running = animSpeed > 5.0f;
  float stride = sinf(animPhase) * (running ? 0.42f : 0.26f);
  float stride2 = sinf(animPhase + PI) * (running ? 0.42f : 0.26f);
  float armSwing = sinf(animPhase + PI) * (running ? 0.5f : 0.3f);
  float bob = grounded ? fabsf(sinf(animPhase)) * 0.045f * clampf(animSpeed / 4.0f, 0, 1) : 0.0f;
  Vec3 base(pos.x, pos.y + bob, pos.z);
  float torsoY0 = base.y + legLen;
  float torsoY1 = torsoY0 + bodyH * 0.62f;
  float cs = cosf(heading), sn = sinf(heading);
  auto rot = [&](float lx, float lz) { return Vec3(base.x + lx * cs - lz * sn, 0, base.z + lx * sn + lz * cs); };
  // torso: rotated box built from 8 corner points
  {
    Vec3 p00 = rot(-0.28f, -0.16f), p01 = rot(-0.28f, 0.16f), p10 = rot(0.28f, -0.16f), p11 = rot(0.28f, 0.16f);
    Vec3 c000(fminf(fminf(p00.x, p01.x), fminf(p10.x, p11.x)), torsoY0, fminf(fminf(p00.z, p01.z), fminf(p10.z, p11.z)));
    Vec3 c111(fmaxf(fmaxf(p00.x, p01.x), fmaxf(p10.x, p11.x)), torsoY1, fmaxf(fmaxf(p00.z, p01.z), fmaxf(p10.z, p11.z)));
    pushBoxTinted(b, c000, c111, look.shirt * 1.1f, look.shirt, look.shirt * 0.7f);
  }
  Vec3 headPos = rot(0, 0);
  headPos.y = torsoY1 + headR + 0.02f;
  pushSphere(b, headPos, headR, 5, 7, look.skin * 1.08f, look.skin * 0.86f);
  pushSphere(b, headPos + Vec3(0, headR * 0.35f, 0), headR * 0.95f, 3, 7, look.hair, look.hair * 0.8f);
  for (int side = 0; side < 2; side++) {
    float lx = side == 0 ? -0.13f : 0.13f;
    Vec3 hip = rot(lx, 0);
    hip.y = torsoY0;
    Vec3 foot = rot(lx, (side == 0 ? stride : stride2));
    foot.y = base.y + 0.06f;
    pushCapsule(b, hip, foot, 0.095f, 5, look.pants);
    pushBox(b, Vec3(foot.x, foot.y, foot.z), Vec3(0.11f, 0.06f, 0.16f), look.shoes, look.shoes * 0.85f, heading, 0.6f);
  }
  for (int side = 0; side < 2; side++) {
    float lx = side == 0 ? -0.34f : 0.34f;
    Vec3 shoulder = rot(lx, 0);
    shoulder.y = torsoY1 - 0.12f;
    Vec3 hand = rot(lx, (side == 0 ? armSwing : -armSwing));
    hand.y = shoulder.y - 0.55f;
    pushCapsule(b, shoulder, hand, 0.075f, 5, look.shirt * 0.94f);
    pushSphere(b, hand, 0.085f, 3, 5, look.skin, look.skin * 0.9f);
  }
  if (landingPulse > 0.01f) {
    pushRing(b, Vec3(base.x, base.y + 0.03f, base.z), 0.35f, 0.35f + landingPulse * 0.6f, 18,
             Color(1, 1, 1, 0.28f * landingPulse), 0.03f);
  }
}

class Character {
 public:
  Vec3 pos{0, 0, 0};
  Vec3 vel{0, 0, 0};
  float heading = 0;
  float desiredHeading = 0;
  bool grounded = true;
  bool running = false;
  bool crouching = false;
  float animPhase = 0;
  float animSpeed = 0;
  float jumpCooldown = 0;
  float landingPulse = 0;
  Appearance look;
  bool inWaterNow = false;

  void spawn(const Vec3& p, float face) {
    pos = p;
    vel = Vec3{0, 0, 0};
    heading = desiredHeading = face;
    grounded = true;
    landingPulse = 0;
    animPhase = 0;
  }

  void update(float dt, float forward, float strafe, bool wantRun, bool wantCrouch, bool wantJump, float cameraYaw,
              const Terrain& terrain, GameWorld& world, bool inputLocked) {
    if (inputLocked) { forward = strafe = 0; wantJump = false; }
    Vec3 input(sinf(cameraYaw) * forward + cosf(cameraYaw) * -strafe, 0,
               cosf(cameraYaw) * forward + sinf(cameraYaw) * strafe);
    float mag = input.length();
    if (mag > 1.0f) input = input * (1.0f / mag);
    if (mag > 0.001f) input = input * (1.0f / input.length());
    crouching = wantCrouch;
    running = wantRun && !crouching && mag > 0.1f;
    float maxSpeed = crouching ? 2.2f : running ? 7.4f : 4.6f;
    float accel = grounded ? 12.0f : 4.0f;
    Vec3 target = input * (maxSpeed * clampf(mag, 0, 1));
    vel.x = damp(vel.x, target.x, accel, dt);
    vel.z = damp(vel.z, target.z, accel, dt);

    if (wantJump && grounded && jumpCooldown <= 0) {
      vel.y = 7.0f;
      grounded = false;
      jumpCooldown = 0.28f;
      audioPlay("jump", 0.55f);
    }
    jumpCooldown = fmaxf(0, jumpCooldown - dt);

    bool inWater = world.inWater(Vec3{pos.x, pos.y + 0.9f, pos.z});
    inWaterNow = inWater;
    float g = inWater ? 4.0f : 22.0f;
    vel.y -= g * dt;
    if (inWater) vel.y = fmaxf(vel.y, -3.0f);

    Vec3 next = pos + vel * dt;
    float half = terrain.halfSize() - 4.0f;
    next.x = clampf(next.x, -half, half);
    next.z = clampf(next.z, -half, half);
    if (world.collides(Vec3{next.x, next.y, next.z}, 0.45f)) {
      next.x = pos.x;
      next.z = pos.z;
      vel.x *= 0.2f;
      vel.z *= 0.2f;
    }
    float ground = terrain.heightAt(next.x, next.z);
    if (next.y <= ground + 0.02f) {
      if (!grounded && vel.y < -4.0f) {
        landingPulse = clampf(-vel.y / 9.0f, 0.2f, 1.0f);
        audioPlay("land", 0.5f);
      }
      next.y = ground;
      vel.y = 0;
      grounded = true;
    } else {
      grounded = false;
    }
    pos = next;
    if (inWater && pos.y < world.waterLevel() - 0.4f) pos.y = world.waterLevel() - 0.4f;

    if (mag > 0.1f) {
      desiredHeading = atan2f(input.x, input.z);
      heading = desiredHeading;
    }
    float speed = vel.lengthXZ();
    animSpeed = speed;
    if (grounded && speed > 0.1f) {
      animPhase += dt * (running ? 11.0f : 7.5f);
      static float lastStep = -1;
      float step = floorf(animPhase / PI);
      if (step != lastStep) {
        lastStep = step;
        const char* surface = "grass";
        if (world.spec().waterEnabled && pos.y < world.waterLevel() + 0.6f) surface = "sand";
        char name[40];
        snprintf(name, sizeof name, "footstep_%s_%d", surface, 1 + (int)(hashf((unsigned)(animPhase * 31)) * 3.9f));
        audioPlay(name, running ? 0.5f : 0.3f);
      }
    }
    landingPulse = fmaxf(0, landingPulse - dt * 3.0f);
  }

  // simple capsule used for camera collision
  Vec3 eyePos() const { return Vec3(pos.x, pos.y + (crouching ? 1.05f : 1.55f), pos.z); }
};

// ---------------------------------------------------------------------------
// Third-person camera with terrain avoidance
// ---------------------------------------------------------------------------
class ThirdPersonCamera {
 public:
  Vec3 position{0, 4, 8};
  Vec3 target{0, 1.6f, 0};
  float yaw = 0;
  float pitch = 0.22f;
  float distance = 5.2f;
  float shake = 0;

  void update(float dt, const Vec3& focus, const Terrain& terrain, float inputDx, float inputDy, bool running) {
    yaw -= inputDx;
    pitch = clampf(pitch + inputDy, -0.35f, 1.15f);
    distance = damp(distance, running ? 5.9f : 5.2f, 6.0f, dt);
    Vec3 dir(sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch));
    Vec3 desired = focus + dir * distance + Vec3(0, 1.5f, 0);
    // keep the camera above terrain
    float ground = terrain.heightAt(desired.x, desired.z) + 0.55f;
    if (desired.y < ground) desired.y = ground;
    shake = fmaxf(0, shake - dt * 2.0f);
    float s = shake * 0.12f;
    position = lerp(position, desired + Vec3(sinf(shake * 61.0f) * s, cosf(shake * 47.0f) * s, 0), clampf(dt * 12.0f, 0, 1));
    target = lerp(target, focus + Vec3(0, 1.35f, 0), clampf(dt * 16.0f, 0, 1));
  }
  Mat4 view() const { return Mat4::lookAt(position, target, Vec3{0, 1, 0}); }
  Vec3 forward() const { return (target - position).normalized(); }
};

void Appearance::applyJson(JsonValue* appearance) {
  if (!appearance) return;
  struct Field { const char* name; Color* target; };
  Field fields[] = {{"skin", &skin}, {"hair", &hair}, {"shirt", &shirt}, {"pants", &pants}, {"shoes", &shoes}};
  for (auto& f : fields) {
    JsonValue* v = Json::get(appearance, f.name);
    if (!v) continue;
    const char* hex = Json::string(v, "");
    if (strlen(hex) == 7 && hex[0] == '#') {
      auto hx = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return (unsigned)(c - '0');
        if (c >= 'a' && c <= 'f') return (unsigned)(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return (unsigned)(c - 'A' + 10);
        return 0;
      };
      f.target->r = (hx(hex[1]) * 16 + hx(hex[2])) / 255.0f;
      f.target->g = (hx(hex[3]) * 16 + hx(hex[4])) / 255.0f;
      f.target->b = (hx(hex[5]) * 16 + hx(hex[6])) / 255.0f;
    }
  }
  bodyType = Json::integer(Json::get(appearance, "bodyType"), 0);
}

// ---------------------------------------------------------------------------
// app:: bridge
// ---------------------------------------------------------------------------
namespace app {

void gameplayCreate(void** outPlayer, void** outCamera) {
  *outPlayer = new Character();
  *outCamera = new ThirdPersonCamera();
}
void gameplayDestroy(void* player, void* camera) {
  delete (Character*)player;
  delete (ThirdPersonCamera*)camera;
}
void gameplaySpawn(void* player, const Vec3& p, float face) { ((Character*)player)->spawn(p, face); }
void gameplayUpdate(void* player, float dt, float fwd, float strafe, bool run, bool crouch, bool jump, float yaw,
                    const Terrain& terrain, GameWorld& world, bool locked) {
  ((Character*)player)->update(dt, fwd, strafe, run, crouch, jump, yaw, terrain, world, locked);
}
Vec3 gameplayEye(void* player) { return ((Character*)player)->eyePos(); }
void gameplayAvatarState(void* player, AvatarState* out) {
  Character* c = (Character*)player;
  out->pos = c->pos;
  out->heading = c->heading;
  out->animPhase = c->animPhase;
  out->animSpeed = c->animSpeed;
  out->grounded = c->grounded;
  out->crouching = c->crouching;
  out->landingPulse = c->landingPulse;
  out->look = c->look;
}
void gameplayCameraUpdate(void* cam, float dt, const Vec3& focus, const Terrain& terrain, float dx, float dy,
                          bool running) {
  ((ThirdPersonCamera*)cam)->update(dt, focus, terrain, dx, dy, running);
}
Mat4 gameplayCameraView(void* cam) { return ((ThirdPersonCamera*)cam)->view(); }
Vec3 gameplayCameraForward(void* cam) { return ((ThirdPersonCamera*)cam)->forward(); }
Vec3 gameplayCameraPosition(void* cam) { return ((ThirdPersonCamera*)cam)->position; }

}  // namespace app

}  // namespace nx
