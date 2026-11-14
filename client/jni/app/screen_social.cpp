// Friends + chat: requests, presence, DMs and in-map invites.
#include "core/audio.h"
#include "game/gameplay.h"
#include "screens.h"
#include "widgets.h"
#include "ws.h"

namespace nx {
namespace app {
namespace screens {

namespace {

struct Person {
  char id[40];
  char username[64];
  bool online;
  char friendshipId[40];
  char last[160];
  int lastAt;
};

Array<Person> gFriends, gIncoming, gOutgoing, gThreads, gSearch;
bool gLoaded = false;
char gSearchQuery[48] = {0};
int gTab = 0;
ui::ScrollView gScroll;

// chat screen state
char gChatWith[40] = {0};
char gChatName[64] = {0};
char gMessage[400] = {0};
struct Msg {
  char body[400];
  char sender[64];
  int at;
  bool mine;
};
Array<Msg> gMessages;
bool gMessagesLoaded = false;
ui::ScrollView gChatScroll;

void parsePeople(JsonValue* arr, Array<Person>& out, bool incoming) {
  for (int i = 0; i < Json::length(arr); i++) {
    JsonValue* p = Json::at(arr, i);
    Person e{};
    snprintf(e.id, sizeof e.id, "%s", Json::string(Json::get(p, "id")));
    snprintf(e.username, sizeof e.username, "%s", Json::string(Json::get(p, "username")));
    snprintf(e.friendshipId, sizeof e.friendshipId, "%s", Json::string(Json::get(p, "friendshipId")));
    e.online = Json::boolean(Json::get(p, "online"));
    out.push(e);
  }
  (void)incoming;
}

void reloadFriends() {
  api.call(NET_FRIENDS, "GET", "/api/friends", nullptr, 71);
}

}  // namespace

// ===========================================================================
// friends
// ===========================================================================
namespace friends {

void enter() {
  gLoaded = false;
  gTab = 0;
  gScroll.offset = 0;
  reloadFriends();
}

void leave() {}

void draw(float dt) {
  Engine& e = Engine::get();
  float w = e.width();
  float pad = ui::dp(16);
  float innerW = w - pad * 2;
  background(dt);

  HeaderOpts ho;
  ho.title = tr("الأصدقاء", "Friends");
  ho.backButton = false;
  header(ho);
  float top = ui::dp(74);
  float navH = ui::dp(72) + e.safeBottom();

  // search / add
  Rect search(pad, top, innerW, ui::dp(42));
  ui::rect(search, Color(1, 1, 1, 0.05f), ui::dp(13));
  ui::rectStroke(search, ui::theme.strokeSoft, 1.0f, ui::dp(13));
  ui::icon("search", Rect(search.x + ui::dp(10), search.y + ui::dp(11), ui::dp(19), ui::dp(19)),
           ui::theme.textMuted);
  ui::textField("fr_search", Rect(search.x + ui::dp(36), search.y + ui::dp(4), search.w - ui::dp(46), ui::dp(34)),
                gSearchQuery, sizeof gSearchQuery, tr("ابحث باسم المستخدم…", "Search by username…"), false, 32);
  if (ui::iconButton("fr_search_go", Rect(search.x + search.w - ui::dp(38), search.y + ui::dp(6), ui::dp(30),
                                          ui::dp(30)),
                     "chevron_left", ui::theme.textDim)
          .clicked &&
      gSearchQuery[0]) {
    char path[128];
    snprintf(path, sizeof path, "/api/users/search?q=%s", gSearchQuery);
    api.call(NET_USER_SEARCH, "GET", path, nullptr, 72);
  }

  float tabsY = search.y + search.h + ui::dp(10);
  char t0[32], t1[32], t2[32];
  snprintf(t0, sizeof t0, "%s %d", tr("أصدقاء", "Friends"), gFriends.count);
  snprintf(t1, sizeof t1, "%s %d", tr("طلبات", "Requests"), gIncoming.count);
  snprintf(t2, sizeof t2, "%s %d", tr("نتائج", "Results"), gSearch.count);
  const char* labels[3] = {t0, t1, t2};
  int picked = ui::segments("fr_tabs", Rect(pad, tabsY, innerW, ui::dp(34)), labels, 3, gTab, 11);
  if (picked >= 0) gTab = picked;

  Rect view(0, tabsY + ui::dp(44), w, e.height() - tabsY - ui::dp(44) - navH);
  Array<Person>& list = gTab == 0 ? gFriends : gTab == 1 ? gIncoming : gSearch;
  float rowH = ui::dp(66);
  gScroll.contentH = list.count == 0 ? view.h : list.count * (rowH + ui::dp(6)) + ui::dp(20);
  gScroll.update(view, dt, true);
  Rect content = gScroll.content(view);

  if (!gLoaded && gTab == 0) {
    for (int i = 0; i < 3; i++) {
      ui::rect(Rect(pad, content.y + i * (rowH + ui::dp(6)), innerW, rowH), Color(1, 1, 1, 0.035f), ui::dp(14));
    }
  } else if (list.count == 0) {
    if (gTab == 1) {
      ui::emptyState(Rect(0, view.y, w, view.h), "users", tr("لا طلبات صداقة", "No friend requests"),
                     tr("عندما يرسل لك أحد طلباً سيظهر هنا.", "Requests you receive will appear here."), nullptr,
                     nullptr);
    } else if (gTab == 2) {
      ui::emptyState(Rect(0, view.y, w, view.h), "search", tr("ابحث عن لاعبين", "Find players"),
                     tr("اكتب اسم المستخدم في الأعلى وأرسل طلب صداقة.",
                        "Type a username above and send a friend request."),
                     nullptr, nullptr);
    } else {
      ui::emptyState(Rect(0, view.y, w, view.h), "users", tr("لا أصدقاء بعد", "No friends yet"),
                     tr("أضف أصدقاءك ليدخلوا معك نفس الماب، وتسمعوا بعضكم بالصوت.",
                        "Add friends to join your maps and talk with real-time voice."),
                     "friends_find", tr("ابحث عن لاعبين", "Find players"));
    }
  } else {
    for (int i = 0; i < list.count; i++) {
      Person& p = list[i];
      Rect r(pad, content.y + i * (rowH + ui::dp(6)), innerW, rowH);
      if (r.y + r.h < view.y - ui::dp(30) || r.y > view.y + view.h + ui::dp(30)) continue;
      ui::card(r, false);
      // avatar: deterministic initial tile
      Rect av(r.x + ui::dp(10), r.y + ui::dp(13), ui::dp(40), ui::dp(40));
      ui::rect(av, ui::theme.violet.withA(0.24f), ui::dp(20));
      char initial[2] = {p.username[0] ? p.username[0] : '?', 0};
      ui::text(initial, av.x, av.y + ui::dp(10), 15, ui::theme.text, av.w, 1, 2);
      if (p.online) ui::rect(Rect(av.x + av.w - ui::dp(11), av.y + av.h - ui::dp(11), ui::dp(11), ui::dp(11)),
                             ui::theme.green, ui::dp(6));
      ui::text(p.username, av.x + av.w + ui::dp(10), r.y + ui::dp(12), 13, ui::theme.text,
               r.w - ui::dp(150), 0, 2);
      ui::text(p.online ? tr("متصل الآن", "Online") : tr("غير متصل", "Offline"), av.x + av.w + ui::dp(10),
               r.y + ui::dp(34), 10.5f, p.online ? ui::theme.green : ui::theme.textMuted, r.w - ui::dp(150), 0, 1);

      if (gTab == 0) {
        Rect chat(r.x + r.w - ui::dp(84), r.y + ui::dp(15), ui::dp(34), ui::dp(34));
        if (ui::iconButton("fr_chat", chat, "chat").clicked) {
          snprintf(gChatWith, sizeof gChatWith, "%s", p.id);
          snprintf(gChatName, sizeof gChatName, "%s", p.username);
          gMessagesLoaded = false;
          gMessages.clear();
          go(Screen::Chat);
        }
        Rect invite(r.x + r.w - ui::dp(44), r.y + ui::dp(15), ui::dp(34), ui::dp(34));
        if (ui::iconButton("fr_invite", invite, "play").clicked) {
          if (game::Gameplay::get().active()) {
            ws::inviteFriend(p.id, game::Gameplay::get().mapId());
            ui::toast(tr("تم إرسال الدعوة", "Invite sent"));
          } else {
            ui::toast(tr("ادخل ماباً أولاً لدعوة صديق", "Join a map first to invite"));
          }
        }
      } else if (gTab == 1) {
        Rect acc(r.x + r.w - ui::dp(120), r.y + ui::dp(15), ui::dp(52), ui::dp(34));
        if (ui::button("fr_acc", acc, tr("قبول", "Accept"), true, true, 11).clicked) {
          char path[96];
          snprintf(path, sizeof path, "/api/friends/%s/accept", p.friendshipId);
          api.call(NET_FRIEND_ACCEPT, "POST", path, "{}", 74);
        }
        Rect dec(r.x + r.w - ui::dp(62), r.y + ui::dp(15), ui::dp(52), ui::dp(34));
        if (ui::button("fr_dec", dec, tr("رفض", "Decline"), false, true, 11).clicked) {
          char path[96];
          snprintf(path, sizeof path, "/api/friends/%s/decline", p.friendshipId);
          api.call(NET_FRIEND_DECLINE, "POST", path, "{}", 75);
        }
      } else {
        Rect add(r.x + r.w - ui::dp(110), r.y + ui::dp(15), ui::dp(100), ui::dp(34));
        if (ui::button("fr_add", add, tr("إضافة", "Add"), true, true, 11).clicked) {
          char body[96];
          snprintf(body, sizeof body, "{\"userId\":\"%s\"}", p.id);
          api.call(NET_FRIEND_REQUEST, "POST", "/api/friends/request", body, 73);
        }
      }
    }
  }

  bottomNav(3);
}

}  // namespace friends

// ===========================================================================
// chat
// ===========================================================================
namespace chat {

void enter() {
  if (!gChatWith[0] || gMessagesLoaded) return;
  // the DM channel id is the two user ids sorted and joined with ':' (server rule);
  // /chat/dm/:userId/read returns that canonical id plus the latest 50 messages
  char path[128];
  snprintf(path, sizeof path, "/api/chat/dm/%s/read", gChatWith);
  api.call(NET_CHAT_HISTORY, "POST", path, "{}", 82);
}

void leave() { ui::setFocus(""); }

void draw(float dt) {
  Engine& e = Engine::get();
  float w = e.width();
  float pad = ui::dp(14);
  float innerW = w - pad * 2;
  background(dt);

  HeaderOpts ho;
  ho.title = gChatName[0] ? gChatName : tr("الرسائل", "Messages");
  ho.wallet = false;
  header(ho);

  float top = ui::dp(74);
  float composerH = ui::dp(58) + e.safeBottom();
  Rect view(0, top, w, e.height() - top - composerH);

  if (!gChatWith[0]) {
    // thread list
    ui::ScrollView s;
    s.viewH = view.h;
    s.contentH = gThreads.count * ui::dp(70) + ui::dp(20);
    s.update(view, dt, true);
    Rect content = s.content(view);
    if (gThreads.count == 0) {
      ui::emptyState(Rect(0, view.y, w, view.h), "chat", tr("لا محادثات", "No conversations"),
                     tr("افتح ملف صديق وابدأ محادثة، أو تحدث داخل الماب مباشرة.",
                        "Open a friend and start chatting, or talk inside a map."),
                     "chat_friends", tr("الأصدقاء", "Friends"));
    }
    for (int i = 0; i < gThreads.count; i++) {
      Person& p = gThreads[i];
      Rect r(pad, content.y + i * ui::dp(70), innerW, ui::dp(64));
      ui::card(r, false);
      Rect av(r.x + ui::dp(10), r.y + ui::dp(12), ui::dp(40), ui::dp(40));
      ui::rect(av, ui::theme.cyan.withA(0.2f), ui::dp(20));
      ui::text(p.username, av.x + av.w + ui::dp(10), r.y + ui::dp(10), 13, ui::theme.text, r.w - ui::dp(120), 0, 2);
      ui::text(p.last, av.x + av.w + ui::dp(10), r.y + ui::dp(32), 10.5f, ui::theme.textMuted, r.w - ui::dp(120), 0,
               1);
      ui::Pointer& ptr = ui::pointer();
      if (r.contains(ptr.pos.x, ptr.pos.y) && ptr.released && ptr.start.distance(ptr.pos) < ui::dp(20)) {
        snprintf(gChatWith, sizeof gChatWith, "%s", p.id);
        snprintf(gChatName, sizeof gChatName, "%s", p.username);
        gMessages.clear();
        gMessagesLoaded = false;
        chat::enter();
      }
    }
    if (ui::emptyState(Rect(w - ui::dp(120), view.y + view.h - ui::dp(10), ui::dp(1), ui::dp(1)), "chat", "", "",
                       nullptr, nullptr)) {
    }
    return;
  }

  // thread
  gChatScroll.viewH = view.h;
  gChatScroll.contentH = gMessages.count * ui::dp(58) + ui::dp(20);
  gChatScroll.update(view, dt, true);
  Rect content = gChatScroll.content(view);
  for (int i = 0; i < gMessages.count; i++) {
    Msg& m = gMessages[i];
    bool mine = m.mine;
    float maxBubble = innerW * 0.74f;
    float thick = ui::textMeasure(m.body, 12, maxBubble - ui::dp(24), 1);
    (void)thick;
    Rect bubble;
    bubble.h = ui::dp(42);
    bubble.w = fminf(maxBubble, ui::textMeasure(m.body, 12, 0, 1) + ui::dp(24));
    if (bubble.w < ui::dp(70)) bubble.w = ui::dp(70);
    bubble.x = mine ? pad + innerW - bubble.w : pad;
    bubble.y = content.y + i * ui::dp(58);
    ui::rect(bubble, mine ? ui::theme.violet.withA(0.85f) : ui::theme.panel, ui::dp(14));
    ui::text(m.body, bubble.x + ui::dp(12), bubble.y + ui::dp(9), 12, ui::theme.text, bubble.w - ui::dp(20),
             mine ? 2 : 0, 1);
    if (!mine) ui::text(m.sender, bubble.x + ui::dp(4), bubble.y + ui::dp(44), 9.5f, ui::theme.textMuted, 0, 0, 1);
  }
  if (gMessages.count == 0 && gMessagesLoaded) {
    ui::text(tr("ابدأ المحادثة", "Say hello"), 0, view.y + ui::dp(40), 12, ui::theme.textMuted, w, 1, 1);
  }

  // composer
  Rect bar(pad, e.height() - composerH + ui::dp(6), innerW - ui::dp(56), ui::dp(44));
  ui::rect(bar, Color(1, 1, 1, 0.05f), ui::dp(22));
  ui::rectStroke(bar, ui::theme.strokeSoft, 1.0f, ui::dp(22));
  ui::textField("chat_msg", bar.inset(ui::dp(10)), gMessage, sizeof gMessage, tr("اكتب رسالة…", "Message…"), false,
                300);
  Rect send(pad + innerW - ui::dp(50), bar.y, ui::dp(50), ui::dp(44));
  if (ui::iconButton("chat_send", send, "chevron_left", ui::theme.violet).clicked && gMessage[0]) {
    ws::sendChat(gMessage, gChatWith);
    Msg m{};
    snprintf(m.body, sizeof m.body, "%s", gMessage);
    snprintf(m.sender, sizeof m.sender, "%s", S.username);
    m.mine = true;
    gMessages.push(m);
    gMessage[0] = 0;
    audioPlay("ui_click", 0.4f);
  }
}

}  // namespace chat

// ===========================================================================
// API results
// ===========================================================================
void friendsApi(const ApiEvent& ev) {
  if (ev.op == (int)NET_FRIENDS) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    gFriends.clear();
    gIncoming.clear();
    gOutgoing.clear();
    parsePeople(Json::get(j.root(), "friends"), gFriends, false);
    parsePeople(Json::get(j.root(), "incoming"), gIncoming, true);
    parsePeople(Json::get(j.root(), "outgoing"), gOutgoing, false);
    gLoaded = true;
    S.friends = gFriends.count;
    return;
  }
  if (ev.op == (int)NET_USER_SEARCH) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    gSearch.clear();
    parsePeople(Json::get(j.root(), "users"), gSearch, false);
    gTab = 2;
    return;
  }
  if (ev.op == (int)NET_FRIEND_REQUEST || ev.op == (int)NET_FRIEND_ACCEPT || ev.op == (int)NET_FRIEND_DECLINE) {
    Json j;
    if (ev.body) j.parse(ev.body);
    if (ev.status == 200 || ev.status == 201) {
      ui::toast(ev.op == (int)NET_FRIEND_REQUEST ? tr("تم إرسال الطلب", "Request sent")
                 : ev.op == (int)NET_FRIEND_ACCEPT ? tr("تم قبول الصداقة", "Friend added")
                                                   : tr("تم رفض الطلب", "Request declined"));
      audioPlay("ui_success", 0.6f);
      reloadFriends();
    } else if (ev.body) {
      toastError(j.root(), tr("تعذر تنفيذ الطلب", "Action failed"));
    }
    return;
  }
  if (ev.op == (int)NET_CHAT_THREADS || ev.op == (int)NET_CHAT_HISTORY) {
    Json j;
    if (!ev.body || !j.parse(ev.body)) return;
    JsonValue* threads = Json::get(j.root(), "threads");
    if (threads) {
      gThreads.clear();
      for (int i = 0; i < Json::length(threads); i++) {
        JsonValue* t = Json::at(threads, i);
        JsonValue* u = Json::get(t, "user");
        Person p{};
        snprintf(p.id, sizeof p.id, "%s", Json::string(Json::get(u, "id")));
        snprintf(p.username, sizeof p.username, "%s", Json::string(Json::get(u, "username")));
        snprintf(p.last, sizeof p.last, "%s", Json::string(Json::get(t, "lastMessage")));
        p.online = Json::boolean(Json::get(u, "online"));
        gThreads.push(p);
      }
    }
    JsonValue* messages = Json::get(j.root(), "messages");
    if (messages) {
      gMessages.clear();
      for (int i = 0; i < Json::length(messages); i++) {
        JsonValue* m = Json::at(messages, i);
        JsonValue* sender = Json::get(m, "sender");
        Msg msg{};
        snprintf(msg.body, sizeof msg.body, "%s", Json::string(Json::get(m, "body")));
        snprintf(msg.sender, sizeof msg.sender, "%s", Json::string(Json::get(sender, "username")));
        const char* senderId = Json::string(Json::get(sender, "id"));
        msg.mine = (senderId[0] && S.uid[0] && strcmp(senderId, S.uid) == 0) ||
                   strcmp(Json::string(Json::get(sender, "username")), S.username) == 0;
        gMessages.push(msg);
      }
      gMessagesLoaded = true;
    }
    return;
  }
}

void chatApi(const ApiEvent& ev) { friendsApi(ev); }
void chatText(const char*) {}
bool chatBack() { return false; }

}  // namespace screens
}  // namespace app
}  // namespace nx
