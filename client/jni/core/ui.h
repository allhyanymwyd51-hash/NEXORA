// NEXORA engine — retained-mode UI: theme, 9-slice panels, buttons, inputs, animations.
#pragma once
#include "engine.h"
#include "font.h"
#include "gfx.h"

namespace nx {
namespace ui {

// ---- design tokens (dark futuristic identity: navy + violet + cyan lights) ----
struct Theme {
  Color bg0 = Color::hex(0x050814);
  Color bg1 = Color::hex(0x0A1024);
  Color panel = Color::hex(0x121A33);
  Color panelHi = Color::hex(0x18223F);
  Color panelPressed = Color::hex(0x0E1529);
  Color stroke = Color::hex(0x2A3557);
  Color strokeSoft = Color::hex(0x1C2440);
  Color violet = Color::hex(0x7C4DFF);
  Color violetDim = Color::hex(0x5A34C9);
  Color cyan = Color::hex(0x38E1FF);
  Color gold = Color::hex(0xE8C15A);
  Color green = Color::hex(0x4EE39B);
  Color red = Color::hex(0xFF5F6D);
  Color text = Color::hex(0xEEF2FF);
  Color textDim = Color::hex(0x9AA6C8);
  Color textMuted = Color::hex(0x66739A);
  float radius = 16.0f;
  float radiusSm = 10.0f;
  float pad = 16.0f;
};
extern Theme theme;

// ---- drawing helpers (all sizes in layout pixels; get DPI-scaled automatically) ----
float dp(float v);           // density independent -> pixels
float dpInv(float px);       // pixels -> dp
void begin();                // starts a UI frame (resets input capture)
void end();

void rect(const Rect& r, const Color& c, float radius = 0);
void rectStroke(const Rect& r, const Color& c, float thickness = 1, float radius = 0);
void verticalGradient(const Rect& r, const Color& top, const Color& bottom);
void glow(const Rect& r, const Color& c, float intensity = 1.0f);
void panel(const Rect& r, float radius = -1, bool elevated = true);
void shadow(const Rect& r, float radius = 12.0f, float strength = 0.5f);
void icon(const char* name, const Rect& r, const Color& c);
void image(const Rect& r, const Color& tint, float alpha = 1.0f);  // logo/icon texture
void avatar(const Rect& r, const char* avatarBase64, const char* fallbackName, int seed);
void divider(float x0, float x1, float y, const Color& c);

// text: size in dp, align 0=left 1=center 2=right; returns drawn height
float text(const char* utf8, float x, float y, float sizeDp, const Color& c, float maxWidth = 0, int align = 0,
           int weight = 0);
float textMeasure(const char* utf8, float sizeDp, float maxWidth = 0, int weight = 0);
void textBlock(const char* utf8, const Rect& r, float sizeDp, const Color& c, int align = 0, int weight = 0,
               int maxLines = 99);

// ---- animation helpers ----
float animTowards(float& state, float target, float speed, float dt);
float alphaFor(float t);  // eased 0..1

// ---- input ----
struct Pointer {
  bool down = false;
  bool pressed = false;   // went down this frame
  bool released = false;  // went up this frame
  Vec2 pos;
  Vec2 start;
  double downTime = 0;
};
void feedPointer(const Pointer& p);
Pointer& pointer();               // current frame's logical pointer

// ---- widgets ----
struct ButtonResult {
  bool clicked = false, held = false, pressedDown = false;
};
ButtonResult button(const char* id, const Rect& r, const char* label, bool primary = false, bool enabled = true,
                    float fontSize = 15.0f, const char* iconName = nullptr);
ButtonResult iconButton(const char* id, const Rect& r, const char* iconName, const Color& tint = theme.text,
                        bool dot = false);
bool toggle(const char* id, const Rect& r, bool value);
float slider(const char* id, const Rect& r, float value, float minV = 0, float maxV = 1);
// text field: returns true when submitted (enter). Edits `buffer` in place.
bool textField(const char* id, const Rect& r, char* buffer, size_t cap, const char* placeholder, bool password = false,
               int maxLen = 120);
bool checkbox(const char* id, const Rect& r, bool& value, const char* label);
// segmented control: returns index of the newly selected segment or -1
int segments(const char* id, const Rect& r, const char** labels, int count, int selected, float fontSize = 13.0f);
// chip row (horizontal, wraps)
Rect chip(const char* id, Rect cursor, const char* label, bool selected, bool* clicked);

// scroll helper: clamps and eases content offset with drag/fling
struct ScrollView {
  float offset = 0;
  float velocity = 0;
  bool dragging = false;
  float lastY = 0;
  float contentH = 0;
  float viewH = 0;
  void update(const Rect& view, float dt, bool allowDrag);
  Rect content(const Rect& view) const;
};
bool isCapturing();
extern Pointer gPointer;
extern bool gCaptured;
extern char gFocusId[48];
extern char gPendingText[512];
extern bool gPendingBackspace;
extern float gCaretBlink;

// Internal per-frame layers (flush order): solid shapes, glyphs, then additive glow.
void flushText();
void flushGlow();
// Text input plumbing from the platform IME/keyboard.
void pushEditableText(const char* utf8);
void pushBackspace();
const char* focusedId();
void setFocus(const char* id);

// toast queue
void toast(const char* msg, float seconds = 3.0f);
void drawToasts(float dt);

}  // namespace ui
}  // namespace nx
