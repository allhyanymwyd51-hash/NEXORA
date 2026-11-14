// Login + registration: brand hero, credentials, validation feedback, terms consent.
#include "core/audio.h"
#include "screens.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

char gUsername[40] = {0};
char gEmail[96] = {0};
char gPassword[72] = {0};
char gConfirm[72] = {0};
char gBirth[16] = {0};
bool gTerms = false;
bool gShowPassword = false;
float gShake = 0;
int gErrorCode = 0;      // 1 invalid credentials, 2 username taken, 3 email taken, 4 weak password, 5 terms
char gErrorMsg[180] = {0};
float gAnim = 0;
char gServerField[96] = {0};
bool gServerOpen = false;   // first-run players need to point the client at their server

// 18+ gate is enforced server side; the client only collects the date.
bool birthLooksValid(const char* s) {
  if (strlen(s) != 10 || s[4] != '-' || s[7] != '-') return false;
  for (int i = 0; i < 10; i++) if (i != 4 && i != 7 && (s[i] < '0' || s[i] > '9')) return false;
  int y = atoi(s), m = atoi(s + 5), d = atoi(s + 8);
  return y >= 1900 && m >= 1 && m <= 12 && d >= 1 && d <= 31;
}

void setError(int code, const char* msg) {
  gErrorCode = code;
  snprintf(gErrorMsg, sizeof gErrorMsg, "%s", msg ? msg : "");
  gShake = 1.0f;
  audioPlay("ui_error", 0.6f);
}

void clearError() {
  gErrorCode = 0;
  gErrorMsg[0] = 0;
}

// Brand lockup: the NEXORA monogram (two risers + light beam), no borrowed marks.
void brandMark(const Rect& r) {
  float s = fminf(r.w, r.h);
  (void)s;
  ui::glow(Rect(r.x - r.w * 0.3f, r.y - r.h * 0.3f, r.w * 1.6f, r.h * 1.6f), ui::theme.violet.withA(0.30f), 1.0f);
  ui::panel(r, r.w * 0.26f, false);
  ui::rect(r, Color(0.10f, 0.09f, 0.26f, 0.95f), r.w * 0.26f);
  ui::rectStroke(r, ui::theme.violet.withA(0.5f), 1.2f, r.w * 0.26f);
  ui::image(r.inset(r.w * 0.18f), ui::theme.cyan, 1.0f);
}

void heroHeader(float w, float top) {
  float markSize = ui::dp(66);
  brandMark(Rect(w * 0.5f - markSize * 0.5f, top + ui::dp(18), markSize, markSize));
  ui::text("NEXORA", 0, top + ui::dp(96), 30, ui::theme.text, w, 1, 2);
  ui::text(screens::tr("منصة عالم اجتماعي ثلاثي الأبعاد", "A 3D social world platform"), 0, top + ui::dp(136), 12.5f,
           ui::theme.textDim, w, 1, 1);
}

float fieldBlock(float x, float y, float w, const char* label, const char* id, char* buf, size_t cap,
                 const char* placeholder, bool password = false, int maxLen = 120) {
  ui::text(label, x, y, 11.5f, ui::theme.textDim, w, 0, 1);
  Rect r(x, y + ui::dp(19), w, ui::dp(48));
  ui::rect(r, Color(1, 1, 1, 0.045f), ui::dp(13));
  ui::rectStroke(r, ui::focusedId()[0] && strcmp(ui::focusedId(), id) == 0 ? ui::theme.violet.withA(0.7f)
                                                                          : ui::theme.strokeSoft,
                 1.0f, ui::dp(13));
  ui::textField(id, r, buf, cap, placeholder, password, maxLen);
  return y + ui::dp(75);
}

}  // namespace

// ---------------------------------------------------------------------------
// login
// ---------------------------------------------------------------------------
namespace login {

void enter() {
  clearError();
  gPassword[0] = 0;
}

void leave() {}

void draw(float dt) {
  gAnim += dt;
  Engine& e = Engine::get();
  float w = e.width();
  float top = e.safeTop();
  bool wide = w > e.height() * 1.15f;
  float colW = fminf(ui::dp(360), w - ui::dp(48));
  float x = (w - colW) * 0.5f;

  screens::background(dt);
  heroHeader(wide ? w : w, top);

  float y = top + ui::dp(172);
  if (gShake > 0.01f) {
    x += sinf(gAnim * 60.0f) * ui::dp(6) * gShake;
    gShake = fmaxf(0, gShake - dt * 3.0f);
  }

  y = fieldBlock(x, y, colW, screens::tr("اسم المستخدم أو البريد", "Username or email"), "login_id", gUsername,
                 sizeof gUsername, screens::tr("مثال: nexora_player", "e.g. nexora_player"), false, 64);
  y = fieldBlock(x, y, colW, screens::tr("كلمة المرور", "Password"), "login_pw", gPassword, sizeof gPassword,
                 screens::tr("••••••••", "••••••••"), true, 64);

  if (gErrorCode) {
    ui::text(gErrorMsg, x, y - ui::dp(2), 11.5f, ui::theme.red, colW, 0, 1);
    y += ui::dp(20);
  }

  Rect submit(x, y + ui::dp(6), colW, ui::dp(50));
  if (ui::button("login_submit", submit, screens::tr("تسجيل الدخول", "Sign in"), true, !app::busy(), 15).clicked) {
    if (!gUsername[0] || !gPassword[0]) {
      setError(1, screens::tr("أدخل اسم المستخدم وكلمة المرور", "Enter your username and password"));
    } else {
      clearError();
      app::setBusy(true, screens::tr("جاري التحقق…", "Verifying…"));
      char body[320];
      snprintf(body, sizeof body, "{\"login\":\"%s\",\"password\":\"%s\"}", gUsername, gPassword);
      api.call(NET_LOGIN, "POST", "/api/auth/login", body);
    }
  }
  y = submit.y + submit.h + ui::dp(14);

  Rect registerBtn(x, y, colW, ui::dp(48));
  if (ui::button("login_register", registerBtn, screens::tr("إنشاء حساب جديد", "Create a new account"), false,
                 !app::busy(), 14, "plus")
          .clicked) {
    app::go(Screen::Register);
  }

  // language switch doubles as an accessibility affordance on first run
  Rect lang(w * 0.5f - ui::dp(58), e.height() - e.safeBottom() - ui::dp(44), ui::dp(116), ui::dp(32));
  if (ui::button("login_lang", lang, S.language == 0 ? "English" : "العربية", false, true, 12).clicked) {
    S.language = S.language == 0 ? 1 : 0;
    saveLocalSettings();
  }

  // ---- server address: the client ships pointed at the emulator loopback, so a
  // real device needs a way to enter its server without rebuilding the APK.
  float sy = registerBtn.y + registerBtn.h + ui::dp(10);
  Rect chip(x, sy, colW, ui::dp(38));
  char chipLabel[128];
  snprintf(chipLabel, sizeof chipLabel, "%s: %s", screens::tr("الخادم", "Server"),
           S.server[0] ? S.server : screens::tr("افتراضي (محاكي)", "default (emulator)"));
  if (ui::button("login_server", chip, chipLabel, false, true, 11.5f, "gear").clicked) {
    gServerOpen = !gServerOpen;
    if (gServerOpen && !gServerField[0]) snprintf(gServerField, sizeof gServerField, "%s", S.server);
  }
  if (gServerOpen) {
    float fy = chip.y + chip.h + ui::dp(8);
    Rect field(x, fy, colW, ui::dp(44));
    ui::textField("login_server_field", field, gServerField, sizeof gServerField,
                  screens::tr("عنوان الخادم، مثال 192.168.1.20:8080", "Server address, e.g. 192.168.1.20:8080"), false, 90);
    Rect save(x, fy + ui::dp(50), colW * 0.48f, ui::dp(38));
    Rect def(x + colW * 0.52f, fy + ui::dp(50), colW * 0.48f, ui::dp(38));
    if (ui::button("login_server_save", save, screens::tr("استخدم هذا الخادم", "Use this server"), true,
                   gServerField[0] != 0, 11.5f).clicked) {
      app::setServerAddress(gServerField);
      gServerOpen = false;
      ui::toast(screens::tr("تم حفظ عنوان الخادم", "Server address saved"), 2.0f);
    }
    if (ui::button("login_server_default", def, screens::tr("الافتراضي", "Reset"), false, true, 11.5f).clicked) {
      app::setServerAddress("");
      gServerField[0] = 0;
      gServerOpen = false;
    }
  }
}

}  // namespace login

// ---------------------------------------------------------------------------
// register
// ---------------------------------------------------------------------------
namespace registerScreen {

void enter() {
  clearError();
  gPassword[0] = 0;
  gConfirm[0] = 0;
  gTerms = false;
}

void leave() {}

void draw(float dt) {
  gAnim += dt;
  Engine& e = Engine::get();
  float w = e.width();
  float top = e.safeTop();
  float colW = fminf(ui::dp(360), w - ui::dp(48));
  float x = (w - colW) * 0.5f;

  screens::background(dt);
  HeaderOpts ho;
  ho.title = screens::tr("حساب جديد", "New account");
  ho.wallet = false;
  ui::ScrollView scroll;
  float headerH = ui::dp(70);
  scroll.viewH = e.height() - headerH;
  scroll.contentH = ui::dp(590);
  Rect view(0, headerH, w, scroll.viewH);
  scroll.update(view, dt, true);
  Rect content = scroll.content(view);
  screens::header(ho);

  float y = content.y + ui::dp(12);
  if (gShake > 0.01f) {
    x += sinf(gAnim * 60.0f) * ui::dp(5) * gShake;
    gShake = fmaxf(0, gShake - dt * 3.0f);
  }
  y = fieldBlock(x, y, colW, screens::tr("اسم المستخدم (3–20 حرفاً)", "Username (3–20 chars)"), "reg_user", gUsername,
                 sizeof gUsername, "nexora_player", false, 20);
  y = fieldBlock(x, y, colW, screens::tr("البريد الإلكتروني", "Email"), "reg_email", gEmail, sizeof gEmail,
                 "you@example.com", false, 90);
  y = fieldBlock(x, y, colW, screens::tr("كلمة المرور (8 أحرف أو أكثر)", "Password (8+ characters)"), "reg_pw", gPassword,
                 sizeof gPassword, "••••••••", !gShowPassword, 64);
  Rect eye(x + colW - ui::dp(40), y - ui::dp(72), ui::dp(32), ui::dp(32));
  if (ui::iconButton("reg_eye", eye, gShowPassword ? "eye" : "lock", ui::theme.textMuted).clicked) {
    gShowPassword = !gShowPassword;
  }
  y = fieldBlock(x, y, colW, screens::tr("تأكيد كلمة المرور", "Confirm password"), "reg_pw2", gConfirm, sizeof gConfirm,
                 "••••••••", true, 64);
  y = fieldBlock(x, y, colW, screens::tr("تاريخ الميلاد (YYYY-MM-DD)", "Date of birth (YYYY-MM-DD)"), "reg_birth", gBirth,
                 sizeof gBirth, "2000-01-01", false, 10);

  Rect terms(x, y, colW, ui::dp(52));
  ui::checkbox("reg_terms", Rect(x, y + ui::dp(4), ui::dp(22), ui::dp(22)), gTerms, "");
  ui::text(screens::tr("أوافق على الشروط وسياسة الخصوصية وأؤكد أن عمري 13 عاماً أو أكثر",
              "I accept the Terms and Privacy Policy and confirm I am 13 or older"),
           x + ui::dp(32), y + ui::dp(4), 11.5f, ui::theme.textDim, colW - ui::dp(34), 0, 1);
  y += ui::dp(58);

  if (gErrorCode) {
    ui::text(gErrorMsg, x, y - ui::dp(4), 11.5f, ui::theme.red, colW, 0, 1);
    y += ui::dp(18);
  }

  Rect submit(x, y, colW, ui::dp(50));
  if (ui::button("reg_submit", submit, screens::tr("إنشاء الحساب", "Create account"), true, !app::busy(), 15).clicked) {
    if (strlen(gUsername) < 3) setError(2, screens::tr("اسم المستخدم قصير جداً", "Username is too short"));
    else if (!strchr(gEmail, '@')) setError(3, screens::tr("البريد الإلكتروني غير صحيح", "Email is invalid"));
    else if (strlen(gPassword) < 8) setError(4, screens::tr("كلمة المرور قصيرة جداً", "Password is too short"));
    else if (strcmp(gPassword, gConfirm) != 0) setError(4, screens::tr("كلمتا المرور غير متطابقتين", "Passwords do not match"));
    else if (!birthLooksValid(gBirth)) setError(1, screens::tr("تاريخ الميلاد غير صحيح", "Date of birth is invalid"));
    else if (!gTerms) setError(5, screens::tr("يجب الموافقة على الشروط", "You must accept the terms"));
    else {
      clearError();
      app::setBusy(true, screens::tr("جاري إنشاء الحساب…", "Creating account…"));
      char body[520];
      snprintf(body, sizeof body,
               "{\"username\":\"%s\",\"email\":\"%s\",\"password\":\"%s\",\"confirmPassword\":\"%s\","
               "\"birthDate\":\"%s\",\"acceptedTerms\":true}",
               gUsername, gEmail, gPassword, gConfirm, gBirth);
      api.call(NET_REGISTER, "POST", "/api/auth/register", body);
    }
  }

  ui::text(screens::tr("بإنشاء حساب فأنت تحصل على محفظة Miy تبدأ من صفر", "A new account starts with 0 Miy"),
           x, submit.y + submit.h + ui::dp(12), 10.5f, ui::theme.textMuted, colW, 0, 1);
}

}  // namespace registerScreen

// ---------------------------------------------------------------------------
// API results
// ---------------------------------------------------------------------------
void loginApi(const ApiEvent& ev) {
  if (ev.op != (int)NET_LOGIN) return;
  app::setBusy(false);
  Json j;
  if (!ev.body || !j.parse(ev.body)) {
    setError(1, screens::tr("تعذر الاتصال بالخادم", "Cannot reach the server"));
    return;
  }
  if (ev.status == 200) {
    applyLogin(j.root());
    ui::toast(screens::tr("مرحباً بك في NEXORA", "Welcome to NEXORA"));
    audioPlay("ui_success", 0.7f);
  } else if (ev.status == 401) {
    setError(1, apiMessage(j.root(), screens::tr("بيانات الدخول غير صحيحة", "Invalid credentials")));
  } else if (ev.status == 429) {
    setError(1, screens::tr("محاولات كثيرة، انتظر قليلاً", "Too many attempts, please wait"));
  } else {
    setError(1, apiMessage(j.root(), screens::tr("تعذر تسجيل الدخول", "Sign-in failed")));
  }
}

void registerApi(const ApiEvent& ev) {
  if (ev.op != (int)NET_REGISTER) return;
  app::setBusy(false);
  Json j;
  if (!ev.body || !j.parse(ev.body)) {
    setError(1, screens::tr("تعذر الاتصال بالخادم", "Cannot reach the server"));
    return;
  }
  if (ev.status == 201) {
    applyLogin(j.root());
    ui::toast(screens::tr("تم إنشاء حسابك بنجاح", "Your account is ready"));
    audioPlay("ui_success", 0.7f);
  } else {
    setError(2, apiMessage(j.root(), screens::tr("تعذر إنشاء الحساب", "Could not create the account")));
  }
}

}  // namespace screens
}  // namespace app
}  // namespace nx
