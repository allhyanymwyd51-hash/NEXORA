// NEXORA UI kit additions used by the app screens (built on core/widgets.cpp).
#pragma once
#include "core/ui.h"

namespace nx {
namespace ui {

// Card surface with optional accent bar; returns its height.
float card(const Rect& r, bool elevated = true, const Color& accent = Color(0, 0, 0, 0));

// Stat pill: icon + value + caption.
void statPill(const Rect& r, const char* iconName, const char* value, const char* caption, const Color& tint);

// Empty state with an illustration glyph, title, body and a CTA button.
// Returns true when the CTA was tapped.
bool emptyState(const Rect& area, const char* iconName, const char* title, const char* body, const char* ctaId,
                const char* ctaLabel);

// Horizontal progress bar with a label/value row.
void progressRow(const Rect& r, const char* label, float t, const Color& tint);

// Small tag/label chip drawn inline (not interactive).
void tag(const Rect& r, const char* label, const Color& c);

// Wrapped body copy inside a width; returns the height used.
float textBlockBody(const char* body, float x, float y, float w, float size, const Color& c);

// Text input wrapper that draws a floating label and a focus underline.
bool labeledField(const char* id, const Rect& r, const char* label, char* buffer, size_t cap, const char* placeholder,
                  bool password = false, int maxLen = 120);

// Wide list row: leading icon tile, title, subtitle, trailing chevron. Returns tap state.
struct RowResult {
  bool clicked = false;
  bool switchedOn = false;
};
RowResult row(const char* id, const Rect& r, const char* iconName, const char* title, const char* subtitle,
              bool chevron = true, const Color& tint = Color(0, 0, 0, 0));

// Soft gradient masks so scrolling content does not collide with the chrome.
void scrollFadeTop(const Rect& r, bool visible);
void scrollFadeBottom(const Rect& r, bool visible);

}  // namespace ui

// Escape user text before it goes into a JSON body. Quotes/backslashes/newlines are
// what actually break a request; everything below 0x20 is dropped. Arabic is UTF-8
// and passes through untouched.
inline void escapeJson(const char* in, char* out, size_t cap) {
  if (!cap) return;
  size_t o = 0;
  for (const char* p = in; p && *p && o + 8 < cap; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
    else if (c == '\n' || c == '\r' || c == '\t') { out[o++] = ' '; }
    else if (c < 0x20) continue;
    else out[o++] = (char)c;
  }
  out[o] = 0;
}

}  // namespace nx
