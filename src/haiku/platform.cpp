/*
 * Haiku platform layer for the Prince of Persia engine: application, window,
 * presentation of the 320x200 frame, keyboard and mouse input, timers.
 *
 * Threads: the game itself runs on the main thread (the engine's pop_main).
 * A second thread owns the BApplication message loop, and the BWindow has its
 * own looper thread as usual on Haiku. Input arriving on the window thread is
 * translated into engine events and queued; the game thread drains the queue
 * with SDL_PollEvent. The game thread presents frames by locking the window
 * and drawing the frame bitmap into the view.
 */
#include <SDL2/SDL.h>

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Locker.h>
#include <Message.h>
#include <OS.h>
#include <Screen.h>
#include <View.h>
#include <Window.h>

#include <deque>
#include <string>

#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

// The engine opens its data relative to the working directory. Started from
// the Deskbar or Tracker, that is the home folder, so move to the folder that
// holds the game data before main() runs: the executable's own folder when it
// has data/ (installed layout), or its parent (build/ inside the source tree).
bool has_data_dir(const std::string& dir) {
	struct stat st;
	std::string p = dir + "/data";
	return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

__attribute__((constructor))
void enter_game_directory() {
	image_info info;
	int32 cookie = 0;
	while (get_next_image_info(B_CURRENT_TEAM, &cookie, &info) == B_OK) {
		if (info.type != B_APP_IMAGE) continue;
		char resolved[PATH_MAX];
		std::string exe = realpath(info.name, resolved) ? resolved : info.name;
		std::string dir = exe.substr(0, exe.rfind('/'));
		if (dir.empty()) return;
		if (has_data_dir(".")) return;
		if (has_data_dir(dir)) { chdir(dir.c_str()); return; }
		std::string parent = dir.substr(0, dir.rfind('/'));
		if (!parent.empty() && has_data_dir(parent)) chdir(parent.c_str());
		return;
	}
}

const char* kAppSignature = "application/x-vnd.rainygirl-princeofpersia";

// ---- error / hints -----------------------------------------------------------

std::string g_error;
std::string g_scale_quality = "0";
bigtime_t g_start_time = 0;

// ---- event queue -------------------------------------------------------------

BLocker g_event_lock("pop events");
std::deque<SDL_Event> g_events;
Uint8 g_keystate[SDL_NUM_SCANCODES];
int32 g_mouse_x = 0, g_mouse_y = 0;
volatile bool g_window_active = false;
bool g_cursor_hidden = false;   // what the game asked for
bool g_cursor_applied = false;  // what was actually told to the app server

void apply_cursor(bool window_active) {
	bool hide = g_cursor_hidden && window_active;
	if (hide == g_cursor_applied || be_app == NULL) return;
	if (hide) be_app->HideCursor(); else be_app->ShowCursor();
	g_cursor_applied = hide;
}

void push_event(const SDL_Event& ev) {
	g_event_lock.Lock();
	if (g_events.size() < 4096) g_events.push_back(ev);
	g_event_lock.Unlock();
}

void push_window_event(Uint8 which, Sint32 d1 = 0, Sint32 d2 = 0) {
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = SDL_WINDOWEVENT;
	ev.window.event = which;
	ev.window.data1 = d1;
	ev.window.data2 = d2;
	push_event(ev);
}

// ---- keyboard mapping --------------------------------------------------------

// Haiku raw key codes (physical positions, see `keymap -d`) -> engine scancodes
// (USB HID usage ids, as used by the engine's key tables).
const Uint16 kRawToScancode[0x70] = {
	/* 0x00 */ 0, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F1, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5, SDL_SCANCODE_F6,
	/* 0x08 */ SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10, SDL_SCANCODE_F11, SDL_SCANCODE_F12, SDL_SCANCODE_PRINTSCREEN, SDL_SCANCODE_SCROLLLOCK,
	/* 0x10 */ SDL_SCANCODE_PAUSE, SDL_SCANCODE_GRAVE, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6,
	/* 0x18 */ SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0, SDL_SCANCODE_MINUS, SDL_SCANCODE_EQUALS, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_INSERT,
	/* 0x20 */ SDL_SCANCODE_HOME, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_NUMLOCKCLEAR, SDL_SCANCODE_KP_DIVIDE, SDL_SCANCODE_KP_MULTIPLY, SDL_SCANCODE_KP_MINUS, SDL_SCANCODE_TAB, SDL_SCANCODE_Q,
	/* 0x28 */ SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R, SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I, SDL_SCANCODE_O,
	/* 0x30 */ SDL_SCANCODE_P, SDL_SCANCODE_LEFTBRACKET, SDL_SCANCODE_RIGHTBRACKET, SDL_SCANCODE_BACKSLASH, SDL_SCANCODE_DELETE, SDL_SCANCODE_END, SDL_SCANCODE_PAGEDOWN, SDL_SCANCODE_KP_7,
	/* 0x38 */ SDL_SCANCODE_KP_8, SDL_SCANCODE_KP_9, SDL_SCANCODE_KP_PLUS, SDL_SCANCODE_CAPSLOCK, SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
	/* 0x40 */ SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_SEMICOLON, SDL_SCANCODE_APOSTROPHE, SDL_SCANCODE_RETURN,
	/* 0x48 */ SDL_SCANCODE_KP_4, SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6, SDL_SCANCODE_LSHIFT, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V,
	/* 0x50 */ SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M, SDL_SCANCODE_COMMA, SDL_SCANCODE_PERIOD, SDL_SCANCODE_SLASH, SDL_SCANCODE_RSHIFT, SDL_SCANCODE_UP,
	/* 0x58 */ SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_LCTRL, SDL_SCANCODE_LALT, SDL_SCANCODE_SPACE, SDL_SCANCODE_RALT,
	/* 0x60 */ SDL_SCANCODE_RCTRL, SDL_SCANCODE_LEFT, SDL_SCANCODE_DOWN, SDL_SCANCODE_RIGHT, SDL_SCANCODE_KP_0, SDL_SCANCODE_KP_PERIOD, SDL_SCANCODE_LGUI, SDL_SCANCODE_RGUI,
	/* 0x68 */ SDL_SCANCODE_APPLICATION, SDL_SCANCODE_NONUSBACKSLASH, 0, 0, 0, 0, 0, 0,
};

// Raw codes of the modifier keys. Haiku reports these through B_MODIFIERS_CHANGED
// rather than as key down/up messages, so they are tracked from the raw key state
// bitmap that comes with every keyboard message.
const int32 kModifierRawKeys[] = { 0x4b, 0x56, 0x5c, 0x60, 0x5d, 0x5f, 0x66, 0x67, 0x3b, 0x22, 0x0f };

Uint16 raw_to_scancode(int32 raw) {
	if (raw < 0 || raw >= 0x70) return 0;
	return kRawToScancode[raw];
}

Uint16 current_kmod() {
	Uint16 mod = 0;
	if (g_keystate[SDL_SCANCODE_LSHIFT]) mod |= KMOD_LSHIFT;
	if (g_keystate[SDL_SCANCODE_RSHIFT]) mod |= KMOD_RSHIFT;
	if (g_keystate[SDL_SCANCODE_LCTRL]) mod |= KMOD_LCTRL;
	if (g_keystate[SDL_SCANCODE_RCTRL]) mod |= KMOD_RCTRL;
	if (g_keystate[SDL_SCANCODE_LALT]) mod |= KMOD_LALT;
	if (g_keystate[SDL_SCANCODE_RALT]) mod |= KMOD_RALT;
	if (g_keystate[SDL_SCANCODE_LGUI]) mod |= KMOD_LGUI;
	if (g_keystate[SDL_SCANCODE_RGUI]) mod |= KMOD_RGUI;
	return mod;
}

void push_key(Uint16 scancode, bool down, bool repeat) {
	if (scancode == 0) return;
	g_keystate[scancode] = down ? 1 : 0;
	SDL_Event ev;
	memset(&ev, 0, sizeof(ev));
	ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	ev.key.state = down ? 1 : 0;
	ev.key.repeat = repeat ? 1 : 0;
	ev.key.keysym.scancode = (SDL_Scancode)scancode;
	ev.key.keysym.mod = current_kmod();
	push_event(ev);
}

// Bring the modifier keys in line with the raw key state bitmap of a message.
void sync_modifiers(const BMessage* msg) {
	const uint8* states = NULL;
	ssize_t size = 0;
	if (msg->FindData("states", B_UINT8_TYPE, (const void**)&states, &size) == B_OK && states != NULL && size >= 16) {
		for (size_t i = 0; i < sizeof(kModifierRawKeys) / sizeof(kModifierRawKeys[0]); ++i) {
			int32 raw = kModifierRawKeys[i];
			bool down = (states[raw >> 3] & (1 << (7 - (raw & 7)))) != 0;
			Uint16 sc = raw_to_scancode(raw);
			if (sc != 0 && (g_keystate[sc] != 0) != down) push_key(sc, down, false);
		}
		return;
	}
	// Fallback without the bitmap: derive from the modifier roles.
	int32 mods = 0;
	if (msg->FindInt32("modifiers", &mods) != B_OK) return;
	struct { int32 flag; Uint16 sc; } table[] = {
		{ B_LEFT_SHIFT_KEY, SDL_SCANCODE_LSHIFT }, { B_RIGHT_SHIFT_KEY, SDL_SCANCODE_RSHIFT },
		{ B_LEFT_CONTROL_KEY, SDL_SCANCODE_LCTRL }, { B_RIGHT_CONTROL_KEY, SDL_SCANCODE_RCTRL },
		{ B_LEFT_COMMAND_KEY, SDL_SCANCODE_LALT }, { B_RIGHT_COMMAND_KEY, SDL_SCANCODE_RALT },
		{ B_LEFT_OPTION_KEY, SDL_SCANCODE_LGUI }, { B_RIGHT_OPTION_KEY, SDL_SCANCODE_RGUI },
		{ B_CAPS_LOCK, SDL_SCANCODE_CAPSLOCK }, { B_NUM_LOCK, SDL_SCANCODE_NUMLOCKCLEAR }, { B_SCROLL_LOCK, SDL_SCANCODE_SCROLLLOCK },
	};
	for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
		bool down = (mods & table[i].flag) != 0;
		if ((g_keystate[table[i].sc] != 0) != down) push_key(table[i].sc, down, false);
	}
}

void release_all_keys() {
	for (int i = 1; i < SDL_NUM_SCANCODES; ++i) {
		if (g_keystate[i]) push_key(i, false, false);
	}
}

// ---- application -------------------------------------------------------------

sem_id g_app_ready = -1;
thread_id g_app_thread = -1;
volatile bool g_quitting = false;

class PoPApp : public BApplication {
public:
	PoPApp() : BApplication(kAppSignature) {}

	virtual void ReadyToRun() {
		release_sem(g_app_ready);
	}

	virtual bool QuitRequested() {
		if (g_quitting) return true;
		// Asked to quit from the Deskbar or at shutdown: let the game close itself.
		SDL_Event ev;
		memset(&ev, 0, sizeof(ev));
		ev.type = SDL_QUIT;
		push_event(ev);
		return false;
	}
};

int32 app_thread_entry(void*) {
	PoPApp* app = new PoPApp();
	app->Run();
	delete app;
	return 0;
}

void ensure_app() {
	if (g_app_thread >= 0) return;
	g_app_ready = create_sem(0, "pop app ready");
	g_app_thread = spawn_thread(app_thread_entry, "pop application", B_NORMAL_PRIORITY, NULL);
	resume_thread(g_app_thread);
	acquire_sem(g_app_ready);
}

// ---- window ------------------------------------------------------------------

class GameWindow;

class GameView : public BView {
public:
	GameView(BRect frame)
		: BView(frame, "game", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS)
	{
		SetViewColor(B_TRANSPARENT_COLOR);
	}

	virtual void Draw(BRect updateRect);
	virtual void FrameResized(float w, float h);

	virtual void MouseDown(BPoint where) {
		int32 buttons = 0;
		if (BMessage* msg = Window()->CurrentMessage()) msg->FindInt32("buttons", &buttons);
		g_mouse_x = (int32)where.x;
		g_mouse_y = (int32)where.y;
		SDL_Event ev;
		memset(&ev, 0, sizeof(ev));
		ev.type = SDL_MOUSEBUTTONDOWN;
		ev.button.state = 1;
		ev.button.clicks = 1;
		ev.button.x = g_mouse_x;
		ev.button.y = g_mouse_y;
		if (buttons & B_PRIMARY_MOUSE_BUTTON) ev.button.button = SDL_BUTTON_LEFT;
		else if (buttons & B_SECONDARY_MOUSE_BUTTON) ev.button.button = SDL_BUTTON_RIGHT;
		else if (buttons & B_TERTIARY_MOUSE_BUTTON) ev.button.button = SDL_BUTTON_MIDDLE;
		else ev.button.button = SDL_BUTTON_X1;
		push_event(ev);
	}

	virtual void MouseMoved(BPoint where, uint32, const BMessage*) {
		g_mouse_x = (int32)where.x;
		g_mouse_y = (int32)where.y;
	}
};

struct FrameLayout {
	int out_w, out_h;      // view size in pixels
	int x, y, w, h;        // where the frame lands inside the view
	float scale;
};

class GameWindow : public BWindow {
public:
	GameWindow(BRect frame, const char* title)
		: BWindow(frame, title, B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, B_ASYNCHRONOUS_CONTROLS),
		  fView(NULL), fBitmap(NULL), fSmooth(false), fFullscreen(false)
	{
		fView = new GameView(Bounds());
		AddChild(fView);
		fView->MakeFocus(true);
		fWidth = (int32)Bounds().Width() + 1;
		fHeight = (int32)Bounds().Height() + 1;
		memset(&fLayout, 0, sizeof(fLayout));
		SetSizeLimits(160, 16384, 100, 16384);
	}

	virtual void DispatchMessage(BMessage* msg, BHandler* handler) {
		switch (msg->what) {
			case B_KEY_DOWN:
			case B_UNMAPPED_KEY_DOWN:
			case B_KEY_UP:
			case B_UNMAPPED_KEY_UP:
				HandleKey(msg);
				return; // swallow: no window shortcuts, no view KeyDown
			case B_MODIFIERS_CHANGED:
				sync_modifiers(msg);
				return;
			case B_MOUSE_WHEEL_CHANGED: {
				float dy = 0, dx = 0;
				msg->FindFloat("be:wheel_delta_y", &dy);
				msg->FindFloat("be:wheel_delta_x", &dx);
				SDL_Event ev;
				memset(&ev, 0, sizeof(ev));
				ev.type = SDL_MOUSEWHEEL;
				ev.wheel.x = (Sint32)dx;
				ev.wheel.y = (Sint32)-dy; // engine expects "up" to be positive
				push_event(ev);
				return;
			}
			default:
				break;
		}
		BWindow::DispatchMessage(msg, handler);
	}

	virtual bool QuitRequested() {
		if (g_quitting) return true;
		SDL_Event ev;
		memset(&ev, 0, sizeof(ev));
		ev.type = SDL_QUIT;
		push_event(ev);
		return false;
	}

	virtual void WindowActivated(bool active) {
		g_window_active = active;
		apply_cursor(active);
		if (active) {
			push_window_event(SDL_WINDOWEVENT_FOCUS_GAINED);
		} else {
			// Keys released while another window has the focus would never be
			// seen; treat losing the focus as releasing everything.
			release_all_keys();
			push_window_event(SDL_WINDOWEVENT_FOCUS_LOST);
		}
	}

	void ViewResized(int32 w, int32 h) {
		fWidth = w;
		fHeight = h;
		push_window_event(SDL_WINDOWEVENT_SIZE_CHANGED, w, h);
	}

	void GetSize(int* w, int* h) const {
		if (w) *w = fWidth;
		if (h) *h = fHeight;
	}

	// Called with the window locked.
	void Present(BBitmap* bitmap, bool smooth, const FrameLayout& layout) {
		fBitmap = bitmap;
		fSmooth = smooth;
		fLayout = layout;
		Paint(fView->Bounds());
		fView->Flush();
	}

	// Called with the window locked.
	void Paint(BRect update) {
		fView->SetHighColor(0, 0, 0);
		if (fBitmap == NULL) {
			fView->FillRect(update);
			return;
		}
		const FrameLayout& l = fLayout;
		BRect dest(l.x, l.y, l.x + l.w - 1, l.y + l.h - 1);
		BRect bounds = fView->Bounds();
		// Black bars around the frame.
		if (dest.top > bounds.top) fView->FillRect(BRect(bounds.left, bounds.top, bounds.right, dest.top - 1));
		if (dest.bottom < bounds.bottom) fView->FillRect(BRect(bounds.left, dest.bottom + 1, bounds.right, bounds.bottom));
		if (dest.left > bounds.left) fView->FillRect(BRect(bounds.left, dest.top, dest.left - 1, dest.bottom));
		if (dest.right < bounds.right) fView->FillRect(BRect(dest.right + 1, dest.top, bounds.right, dest.bottom));
		fView->DrawBitmapAsync(fBitmap, fBitmap->Bounds(), dest, fSmooth ? B_FILTER_BITMAP_BILINEAR : 0);
	}

	void SetFullscreen(bool on) {
		if (on == fFullscreen) return;
		fFullscreen = on;
		if (on) {
			fSavedFrame = Frame();
			BScreen screen(this);
			BRect sf = screen.Frame();
			SetLook(B_NO_BORDER_WINDOW_LOOK);
			MoveTo(sf.left, sf.top);
			ResizeTo(sf.Width(), sf.Height());
		} else {
			SetLook(B_TITLED_WINDOW_LOOK);
			MoveTo(fSavedFrame.left, fSavedFrame.top);
			ResizeTo(fSavedFrame.Width(), fSavedFrame.Height());
		}
	}

	bool IsFullscreen() const { return fFullscreen; }

private:
	void HandleKey(BMessage* msg) {
		sync_modifiers(msg);
		int32 raw = 0;
		if (msg->FindInt32("key", &raw) != B_OK) return;
		Uint16 sc = raw_to_scancode(raw);
		bool down = (msg->what == B_KEY_DOWN || msg->what == B_UNMAPPED_KEY_DOWN);
		int32 repeat = 0;
		msg->FindInt32("be:key_repeat", &repeat);
		push_key(sc, down, repeat > 0);
		if (down && msg->what == B_KEY_DOWN) {
			const char* bytes = NULL;
			if (msg->FindString("bytes", &bytes) == B_OK && bytes != NULL) {
				unsigned char c = (unsigned char)bytes[0];
				Uint16 mod = current_kmod();
				if (c >= 0x20 && c != 0x7f && !(mod & (KMOD_CTRL | KMOD_ALT))) {
					SDL_Event ev;
					memset(&ev, 0, sizeof(ev));
					ev.type = SDL_TEXTINPUT;
					strncpy(ev.text.text, bytes, SDL_TEXTINPUTEVENT_TEXT_SIZE - 1);
					push_event(ev);
				}
			}
		}
	}

	GameView* fView;
	BBitmap* fBitmap;
	bool fSmooth;
	FrameLayout fLayout;
	int32 fWidth, fHeight;
	bool fFullscreen;
	BRect fSavedFrame;
};

void GameView::Draw(BRect updateRect) {
	static_cast<GameWindow*>(Window())->Paint(updateRect);
	push_window_event(SDL_WINDOWEVENT_EXPOSED);
}

void GameView::FrameResized(float w, float h) {
	static_cast<GameWindow*>(Window())->ViewResized((int32)w + 1, (int32)h + 1);
}

} // namespace

// ---- SDL-style objects -------------------------------------------------------

struct SDL_Window {
	GameWindow* win;
	std::string title;
};

struct SDL_Texture {
	BBitmap* bitmap;
	int w, h;
	Uint32 format;
	bool smooth;
};

struct SDL_Renderer {
	SDL_Window* window;
	int logical_w, logical_h;
	bool integer_scale;
	SDL_Texture* pending;
};

namespace {

FrameLayout compute_layout(SDL_Renderer* r) {
	FrameLayout l;
	memset(&l, 0, sizeof(l));
	if (r == NULL || r->window == NULL) return l;
	r->window->win->GetSize(&l.out_w, &l.out_h);
	int lw = r->logical_w > 0 ? r->logical_w : 320;
	int lh = r->logical_h > 0 ? r->logical_h : 200;
	float sx = (float)l.out_w / lw;
	float sy = (float)l.out_h / lh;
	float s = sx < sy ? sx : sy;
	if (r->integer_scale && s >= 1.0f) s = (float)(int)s;
	if (s <= 0) s = 1.0f;
	l.scale = s;
	l.w = (int)(lw * s);
	l.h = (int)(lh * s);
	if (l.w < 1) l.w = 1;
	if (l.h < 1) l.h = 1;
	l.x = (l.out_w - l.w) / 2;
	l.y = (l.out_h - l.h) / 2;
	return l;
}

} // namespace

extern "C" {

// ---- init / misc -------------------------------------------------------------

const char* SDL_GetError(void) { return g_error.c_str(); }

void SDL_GetVersion(SDL_version* v) {
	if (v) { v->major = SDL_MAJOR_VERSION; v->minor = SDL_MINOR_VERSION; v->patch = SDL_PATCHLEVEL; }
}

int SDL_Init(Uint32 flags) {
	if (g_start_time == 0) g_start_time = system_time();
	if (flags & SDL_INIT_VIDEO) ensure_app();
	return 0;
}

int SDL_InitSubSystem(Uint32 flags) {
	(void)flags;
	return 0;
}

void SDL_Quit(void) {
	g_quitting = true;
	SDL_CloseAudio();
	if (be_app != NULL) {
		g_cursor_hidden = false;
		apply_cursor(false);
		be_app->PostMessage(B_QUIT_REQUESTED);
	}
}

SDL_bool SDL_SetHint(const char* name, const char* value) {
	if (name && value && strcmp(name, SDL_HINT_RENDER_SCALE_QUALITY) == 0) g_scale_quality = value;
	return SDL_TRUE;
}

int SDL_ShowCursor(int toggle) {
	int was = g_cursor_hidden ? SDL_DISABLE : SDL_ENABLE;
	if (toggle == SDL_QUERY || be_app == NULL) return was;
	g_cursor_hidden = (toggle == SDL_DISABLE);
	apply_cursor(g_window_active);
	return was;
}

int SDL_ShowSimpleMessageBox(Uint32 flags, const char* title, const char* message, SDL_Window* w) {
	(void)flags; (void)w;
	ensure_app();
	BAlert* alert = new BAlert(title ? title : "Prince of Persia", message ? message : "", "OK",
		NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->Go();
	return 0;
}

// ---- window ------------------------------------------------------------------

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags) {
	ensure_app();
	if (w <= 0) w = 640;
	if (h <= 0) h = 400;
	BRect screen_frame = BScreen().Frame();
	float left = (x == SDL_WINDOWPOS_UNDEFINED) ? (screen_frame.Width() - w) / 2 : x;
	float top = (y == SDL_WINDOWPOS_UNDEFINED) ? (screen_frame.Height() - h) / 2 : y;
	if (left < 0) left = 0;
	if (top < 20) top = 20;
	SDL_Window* window = new SDL_Window;
	window->title = title ? title : "";
	window->win = new GameWindow(BRect(left, top, left + w - 1, top + h - 1), window->title.c_str());
	if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) window->win->SetFullscreen(true);
	window->win->Show();
	return window;
}

void SDL_SetWindowTitle(SDL_Window* w, const char* title) {
	if (w == NULL || title == NULL) return;
	w->title = title;
	if (w->win->Lock()) { w->win->SetTitle(title); w->win->Unlock(); }
}

void SDL_SetWindowIcon(SDL_Window* w, SDL_Surface* icon) {
	// The application icon comes from the executable's resources on Haiku.
	(void)w; (void)icon;
}

Uint32 SDL_GetWindowFlags(SDL_Window* w) {
	Uint32 flags = SDL_WINDOW_RESIZABLE;
	if (w && w->win->IsFullscreen()) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	return flags;
}

int SDL_SetWindowFullscreen(SDL_Window* w, Uint32 flags) {
	if (w == NULL) return -1;
	if (w->win->Lock()) {
		w->win->SetFullscreen((flags & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0);
		w->win->Unlock();
	}
	return 0;
}

void SDL_GetWindowSize(SDL_Window* w, int* width, int* height) {
	if (w == NULL) { if (width) *width = 0; if (height) *height = 0; return; }
	w->win->GetSize(width, height);
}

void SDL_GL_GetDrawableSize(SDL_Window* w, int* width, int* height) {
	SDL_GetWindowSize(w, width, height);
}

// ---- renderer ----------------------------------------------------------------

SDL_Renderer* SDL_CreateRenderer(SDL_Window* w, int index, Uint32 flags) {
	(void)index; (void)flags;
	if (w == NULL) { g_error = "no window"; return NULL; }
	SDL_Renderer* r = new SDL_Renderer;
	r->window = w;
	r->logical_w = 0;
	r->logical_h = 0;
	r->integer_scale = false;
	r->pending = NULL;
	return r;
}

int SDL_GetRendererInfo(SDL_Renderer* r, SDL_RendererInfo* info) {
	if (r == NULL || info == NULL) return -1;
	info->name = "haiku";
	info->flags = SDL_RENDERER_SOFTWARE;
	return 0;
}

int SDL_RenderSetLogicalSize(SDL_Renderer* r, int w, int h) {
	if (r == NULL) return -1;
	r->logical_w = w;
	r->logical_h = h;
	return 0;
}

void SDL_RenderGetLogicalSize(SDL_Renderer* r, int* w, int* h) {
	if (w) *w = r ? r->logical_w : 0;
	if (h) *h = r ? r->logical_h : 0;
}

int SDL_RenderSetIntegerScale(SDL_Renderer* r, SDL_bool enable) {
	if (r == NULL) return -1;
	r->integer_scale = enable != SDL_FALSE;
	return 0;
}

int SDL_GetRendererOutputSize(SDL_Renderer* r, int* w, int* h) {
	if (r == NULL) return -1;
	SDL_GetWindowSize(r->window, w, h);
	return 0;
}

void SDL_RenderGetScale(SDL_Renderer* r, float* sx, float* sy) {
	FrameLayout l = compute_layout(r);
	if (sx) *sx = l.scale;
	if (sy) *sy = l.scale;
}

void SDL_RenderGetViewport(SDL_Renderer* r, SDL_Rect* rect) {
	if (rect == NULL) return;
	FrameLayout l = compute_layout(r);
	// Reported in logical units, like the engine expects for mapping the mouse.
	rect->x = l.scale > 0 ? (int)(l.x / l.scale) : 0;
	rect->y = l.scale > 0 ? (int)(l.y / l.scale) : 0;
	rect->w = r ? r->logical_w : 0;
	rect->h = r ? r->logical_h : 0;
}

int SDL_SetRenderTarget(SDL_Renderer* r, SDL_Texture* t) { (void)r; (void)t; return 0; }
int SDL_RenderClear(SDL_Renderer* r) { (void)r; return 0; }

int SDL_RenderCopy(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_Rect* dst) {
	(void)src; (void)dst;
	if (r == NULL) return -1;
	r->pending = t;
	return 0;
}

void SDL_RenderPresent(SDL_Renderer* r) {
	if (r == NULL || r->pending == NULL || r->window == NULL) return;
	GameWindow* win = r->window->win;
	if (!win->Lock()) return;
	FrameLayout l = compute_layout(r);
	win->Present(r->pending->bitmap, r->pending->smooth, l);
	win->Unlock();
}

SDL_Texture* SDL_CreateTexture(SDL_Renderer* r, Uint32 format, int access, int w, int h) {
	(void)access;
	if (r == NULL || w <= 0 || h <= 0) return NULL;
	SDL_Texture* t = new SDL_Texture;
	t->bitmap = new BBitmap(BRect(0, 0, w - 1, h - 1), B_RGB32);
	t->w = w;
	t->h = h;
	t->format = format;
	t->smooth = (g_scale_quality != "0" && g_scale_quality != "nearest");
	if (t->bitmap->InitCheck() != B_OK) {
		delete t->bitmap;
		delete t;
		g_error = "could not allocate frame bitmap";
		return NULL;
	}
	memset(t->bitmap->Bits(), 0, t->bitmap->BitsLength());
	return t;
}

int SDL_UpdateTexture(SDL_Texture* t, const SDL_Rect* rect, const void* pixels, int pitch) {
	if (t == NULL || pixels == NULL) return -1;
	int x0 = 0, y0 = 0, w = t->w, h = t->h;
	if (rect) { x0 = rect->x; y0 = rect->y; w = rect->w; h = rect->h; }
	if (x0 < 0 || y0 < 0 || x0 + w > t->w || y0 + h > t->h) return -1;
	Uint8* dst_base = (Uint8*)t->bitmap->Bits();
	int32 dst_pitch = t->bitmap->BytesPerRow();
	for (int y = 0; y < h; ++y) {
		const Uint8* src = (const Uint8*)pixels + y * pitch;
		Uint32* dst = (Uint32*)(dst_base + (y0 + y) * dst_pitch) + x0;
		if (t->format == SDL_PIXELFORMAT_RGB24) {
			for (int x = 0; x < w; ++x, src += 3) {
				dst[x] = 0xFF000000u | ((Uint32)src[0] << 16) | ((Uint32)src[1] << 8) | src[2];
			}
		} else if (t->format == SDL_PIXELFORMAT_ABGR8888) {
			const Uint32* s32 = (const Uint32*)src;
			for (int x = 0; x < w; ++x) {
				Uint32 v = s32[x];
				dst[x] = 0xFF000000u | ((v & 0xFF) << 16) | (v & 0xFF00) | ((v >> 16) & 0xFF);
			}
		} else {
			memcpy(dst, src, (size_t)w * 4);
		}
	}
	return 0;
}

// ---- events ------------------------------------------------------------------

int SDL_PollEvent(SDL_Event* event) {
	g_event_lock.Lock();
	if (g_events.empty()) { g_event_lock.Unlock(); return 0; }
	if (event) *event = g_events.front();
	g_events.pop_front();
	g_event_lock.Unlock();
	return 1;
}

int SDL_PushEvent(SDL_Event* event) {
	if (event == NULL) return -1;
	push_event(*event);
	return 1;
}

const Uint8* SDL_GetKeyboardState(int* numkeys) {
	if (numkeys) *numkeys = SDL_NUM_SCANCODES;
	return g_keystate;
}

Uint32 SDL_GetMouseState(int* x, int* y) {
	if (x) *x = g_mouse_x;
	if (y) *y = g_mouse_y;
	return 0;
}

void SDL_StartTextInput(void) {}
void SDL_StopTextInput(void) {}
void SDL_SetTextInputRect(const SDL_Rect* rect) { (void)rect; }

const char* SDL_GetScancodeName(SDL_Scancode sc) {
	static const char* letters[] = { "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z" };
	static const char* digits[] = { "1","2","3","4","5","6","7","8","9","0" };
	static const char* fkeys[] = { "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12" };
	static const char* kp[] = { "Keypad 1","Keypad 2","Keypad 3","Keypad 4","Keypad 5","Keypad 6","Keypad 7","Keypad 8","Keypad 9" };
	int c = (int)sc;
	if (c >= SDL_SCANCODE_A && c <= SDL_SCANCODE_Z) return letters[c - SDL_SCANCODE_A];
	if (c >= SDL_SCANCODE_1 && c <= SDL_SCANCODE_0) return digits[c - SDL_SCANCODE_1];
	if (c >= SDL_SCANCODE_F1 && c <= SDL_SCANCODE_F12) return fkeys[c - SDL_SCANCODE_F1];
	if (c >= SDL_SCANCODE_KP_1 && c <= SDL_SCANCODE_KP_9) return kp[c - SDL_SCANCODE_KP_1];
	switch (c) {
		case SDL_SCANCODE_RETURN: return "Return";
		case SDL_SCANCODE_ESCAPE: return "Escape";
		case SDL_SCANCODE_BACKSPACE: return "Backspace";
		case SDL_SCANCODE_TAB: return "Tab";
		case SDL_SCANCODE_SPACE: return "Space";
		case SDL_SCANCODE_MINUS: return "-";
		case SDL_SCANCODE_EQUALS: return "=";
		case SDL_SCANCODE_LEFTBRACKET: return "[";
		case SDL_SCANCODE_RIGHTBRACKET: return "]";
		case SDL_SCANCODE_BACKSLASH: return "\\";
		case SDL_SCANCODE_SEMICOLON: return ";";
		case SDL_SCANCODE_APOSTROPHE: return "'";
		case SDL_SCANCODE_GRAVE: return "`";
		case SDL_SCANCODE_COMMA: return ",";
		case SDL_SCANCODE_PERIOD: return ".";
		case SDL_SCANCODE_SLASH: return "/";
		case SDL_SCANCODE_CAPSLOCK: return "CapsLock";
		case SDL_SCANCODE_PRINTSCREEN: return "PrintScreen";
		case SDL_SCANCODE_SCROLLLOCK: return "ScrollLock";
		case SDL_SCANCODE_PAUSE: return "Pause";
		case SDL_SCANCODE_INSERT: return "Insert";
		case SDL_SCANCODE_HOME: return "Home";
		case SDL_SCANCODE_PAGEUP: return "PageUp";
		case SDL_SCANCODE_DELETE: return "Delete";
		case SDL_SCANCODE_END: return "End";
		case SDL_SCANCODE_PAGEDOWN: return "PageDown";
		case SDL_SCANCODE_RIGHT: return "Right";
		case SDL_SCANCODE_LEFT: return "Left";
		case SDL_SCANCODE_DOWN: return "Down";
		case SDL_SCANCODE_UP: return "Up";
		case SDL_SCANCODE_NUMLOCKCLEAR: return "Numlock";
		case SDL_SCANCODE_KP_DIVIDE: return "Keypad /";
		case SDL_SCANCODE_KP_MULTIPLY: return "Keypad *";
		case SDL_SCANCODE_KP_MINUS: return "Keypad -";
		case SDL_SCANCODE_KP_PLUS: return "Keypad +";
		case SDL_SCANCODE_KP_ENTER: return "Keypad Enter";
		case SDL_SCANCODE_KP_0: return "Keypad 0";
		case SDL_SCANCODE_KP_PERIOD: return "Keypad .";
		case SDL_SCANCODE_LCTRL: return "Left Ctrl";
		case SDL_SCANCODE_LSHIFT: return "Left Shift";
		case SDL_SCANCODE_LALT: return "Left Alt";
		case SDL_SCANCODE_LGUI: return "Left GUI";
		case SDL_SCANCODE_RCTRL: return "Right Ctrl";
		case SDL_SCANCODE_RSHIFT: return "Right Shift";
		case SDL_SCANCODE_RALT: return "Right Alt";
		case SDL_SCANCODE_RGUI: return "Right GUI";
		case SDL_SCANCODE_CLEAR: return "Clear";
		default: return "";
	}
}

// ---- game controllers (none) -------------------------------------------------

int SDL_NumJoysticks(void) { return 0; }
SDL_bool SDL_IsGameController(int index) { (void)index; return SDL_FALSE; }
SDL_GameController* SDL_GameControllerOpen(int index) { (void)index; return NULL; }
void SDL_GameControllerClose(SDL_GameController* gc) { (void)gc; }
SDL_GameController* SDL_GameControllerFromInstanceID(Sint32 id) { (void)id; return NULL; }
int SDL_GameControllerAddMappingsFromFile(const char* file) { (void)file; return 0; }
int SDL_GameControllerRumble(SDL_GameController* gc, Uint16 low, Uint16 high, Uint32 ms) { (void)gc; (void)low; (void)high; (void)ms; return -1; }
SDL_Joystick* SDL_JoystickOpen(int index) { (void)index; return NULL; }
int SDL_JoystickRumble(SDL_Joystick* j, Uint16 low, Uint16 high, Uint32 ms) { (void)j; (void)low; (void)high; (void)ms; return -1; }
SDL_Haptic* SDL_HapticOpen(int index) { (void)index; return NULL; }
int SDL_HapticRumbleInit(SDL_Haptic* h) { (void)h; return -1; }
int SDL_HapticRumblePlay(SDL_Haptic* h, float strength, Uint32 ms) { (void)h; (void)strength; (void)ms; return -1; }

// ---- time and timers ---------------------------------------------------------

Uint32 SDL_GetTicks(void) {
	if (g_start_time == 0) g_start_time = system_time();
	return (Uint32)((system_time() - g_start_time) / 1000);
}

void SDL_Delay(Uint32 ms) {
	snooze((bigtime_t)ms * 1000);
}

Uint64 SDL_GetPerformanceCounter(void) { return (Uint64)system_time(); }
Uint64 SDL_GetPerformanceFrequency(void) { return 1000000; }

} // extern "C"

namespace {

struct Timer {
	int id;
	Uint32 interval;
	SDL_TimerCallback callback;
	void* param;
	volatile bool cancelled;
};

BLocker g_timer_lock("pop timers");
std::deque<Timer*> g_timers;
int g_next_timer_id = 1;

void forget_timer(Timer* t) {
	g_timer_lock.Lock();
	for (std::deque<Timer*>::iterator it = g_timers.begin(); it != g_timers.end(); ++it) {
		if (*it == t) { g_timers.erase(it); break; }
	}
	g_timer_lock.Unlock();
	delete t;
}

int32 timer_thread(void* data) {
	Timer* t = (Timer*)data;
	Uint32 interval = t->interval;
	while (true) {
		snooze((bigtime_t)interval * 1000);
		if (t->cancelled) break;
		Uint32 next = t->callback(interval, t->param);
		if (next == 0 || t->cancelled) break;
		interval = next;
	}
	forget_timer(t);
	return 0;
}

} // namespace

extern "C" {

SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_TimerCallback callback, void* param) {
	if (callback == NULL) return 0;
	Timer* t = new Timer;
	t->interval = interval;
	t->callback = callback;
	t->param = param;
	t->cancelled = false;
	g_timer_lock.Lock();
	t->id = g_next_timer_id++;
	g_timers.push_back(t);
	g_timer_lock.Unlock();
	thread_id th = spawn_thread(timer_thread, "pop timer", B_NORMAL_PRIORITY, t);
	if (th < 0) { forget_timer(t); return 0; }
	resume_thread(th);
	return t->id;
}

SDL_bool SDL_RemoveTimer(SDL_TimerID id) {
	SDL_bool found = SDL_FALSE;
	g_timer_lock.Lock();
	for (std::deque<Timer*>::iterator it = g_timers.begin(); it != g_timers.end(); ++it) {
		if ((*it)->id == id) { (*it)->cancelled = true; found = SDL_TRUE; break; }
	}
	g_timer_lock.Unlock();
	return found;
}

} // extern "C"
