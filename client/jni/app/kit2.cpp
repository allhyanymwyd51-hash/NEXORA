// NEXORA app UI kit: cards, stat pills, empty states, labeled fields, list rows.
#include "core/audio.h"
#include "widgets.h"

namespace nx {
namespace ui {

float card(const Rect& r, bool elevated, const Color& accent) {
  if (elevated) shadow(r, dp(14), 0.35f);
  rect(r, theme.panel, dp(theme.radius));
  rectStroke(r, Color(1, 1, 1, 0.055f), 1.0f, dp(theme.radius));
  // subtle top highlight: premium surface without a heavy border
  verticalGradient(Rect(r.x + 1, r.y + 1, r.w - 2, fminf(r.h * 0.4f, dp(37))),
                   Color(1, 1, 1, 0.045f), Color(1, 1, 1, 0.0f));
  if (accent.a > 0.01f) {
    rect(Rect(r.x, r.y + dp(14), dp(3), r.h - dp(28)), accent, dp(2));
  }
  return r.h;
}

void statPill(const Rect& r, const char* iconName, const char* value, const char* caption, const Color& tint) {
  rect(r, Color(1, 1, 1, 0.035f), dp(theme.radiusSm));
  float ix = r.x + dp(10);
  icon(iconName, Rect(ix, r.y + dp(11), dp(16), dp(16)), tint);
  text(value, ix + dp(22), r.y + dp(8), 14, theme.text, r.w - dp(34), 0, 2);
  text(caption, r.x + dp(10), r.y + dp(28), 10, theme.textMuted, r.w - dp(20), 0, 1);
}

bool emptyState(const Rect& area, const char* iconName, const char* title, const char* body, const char* ctaId,
                const char* ctaLabel) {
  float cx = area.x + area.w * 0.5f;
  float cy = area.y + area.h * 0.42f;
  // illustration: concentric rings with the glyph at the centre
  float R = dp(52);
  glow(Rect(cx - R * 1.6f, cy - R * 1.6f, R * 3.2f, R * 3.2f), theme.violet.withA(0.16f), 1.0f);
  rectStroke(Rect(cx - R, cy - R, R * 2, R * 2), Color(1, 1, 1, 0.06f), 1.0f, R);
  rectStroke(Rect(cx - R * 0.72f, cy - R * 0.72f, R * 1.44f, R * 1.44f), Color(1, 1, 1, 0.09f), 1.0f, R * 0.72f);
  rect(Rect(cx - R * 0.5f, cy - R * 0.5f, R, R), Color(0.13f, 0.10f, 0.30f, 0.9f), R * 0.5f);
  icon(iconName, Rect(cx - dp(16), cy - dp(16), dp(32), dp(32)), theme.cyan);

  float ty = cy + R + dp(28);
  text(title, area.x, ty, 17, theme.text, area.w, 1, 2);
  ty += dp(26);
  float bh = textBlockBody(body, area.x + dp(28), ty, area.w - dp(56), 12.5f, theme.textDim);
  ty += bh + dp(22);

  bool clicked = false;
  if (ctaLabel) {
    float bw = fminf(area.w - dp(64), dp(220));
    Rect btn(cx - bw * 0.5f, ty, bw, dp(46));
    clicked = button(ctaId, btn, ctaLabel, true, true, 14).clicked;
  }
  return clicked;
}

float textBlockBody(const char* body, float x, float y, float w, float size, const Color& c) {
  Engine& e = Engine::get();
  (void)e;
  return text(body, x, y, size, c, w, 1, 1);
}

void progressRow(const Rect& r, const char* label, float t, const Color& tint) {
  text(label, r.x, r.y, 11.5f, theme.textDim, r.w - dp(42), 0, 1);
  char pct[16];
  snprintf(pct, sizeof pct, "%d%%", (int)(clampf(t, 0, 1) * 100.0f + 0.5f));
  text(pct, r.x + r.w - dp(42), r.y, 11.5f, tint, dp(42), 2, 2);
  Rect bar(r.x, r.y + dp(19), r.w, dp(5));
  rect(bar, Color(1, 1, 1, 0.08f), dp(2.5f));
  if (t > 0.001f) rect(Rect(bar.x, bar.y, bar.w * clampf(t, 0, 1), bar.h), tint, dp(2.5f));
}

void tag(const Rect& r, const char* label, const Color& c) {
  rect(r, c.withA(0.14f), r.h * 0.5f);
  rectStroke(r, c.withA(0.32f), 1.0f, r.h * 0.5f);
  text(label, r.x, r.y + r.h * 0.24f, 10.5f, c, r.w, 1, 1);
}

bool labeledField(const char* id, const Rect& r, const char* label, char* buffer, size_t cap, const char* placeholder,
                  bool password, int maxLen) {
  (void)label;
  return textField(id, r, buffer, cap, placeholder, password, maxLen);
}

RowResult row(const char* id, const Rect& r, const char* iconName, const char* title, const char* subtitle,
              bool chevron, const Color& tint) {
  RowResult out;
  Pointer& p = pointer();
  bool hover = r.contains(p.pos.x, p.pos.y);
  static char pressedId[48] = {0};
  if (hover && p.pressed) snprintf(pressedId, sizeof pressedId, "%s", id);
  if (hover && p.released && p.start.distance(p.pos) < dp(24)) {
    if (strcmp(pressedId, id) == 0) {
      out.clicked = true;
      audioPlay("ui_click", 0.4f);
    }
  }
  Color bg = hover ? Color(1, 1, 1, 0.045f) : Color(1, 1, 1, 0.02f);
  rect(r, bg, dp(theme.radiusSm));
  if (iconName && *iconName) {
    Rect tile(r.x + dp(10), r.y + (r.h - dp(34)) * 0.5f, dp(34), dp(34));
    Color base = tint.a > 0.01f ? tint : theme.violet;
    rect(tile, base.withA(0.16f), dp(10));
    icon(iconName, tile.inset(dp(8)), base);
  }
  float tx = iconName && *iconName ? r.x + dp(54) : r.x + dp(14);
  float tw = r.w - (tx - r.x) - (chevron ? dp(34) : dp(14));
  if (subtitle && *subtitle) {
    text(title, tx, r.y + dp(10), 13.5f, theme.text, tw, 0, 2);
    text(subtitle, tx, r.y + dp(29), 11, theme.textMuted, tw, 0, 1);
  } else {
    text(title, tx, r.y + (r.h - dp(19)) * 0.5f, 13.5f, theme.text, tw, 0, 2);
  }
  if (chevron) {
    icon("chevron_left", Rect(r.x + r.w - dp(28), r.y + (r.h - dp(16)) * 0.5f, dp(16), dp(16)), theme.textMuted);
  }
  return out;
}

void scrollFadeTop(const Rect& r, bool visible) {
  if (!visible) return;
  verticalGradient(Rect(r.x, r.y, r.w, dp(26)), Color(0.04f, 0.05f, 0.11f, 0.95f), Color(0.04f, 0.05f, 0.11f, 0.0f));
}

void scrollFadeBottom(const Rect& r, bool visible) {
  if (!visible) return;
  verticalGradient(Rect(r.x, r.y + r.h - dp(26), r.w, dp(26)), Color(0.04f, 0.05f, 0.11f, 0.0f),
                   Color(0.04f, 0.05f, 0.11f, 0.95f));
}

}  // namespace ui
}  // namespace nx
