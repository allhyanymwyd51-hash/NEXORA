#include "ui.h"
#include "audio.h"

namespace nx {
namespace ui {

// persistent per-widget animation state (hover/press/selection easing)
struct WidgetState {
  char id[48];
  float press = 0;      // 0..1
  float focus = 0;      // 0..1
  float toggle = 0;     // 0..1
  float value = 0;
  bool init = false;
};
static const int kMaxWidgets = 160;
static WidgetState gWidgets[kMaxWidgets];
static int gWidgetCount = 0;

static WidgetState* ws(const char* id) {
  for (int i = 0; i < gWidgetCount; i++) if (strcmp(gWidgets[i].id, id) == 0) return &gWidgets[i];
  if (gWidgetCount >= kMaxWidgets) {
    // recycle oldest
    for (int i = 0; i < kMaxWidgets - 1; i++) gWidgets[i] = gWidgets[i + 1];
    gWidgetCount = kMaxWidgets - 1;
  }
  WidgetState& s = gWidgets[gWidgetCount++];
  strncpy(s.id, id, sizeof s.id - 1);
  return &s;
}

void feedPointer(const Pointer& p) { gPointer = p; }
bool isCapturing() { return gCaptured; }

float animTowards(float& state, float target, float speed, float dt) {
  state = damp(state, target, speed, dt);
  return state;
}
float alphaFor(float t) { return clampf(t, 0, 1); }

// ---------------------------------------------------------------------------
// button
// ---------------------------------------------------------------------------
ButtonResult button(const char* id, const Rect& r, const char* label, bool primary, bool enabled, float fontSize,
                    const char* iconName) {
  ButtonResult out;
  WidgetState* st = ws(id);
  bool hover = r.contains(gPointer.pos.x, gPointer.pos.y);
  bool active = enabled && hover && gPointer.down;
  if (enabled && hover && gPointer.pressed) {
    out.pressedDown = true;
    gCaptured = true;
  }
  // A tap counts only when press and release happen on the same widget without dragging.
  if (enabled && hover && gPointer.released && gPointer.start.distance(gPointer.pos) < dp(24)) {
    out.clicked = true;
    audioPlay("ui_click", 0.5f);
  } else if (enabled && hover && gPointer.released && gPointer.start.distance(gPointer.pos) >= dp(24)) {
    // released after a drag: not a tap, but the widget still swallows the gesture
    gCaptured = false;
  }
  (void)active;
  float pressTarget = (enabled && gPointer.down && hover) ? 1.0f : 0.0f;
  animTowards(st->press, pressTarget, 22.0f, 1.0f / 60.0f);
  if (!enabled) st->press = 0;

  float radius = dp(theme.radiusSm * 1.2f);
  Color base;
  if (primary) base = Color::lerp(theme.violet, theme.violetDim, st->press * 0.6f);
  else base = Color::lerp(theme.panelHi, theme.panelPressed, st->press);
  if (!enabled) base = Color::lerp(theme.panel, theme.bg1, 0.5f);

  if (primary && enabled) {
    glow(Rect(r.x - dp(6), r.y - dp(6), r.w + dp(12), r.h + dp(12)), theme.violet.withA(0.45f), 0.9f);
  }
  rect(r, base, radius);
  if (primary) {
    verticalGradient(Rect(r.x + 1, r.y + 1, r.w - 2, r.h * 0.55f), Color(1, 1, 1, 0.16f), Color(1, 1, 1, 0.0f));
  } else {
    rectStroke(r, Color::lerp(theme.stroke, theme.violet, st->press * 0.5f), 1.0f, radius);
  }
  Color labelColor = enabled ? theme.text : theme.textMuted;
  float size = fontSize;
  float tw = textMeasure(label, size, 0, primary ? 2 : 1);
  float iconW = iconName ? dp(18) + dp(8) : 0;
  float totalW = tw + iconW;
  float startX = r.x + (r.w - totalW) * 0.5f;
  if (iconName) {
    Rect ir(startX, r.y + (r.h - dp(18)) * 0.5f, dp(18), dp(18));
    icon(iconName, ir, labelColor);
    startX += iconW;
  }
  text(label, startX, r.y + (r.h - dp(size) * 1.35f) * 0.5f, size, labelColor, 0, 0, primary ? 2 : 1);
  return out;
}

ButtonResult iconButton(const char* id, const Rect& r, const char* iconName, const Color& tint, bool dot) {
  ButtonResult out;
  WidgetState* st = ws(id);
  bool hover = r.contains(gPointer.pos.x, gPointer.pos.y);
  if (hover && gPointer.released && gPointer.start.distance(gPointer.pos) < dp(24)) {
    out.clicked = true;
    audioPlay("ui_click", 0.45f);
  }
  if (hover && gPointer.pressed) gCaptured = true;
  float pressTarget = (gPointer.down && hover) ? 1.0f : 0.0f;
  animTowards(st->press, pressTarget, 22.0f, 1.0f / 60.0f);
  float radius = r.h * 0.5f;
  Color bg = Color::lerp(theme.panelHi.withA(0.9f), theme.panelPressed, st->press);
  rect(r, bg, fminf(radius, dp(14)));
  rectStroke(r, theme.strokeSoft, 1.0f, fminf(radius, dp(14)));
  float inset = r.w * 0.28f;
  icon(iconName, Rect(r.x + inset, r.y + inset, r.w - inset * 2, r.h - inset * 2), tint);
  if (dot) {
    float d = dp(7);
    rect(Rect(r.x + r.w - d * 1.4f, r.y + d * 0.4f, d, d), theme.red, d * 0.5f);
  }
  return out;
}

// ---------------------------------------------------------------------------
// toggle
// ---------------------------------------------------------------------------
bool toggle(const char* id, const Rect& r, bool value) {
  WidgetState* st = ws(id);
  bool changed = false;
  bool hover = r.contains(gPointer.pos.x, gPointer.pos.y);
  if (hover && gPointer.released && gPointer.start.distance(gPointer.pos) < dp(24)) {
    changed = true;
    gCaptured = true;
    audioPlay("ui_click", 0.4f);
  }
  animTowards(st->toggle, value ? 1.0f : 0.0f, 18.0f, 1.0f / 60.0f);
  float h = r.h;
  Color track = Color::lerp(theme.stroke, theme.violet, st->toggle);
  rect(r, track, h * 0.5f);
  float knob = h - dp(6);
  float x = r.x + dp(3) + (r.w - knob - dp(6)) * st->toggle;
  rect(Rect(x, r.y + dp(3), knob, knob), theme.text, knob * 0.5f);
  return changed ? !value : value;
}

// ---------------------------------------------------------------------------
// slider
// ---------------------------------------------------------------------------
float slider(const char* id, const Rect& r, float value, float minV, float maxV) {
  WidgetState* st = ws(id);
  float trackH = dp(6);
  Rect track(r.x, r.y + (r.h - trackH) * 0.5f, r.w, trackH);
  rect(track, theme.stroke, trackH * 0.5f);
  float t = (value - minV) / (maxV - minV);
  t = clampf(t, 0, 1);
  rect(Rect(track.x, track.y, track.w * t, trackH), theme.violet, trackH * 0.5f);
  float knobR = dp(11);
  float knobX = track.x + track.w * t;
  glow(Rect(knobX - knobR * 2, track.center().y - knobR * 2, knobR * 4, knobR * 4), theme.violet.withA(0.5f), 1.0f);
  rect(Rect(knobX - knobR, track.center().y - knobR, knobR * 2, knobR * 2), theme.text, knobR);
  if (r.contains(gPointer.pos.x, gPointer.pos.y) && gPointer.down) {
    gCaptured = true;
    float nt = clampf((gPointer.pos.x - track.x) / fmaxf(1, track.w), 0, 1);
    return minV + (maxV - minV) * nt;
  }
  animTowards(st->value, value, 20.0f, 1.0f / 60.0f);
  return value;
}

// ---------------------------------------------------------------------------
// text field
// ---------------------------------------------------------------------------
static float gCaret = 0;
static double gLastInput = 0;

bool textField(const char* id, const Rect& r, char* buffer, size_t cap, const char* placeholder, bool password,
               int maxLen) {
  bool submitted = false;
  bool focused = strcmp(gFocusId, id) == 0;
  bool hover = r.contains(gPointer.pos.x, gPointer.pos.y);
  if (gPointer.pressed && hover) {
    strncpy(gFocusId, id, sizeof gFocusId - 1);
    focused = true;
    gCaptured = true;
    gCaret = 0;
  } else if (gPointer.pressed && !hover && focused) {
    gFocusId[0] = 0;
    focused = false;
  }
  // apply queued edits only to the focused field
  if (focused) {
    if (gPendingText[0]) {
      size_t len = strlen(buffer);
      for (const char* p = gPendingText; *p && len + 1 < cap && (int)len < maxLen;) {
        buffer[len++] = *p++;
      }
      buffer[len] = 0;
      gPendingText[0] = 0;
      gLastInput = Engine::get().time();
    }
    if (gPendingBackspace) {
      size_t len = strlen(buffer);
      if (len > 0) {
        len--;
        while (len > 0 && ((unsigned char)buffer[len] & 0xC0) == 0x80) len--;
        buffer[len] = 0;
      }
      gPendingBackspace = false;
      gLastInput = Engine::get().time();
    }
  } else {
    gPendingText[0] = 0;
    gPendingBackspace = false;
  }

  float radius = dp(theme.radiusSm);
  rect(r, focused ? theme.panelHi : theme.panel, radius);
  rectStroke(r, focused ? theme.violet : theme.strokeSoft, focused ? 1.5f : 1.0f, radius);

  char display[256];
  size_t n = 0;
  for (size_t i = 0; buffer[i] && n < sizeof display - 1; i++) {
    if (password) display[n++] = (buffer[i] & 0xC0) == 0x80 ? 0 : '*';
    else display[n++] = buffer[i];
  }
  display[n] = 0;
  // limit display so the caret stays visible
  float fs = dp(15);
  bool rtl = Engine::get().font(0).isRtlText(buffer[0] ? buffer : (placeholder ? placeholder : ""));
  float textW = textMeasure(display, 15, 0, 0);
  while (textW > r.w - dp(28) && n > 1) {
    size_t cut = 0;
    while (display[cut]) cut++;
    cut--;
    while (cut > 0 && ((unsigned char)display[cut] & 0xC0) == 0x80) cut--;
    display[cut] = 0;
    textW = textMeasure(display, 15, 0, 0);
  }
  float tx = rtl ? r.x + r.w - dp(14) - textW : r.x + dp(14);
  if (buffer[0] == 0) {
    text(placeholder, r.x + dp(14), r.y + (r.h - fs * 1.3f) * 0.5f, 15, theme.textMuted, 0, 0, 0);
  } else {
    text(display, tx, r.y + (r.h - fs * 1.3f) * 0.5f, 15, theme.text, 0, 0, 0);
  }
  if (focused) {
    float blink = fractf((float)Engine::get().time() * 1.6f) < 0.55f ? 1.0f : 0.0f;
    float cx = rtl ? tx - dp(3) : tx + textW + dp(2);
    rect(Rect(cx, r.y + r.h * 0.24f, dp(2), r.h * 0.52f), theme.cyan.withA(blink), dp(1));
  }
  return submitted;
}

bool checkbox(const char* id, const Rect& r, bool& value, const char* label) {
  bool changed = false;
  float box = fminf(r.h, dp(22));
  Rect boxR(r.x, r.y + (r.h - box) * 0.5f, box, box);
  bool hover = r.contains(gPointer.pos.x, gPointer.pos.y);
  if (hover && gPointer.released && gPointer.start.distance(gPointer.pos) < dp(24)) {
    value = !value;
    changed = true;
    gCaptured = true;
    audioPlay("ui_click", 0.35f);
  }
  rect(boxR, value ? theme.violet : theme.panelHi, dp(6));
  rectStroke(boxR, value ? theme.violet : theme.stroke, 1.2f, dp(6));
  if (value) icon("check", boxR.inset(dp(3)), theme.text);
  if (label) {
    text(label, boxR.x + box + dp(10), r.y + (r.h - dp(13) * 1.35f) * 0.5f, 13, theme.textDim, r.w - box - dp(14), 0, 0);
  }
  return changed;
}

// ---------------------------------------------------------------------------
// segmented control
// ---------------------------------------------------------------------------
int segments(const char* id, const Rect& r, const char** labels, int count, int selected, float fontSize) {
  int clicked = -1;
  float segW = r.w / (float)count;
  rect(r, theme.panel, dp(theme.radiusSm));
  rectStroke(r, theme.strokeSoft, 1.0f, dp(theme.radiusSm));
  for (int i = 0; i < count; i++) {
    Rect sr(r.x + i * segW, r.y, segW, r.h);
    bool sel = i == selected;
    bool hover = sr.contains(gPointer.pos.x, gPointer.pos.y);
    if (hover && gPointer.released && gPointer.start.distance(gPointer.pos) < dp(24)) clicked = i;
    if (sel) {
      Rect inner = sr.inset(dp(3));
      rect(inner, theme.violet, dp(theme.radiusSm - 4));
      glow(Rect(inner.x - dp(4), inner.y - dp(4), inner.w + dp(8), inner.h + dp(8)), theme.violet.withA(0.35f), 0.8f);
    }
    Color c = sel ? theme.text : (hover ? theme.textDim : theme.textMuted);
    float tw = textMeasure(labels[i], fontSize, 0, sel ? 2 : 1);
    text(labels[i], sr.x + (sr.w - tw) * 0.5f, sr.y + (sr.h - dp(fontSize) * 1.35f) * 0.5f, fontSize, c, 0, 0,
         sel ? 2 : 1);
  }
  return clicked;
}

Rect chip(const char* id, Rect cursor, const char* label, bool selected, bool* clicked) {
  (void)id;
  float pad = dp(14);
  float tw = textMeasure(label, 13, 0, selected ? 2 : 1);
  float w = tw + pad * 2;
  float h = dp(32);
  Rect r(cursor.x, cursor.y, w, h);
  bool hover = r.contains(gPointer.pos.x, gPointer.pos.y);
  rect(r, selected ? theme.violet : theme.panelHi, h * 0.5f);
  if (!selected) rectStroke(r, theme.stroke, 1.0f, h * 0.5f);
  text(label, r.x + pad, r.y + (h - dp(13) * 1.3f) * 0.5f, 13, selected ? theme.text : theme.textDim, 0, 0,
       selected ? 2 : 1);
  if (hover && gPointer.released && gPointer.start.distance(gPointer.pos) < dp(20) && clicked) *clicked = true;
  cursor.x += w + dp(8);
  return cursor;
}

// ---------------------------------------------------------------------------
// scroll view
// ---------------------------------------------------------------------------
void ScrollView::update(const Rect& view, float dt, bool allowDrag) {
  bool hover = view.contains(gPointer.pos.x, gPointer.pos.y);
  if (allowDrag && hover) {
    if (gPointer.pressed) {
      dragging = true;
      lastY = gPointer.pos.y;
      velocity = 0;
    }
    if (dragging && gPointer.down) {
      float dy = gPointer.pos.y - lastY;
      offset += dy;
      velocity = dy / fmaxf(dt, 0.001f);
      lastY = gPointer.pos.y;
      gCaptured = true;
    }
  }
  if (gPointer.released) dragging = false;
  if (!dragging) {
    offset += velocity * dt;
    velocity *= powf(0.02f, dt);  // friction
    if (fabsf(velocity) < 4.0f) velocity = 0;
  }
  float maxOffset = fmaxf(0, contentH - viewH);
  if (offset > 0) { offset = damp(offset, 0, 18, dt); velocity *= 0.5f; }
  if (offset < -maxOffset) { offset = damp(offset, -maxOffset, 18, dt); velocity *= 0.5f; }
}

Rect ScrollView::content(const Rect& view) const { return Rect(view.x, view.y + offset, view.w, contentH); }

// ---------------------------------------------------------------------------
// toasts
// ---------------------------------------------------------------------------
struct Toast {
  char msg[160];
  float life;
};
static Toast gToasts[4];
static int gToastCount = 0;

void toast(const char* msg, float seconds) {
  if (gToastCount >= 4) {
    for (int i = 0; i < 3; i++) gToasts[i] = gToasts[i + 1];
    gToastCount = 3;
  }
  Toast& t = gToasts[gToastCount++];
  strncpy(t.msg, msg, sizeof t.msg - 1);
  t.life = seconds;
}

void drawToasts(float dt) {
  Engine& e = Engine::get();
  float y = e.height() - dp(28);
  for (int i = gToastCount - 1; i >= 0; i--) {
    gToasts[i].life -= dt;
    if (gToasts[i].life <= 0) {
      for (int k = i; k < gToastCount - 1; k++) gToasts[k] = gToasts[k + 1];
      gToastCount--;
      continue;
    }
    float alpha = clampf(gToasts[i].life, 0, 1);
    float tw = textMeasure(gToasts[i].msg, 13, 0, 1);
    float w = fminf(tw + dp(36), e.width() - dp(40));
    Rect r(e.width() * 0.5f - w * 0.5f, y - dp(44), w, dp(40));
    rect(r, theme.bg1.withA(0.95f * alpha), dp(12));
    rectStroke(r, theme.stroke.withA(alpha), 1.0f, dp(12));
    rect(Rect(r.x, r.y + dp(10), dp(3), r.h - dp(20)), theme.cyan.withA(alpha), dp(2));
    text(gToasts[i].msg, r.x + dp(16), r.y + (r.h - dp(13) * 1.3f) * 0.5f, 13, theme.text.withA(alpha), 0, 0, 1);
    y -= dp(48);
  }
}

}  // namespace ui
}  // namespace nx
