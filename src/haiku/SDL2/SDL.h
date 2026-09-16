/*
 * Haiku platform layer for the Prince of Persia engine.
 *
 * The engine (src/engine) was written against the SDL2 API. This header
 * declares exactly the subset of that API the engine uses; the functions are
 * implemented natively on top of the Be API in the src/haiku sources. There is no
 * SDL library involved, and no DOS emulation: the game code runs directly as a
 * Haiku process.
 */
#ifndef HAIKU_POP_SDL_H
#define HAIKU_POP_SDL_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <alloca.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- basic types -------------------------------------------------------- */

typedef uint8_t  Uint8;
typedef int8_t   Sint8;
typedef uint16_t Uint16;
typedef int16_t  Sint16;
typedef uint32_t Uint32;
typedef int32_t  Sint32;
typedef uint64_t Uint64;
typedef int64_t  Sint64;

typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER  SDL_LIL_ENDIAN

#define SDL_SwapLE16(x) ((Uint16)(x))
#define SDL_SwapLE32(x) ((Uint32)(x))
#define SDL_SwapBE16(x) ((Uint16)__builtin_bswap16((Uint16)(x)))
#define SDL_SwapBE32(x) ((Uint32)__builtin_bswap32((Uint32)(x)))

#define SDL_COMPILE_TIME_ASSERT(name, x) typedef int SDL_dummy_##name[(x) * 2 - 1]

#define SDL_free   free
#define SDL_memset memset
#define SDL_strlen strlen

/* ---- version / init ----------------------------------------------------- */

typedef struct SDL_version { Uint8 major, minor, patch; } SDL_version;

#define SDL_MAJOR_VERSION 2
#define SDL_MINOR_VERSION 0
#define SDL_PATCHLEVEL    20
#define SDL_VERSION(v) do { (v)->major = SDL_MAJOR_VERSION; (v)->minor = SDL_MINOR_VERSION; (v)->patch = SDL_PATCHLEVEL; } while (0)
#define SDL_VERSION_ATLEAST(X, Y, Z) \
	((SDL_MAJOR_VERSION > (X)) || \
	 (SDL_MAJOR_VERSION == (X) && SDL_MINOR_VERSION > (Y)) || \
	 (SDL_MAJOR_VERSION == (X) && SDL_MINOR_VERSION == (Y) && SDL_PATCHLEVEL >= (Z)))

void SDL_GetVersion(SDL_version* v);
const char* SDL_GetError(void);

#define SDL_INIT_TIMER          0x00000001u
#define SDL_INIT_AUDIO          0x00000010u
#define SDL_INIT_VIDEO          0x00000020u
#define SDL_INIT_JOYSTICK       0x00000200u
#define SDL_INIT_HAPTIC         0x00001000u
#define SDL_INIT_GAMECONTROLLER 0x00002000u
#define SDL_INIT_NOPARACHUTE    0x00100000u

int  SDL_Init(Uint32 flags);
int  SDL_InitSubSystem(Uint32 flags);
void SDL_Quit(void);

#define SDL_HINT_RENDER_SCALE_QUALITY "SDL_RENDER_SCALE_QUALITY"
#define SDL_HINT_RENDER_VSYNC         "SDL_RENDER_VSYNC"
#define SDL_HINT_IME_SHOW_UI          "SDL_IME_SHOW_UI"
SDL_bool SDL_SetHint(const char* name, const char* value);

/* ---- surfaces ----------------------------------------------------------- */

typedef struct SDL_Rect { int x, y, w, h; } SDL_Rect;
typedef struct SDL_Color { Uint8 r, g, b, a; } SDL_Color;

typedef struct SDL_Palette {
	int ncolors;
	SDL_Color* colors;
	Uint32 version;
	int refcount;
} SDL_Palette;

enum {
	SDL_PIXELFORMAT_UNKNOWN  = 0,
	SDL_PIXELFORMAT_INDEX8   = 1,
	SDL_PIXELFORMAT_RGB24    = 2,   /* bytes in memory: R G B            */
	SDL_PIXELFORMAT_ARGB8888 = 3,   /* bytes in memory: B G R A (LE)     */
	SDL_PIXELFORMAT_ABGR8888 = 4,   /* bytes in memory: R G B A (LE)     */
	SDL_PIXELFORMAT_RGB888   = 5,   /* bytes in memory: B G R x (LE)     */
	SDL_PIXELFORMAT_RGBA32   = SDL_PIXELFORMAT_ABGR8888
};
#define SDL_ISPIXELFORMAT_INDEXED(f) ((f) == SDL_PIXELFORMAT_INDEX8)
const char* SDL_GetPixelFormatName(Uint32 format);

typedef struct SDL_PixelFormat {
	Uint32 format;
	SDL_Palette* palette;
	Uint8 BitsPerPixel;
	Uint8 BytesPerPixel;
	Uint32 Rmask, Gmask, Bmask, Amask;
	Uint8 Rshift, Gshift, Bshift, Ashift;
} SDL_PixelFormat;

typedef enum {
	SDL_BLENDMODE_NONE  = 0,
	SDL_BLENDMODE_BLEND = 1,
	SDL_BLENDMODE_ADD   = 2,
	SDL_BLENDMODE_MOD   = 4
} SDL_BlendMode;

typedef struct SDL_Surface {
	Uint32 flags;
	SDL_PixelFormat* format;
	int w, h;
	int pitch;
	void* pixels;
	void* userdata;
	int locked;
	SDL_Rect clip_rect;
	int refcount;
	/* platform-private state (the engine never touches these) */
	int colorkey_enabled;
	Uint32 colorkey;
	int blend_mode;
	Uint8 alpha_mod;
} SDL_Surface;

#define SDL_ALPHA_OPAQUE      255
#define SDL_ALPHA_TRANSPARENT 0

SDL_Surface* SDL_CreateRGBSurface(Uint32 flags, int w, int h, int depth, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
void   SDL_FreeSurface(SDL_Surface* s);
int    SDL_LockSurface(SDL_Surface* s);
void   SDL_UnlockSurface(SDL_Surface* s);
int    SDL_FillRect(SDL_Surface* dst, const SDL_Rect* rect, Uint32 color);
int    SDL_BlitSurface(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect);
int    SDL_BlitScaled(SDL_Surface* src, const SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect);
int    SDL_SetColorKey(SDL_Surface* s, int flag, Uint32 key);
int    SDL_SetPaletteColors(SDL_Palette* palette, const SDL_Color* colors, int firstcolor, int ncolors);
int    SDL_SetSurfacePalette(SDL_Surface* s, SDL_Palette* palette);
int    SDL_SetSurfaceBlendMode(SDL_Surface* s, SDL_BlendMode mode);
int    SDL_SetSurfaceAlphaMod(SDL_Surface* s, Uint8 alpha);
SDL_bool SDL_SetClipRect(SDL_Surface* s, const SDL_Rect* rect);
SDL_Surface* SDL_ConvertSurface(SDL_Surface* src, const SDL_PixelFormat* fmt, Uint32 flags);
SDL_Surface* SDL_ConvertSurfaceFormat(SDL_Surface* src, Uint32 pixel_format, Uint32 flags);
Uint32 SDL_MapRGB(const SDL_PixelFormat* fmt, Uint8 r, Uint8 g, Uint8 b);
Uint32 SDL_MapRGBA(const SDL_PixelFormat* fmt, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/* ---- window / renderer -------------------------------------------------- */

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;

#define SDL_WINDOWPOS_UNDEFINED 0x1FFF0000
#define SDL_WINDOW_FULLSCREEN_DESKTOP 0x00001001u
#define SDL_WINDOW_RESIZABLE          0x00000020u
#define SDL_WINDOW_ALLOW_HIGHDPI      0x00002000u

SDL_Window* SDL_CreateWindow(const char* title, int x, int y, int w, int h, Uint32 flags);
void   SDL_SetWindowTitle(SDL_Window* w, const char* title);
void   SDL_SetWindowIcon(SDL_Window* w, SDL_Surface* icon);
Uint32 SDL_GetWindowFlags(SDL_Window* w);
int    SDL_SetWindowFullscreen(SDL_Window* w, Uint32 flags);
void   SDL_GetWindowSize(SDL_Window* w, int* width, int* height);
void   SDL_GL_GetDrawableSize(SDL_Window* w, int* width, int* height);

#define SDL_QUERY   -1
#define SDL_DISABLE 0
#define SDL_ENABLE  1
int SDL_ShowCursor(int toggle);

#define SDL_MESSAGEBOX_ERROR 0x10
int SDL_ShowSimpleMessageBox(Uint32 flags, const char* title, const char* message, SDL_Window* w);

#define SDL_RENDERER_SOFTWARE      0x00000001u
#define SDL_RENDERER_ACCELERATED   0x00000002u
#define SDL_RENDERER_TARGETTEXTURE 0x00000008u

typedef struct SDL_RendererInfo {
	const char* name;
	Uint32 flags;
} SDL_RendererInfo;

enum {
	SDL_TEXTUREACCESS_STATIC = 0,
	SDL_TEXTUREACCESS_STREAMING = 1,
	SDL_TEXTUREACCESS_TARGET = 2
};

SDL_Renderer* SDL_CreateRenderer(SDL_Window* w, int index, Uint32 flags);
int  SDL_GetRendererInfo(SDL_Renderer* r, SDL_RendererInfo* info);
int  SDL_RenderSetLogicalSize(SDL_Renderer* r, int w, int h);
void SDL_RenderGetLogicalSize(SDL_Renderer* r, int* w, int* h);
int  SDL_RenderSetIntegerScale(SDL_Renderer* r, SDL_bool enable);
int  SDL_GetRendererOutputSize(SDL_Renderer* r, int* w, int* h);
void SDL_RenderGetScale(SDL_Renderer* r, float* sx, float* sy);
void SDL_RenderGetViewport(SDL_Renderer* r, SDL_Rect* rect);
int  SDL_SetRenderTarget(SDL_Renderer* r, SDL_Texture* t);
int  SDL_RenderClear(SDL_Renderer* r);
int  SDL_RenderCopy(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* src, const SDL_Rect* dst);
void SDL_RenderPresent(SDL_Renderer* r);
SDL_Texture* SDL_CreateTexture(SDL_Renderer* r, Uint32 format, int access, int w, int h);
int  SDL_UpdateTexture(SDL_Texture* t, const SDL_Rect* rect, const void* pixels, int pitch);

/* ---- keyboard / events -------------------------------------------------- */

typedef enum {
	SDL_SCANCODE_UNKNOWN = 0,
	SDL_SCANCODE_A = 4, SDL_SCANCODE_B, SDL_SCANCODE_C, SDL_SCANCODE_D, SDL_SCANCODE_E,
	SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_I, SDL_SCANCODE_J,
	SDL_SCANCODE_K, SDL_SCANCODE_L, SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_O,
	SDL_SCANCODE_P, SDL_SCANCODE_Q, SDL_SCANCODE_R, SDL_SCANCODE_S, SDL_SCANCODE_T,
	SDL_SCANCODE_U, SDL_SCANCODE_V, SDL_SCANCODE_W, SDL_SCANCODE_X, SDL_SCANCODE_Y,
	SDL_SCANCODE_Z = 29,
	SDL_SCANCODE_1 = 30, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4, SDL_SCANCODE_5,
	SDL_SCANCODE_6, SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_0 = 39,
	SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_BACKSPACE = 42,
	SDL_SCANCODE_TAB = 43, SDL_SCANCODE_SPACE = 44, SDL_SCANCODE_MINUS = 45,
	SDL_SCANCODE_EQUALS = 46, SDL_SCANCODE_LEFTBRACKET = 47, SDL_SCANCODE_RIGHTBRACKET = 48,
	SDL_SCANCODE_BACKSLASH = 49, SDL_SCANCODE_NONUSHASH = 50, SDL_SCANCODE_SEMICOLON = 51,
	SDL_SCANCODE_APOSTROPHE = 52, SDL_SCANCODE_GRAVE = 53, SDL_SCANCODE_COMMA = 54,
	SDL_SCANCODE_PERIOD = 55, SDL_SCANCODE_SLASH = 56, SDL_SCANCODE_CAPSLOCK = 57,
	SDL_SCANCODE_F1 = 58, SDL_SCANCODE_F2, SDL_SCANCODE_F3, SDL_SCANCODE_F4, SDL_SCANCODE_F5,
	SDL_SCANCODE_F6, SDL_SCANCODE_F7, SDL_SCANCODE_F8, SDL_SCANCODE_F9, SDL_SCANCODE_F10,
	SDL_SCANCODE_F11, SDL_SCANCODE_F12 = 69,
	SDL_SCANCODE_PRINTSCREEN = 70, SDL_SCANCODE_SCROLLLOCK = 71, SDL_SCANCODE_PAUSE = 72,
	SDL_SCANCODE_INSERT = 73, SDL_SCANCODE_HOME = 74, SDL_SCANCODE_PAGEUP = 75,
	SDL_SCANCODE_DELETE = 76, SDL_SCANCODE_END = 77, SDL_SCANCODE_PAGEDOWN = 78,
	SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80, SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82,
	SDL_SCANCODE_NUMLOCKCLEAR = 83, SDL_SCANCODE_KP_DIVIDE = 84, SDL_SCANCODE_KP_MULTIPLY = 85,
	SDL_SCANCODE_KP_MINUS = 86, SDL_SCANCODE_KP_PLUS = 87, SDL_SCANCODE_KP_ENTER = 88,
	SDL_SCANCODE_KP_1 = 89, SDL_SCANCODE_KP_2, SDL_SCANCODE_KP_3, SDL_SCANCODE_KP_4,
	SDL_SCANCODE_KP_5, SDL_SCANCODE_KP_6, SDL_SCANCODE_KP_7, SDL_SCANCODE_KP_8,
	SDL_SCANCODE_KP_9 = 97, SDL_SCANCODE_KP_0 = 98, SDL_SCANCODE_KP_PERIOD = 99,
	SDL_SCANCODE_NONUSBACKSLASH = 100, SDL_SCANCODE_APPLICATION = 101,
	SDL_SCANCODE_MUTE = 127, SDL_SCANCODE_VOLUMEUP = 128, SDL_SCANCODE_VOLUMEDOWN = 129,
	SDL_SCANCODE_CLEAR = 156,
	SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225, SDL_SCANCODE_LALT = 226,
	SDL_SCANCODE_LGUI = 227, SDL_SCANCODE_RCTRL = 228, SDL_SCANCODE_RSHIFT = 229,
	SDL_SCANCODE_RALT = 230, SDL_SCANCODE_RGUI = 231,
	SDL_SCANCODE_AUDIOMUTE = 262,
	SDL_NUM_SCANCODES = 512
} SDL_Scancode;

typedef enum {
	KMOD_NONE   = 0x0000,
	KMOD_LSHIFT = 0x0001, KMOD_RSHIFT = 0x0002,
	KMOD_LCTRL  = 0x0040, KMOD_RCTRL  = 0x0080,
	KMOD_LALT   = 0x0100, KMOD_RALT   = 0x0200,
	KMOD_LGUI   = 0x0400, KMOD_RGUI   = 0x0800,
	KMOD_NUM    = 0x1000, KMOD_CAPS   = 0x2000
} SDL_Keymod;
#define KMOD_CTRL  (KMOD_LCTRL | KMOD_RCTRL)
#define KMOD_SHIFT (KMOD_LSHIFT | KMOD_RSHIFT)
#define KMOD_ALT   (KMOD_LALT | KMOD_RALT)
#define KMOD_GUI   (KMOD_LGUI | KMOD_RGUI)

typedef struct SDL_Keysym {
	SDL_Scancode scancode;
	Sint32 sym;
	Uint16 mod;
	Uint32 unused;
} SDL_Keysym;

enum {
	SDL_QUIT = 0x100,
	SDL_WINDOWEVENT = 0x200,
	SDL_KEYDOWN = 0x300, SDL_KEYUP, SDL_TEXTEDITING, SDL_TEXTINPUT,
	SDL_MOUSEMOTION = 0x400, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL,
	SDL_JOYAXISMOTION = 0x600, SDL_JOYBALLMOTION, SDL_JOYHATMOTION, SDL_JOYBUTTONDOWN, SDL_JOYBUTTONUP,
	SDL_CONTROLLERAXISMOTION = 0x650, SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP,
	SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED,
	SDL_USEREVENT = 0x8000
};

enum {
	SDL_WINDOWEVENT_NONE = 0, SDL_WINDOWEVENT_SHOWN, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_EXPOSED,
	SDL_WINDOWEVENT_MOVED, SDL_WINDOWEVENT_RESIZED, SDL_WINDOWEVENT_SIZE_CHANGED,
	SDL_WINDOWEVENT_MINIMIZED, SDL_WINDOWEVENT_MAXIMIZED, SDL_WINDOWEVENT_RESTORED,
	SDL_WINDOWEVENT_ENTER, SDL_WINDOWEVENT_LEAVE, SDL_WINDOWEVENT_FOCUS_GAINED, SDL_WINDOWEVENT_FOCUS_LOST
};

#define SDL_BUTTON_LEFT   1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT  3
#define SDL_BUTTON_X1     4
#define SDL_BUTTON_X2     5

#define SDL_TEXTINPUTEVENT_TEXT_SIZE 32

typedef struct SDL_CommonEvent { Uint32 type; Uint32 timestamp; } SDL_CommonEvent;
typedef struct SDL_KeyboardEvent { Uint32 type; Uint32 timestamp; Uint32 windowID; Uint8 state; Uint8 repeat; SDL_Keysym keysym; } SDL_KeyboardEvent;
typedef struct SDL_TextInputEvent { Uint32 type; Uint32 timestamp; Uint32 windowID; char text[SDL_TEXTINPUTEVENT_TEXT_SIZE]; } SDL_TextInputEvent;
typedef struct SDL_WindowEvent { Uint32 type; Uint32 timestamp; Uint32 windowID; Uint8 event; Sint32 data1; Sint32 data2; } SDL_WindowEvent;
typedef struct SDL_UserEvent { Uint32 type; Uint32 timestamp; Uint32 windowID; Sint32 code; void* data1; void* data2; } SDL_UserEvent;
typedef struct SDL_MouseButtonEvent { Uint32 type; Uint32 timestamp; Uint32 windowID; Uint32 which; Uint8 button; Uint8 state; Uint8 clicks; Sint32 x; Sint32 y; } SDL_MouseButtonEvent;
typedef struct SDL_MouseWheelEvent { Uint32 type; Uint32 timestamp; Uint32 windowID; Uint32 which; Sint32 x; Sint32 y; } SDL_MouseWheelEvent;
typedef struct SDL_ControllerAxisEvent { Uint32 type; Uint32 timestamp; Sint32 which; Uint8 axis; Sint16 value; } SDL_ControllerAxisEvent;
typedef struct SDL_ControllerButtonEvent { Uint32 type; Uint32 timestamp; Sint32 which; Uint8 button; Uint8 state; } SDL_ControllerButtonEvent;
typedef struct SDL_ControllerDeviceEvent { Uint32 type; Uint32 timestamp; Sint32 which; } SDL_ControllerDeviceEvent;
typedef struct SDL_JoyAxisEvent { Uint32 type; Uint32 timestamp; Sint32 which; Uint8 axis; Sint16 value; } SDL_JoyAxisEvent;
typedef struct SDL_JoyButtonEvent { Uint32 type; Uint32 timestamp; Sint32 which; Uint8 button; Uint8 state; } SDL_JoyButtonEvent;

typedef union SDL_Event {
	Uint32 type;
	SDL_CommonEvent common;
	SDL_KeyboardEvent key;
	SDL_TextInputEvent text;
	SDL_WindowEvent window;
	SDL_UserEvent user;
	SDL_MouseButtonEvent button;
	SDL_MouseWheelEvent wheel;
	SDL_ControllerAxisEvent caxis;
	SDL_ControllerButtonEvent cbutton;
	SDL_ControllerDeviceEvent cdevice;
	SDL_JoyAxisEvent jaxis;
	SDL_JoyButtonEvent jbutton;
	Uint8 padding[64];
} SDL_Event;

int  SDL_PollEvent(SDL_Event* event);
int  SDL_PushEvent(SDL_Event* event);
const Uint8* SDL_GetKeyboardState(int* numkeys);
Uint32 SDL_GetMouseState(int* x, int* y);
void SDL_StartTextInput(void);
void SDL_StopTextInput(void);
void SDL_SetTextInputRect(const SDL_Rect* rect);
const char* SDL_GetScancodeName(SDL_Scancode scancode);

/* ---- game controllers (no devices are exposed on Haiku) ----------------- */

typedef struct SDL_GameController SDL_GameController;
typedef struct SDL_Joystick SDL_Joystick;
typedef struct SDL_Haptic SDL_Haptic;

typedef enum {
	SDL_CONTROLLER_AXIS_LEFTX = 0, SDL_CONTROLLER_AXIS_LEFTY, SDL_CONTROLLER_AXIS_RIGHTX,
	SDL_CONTROLLER_AXIS_RIGHTY, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_CONTROLLER_AXIS_TRIGGERRIGHT
} SDL_GameControllerAxis;

typedef enum {
	SDL_CONTROLLER_BUTTON_A = 0, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
	SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_GUIDE, SDL_CONTROLLER_BUTTON_START,
	SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
	SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
	SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
	SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT
} SDL_GameControllerButton;

int  SDL_NumJoysticks(void);
SDL_bool SDL_IsGameController(int index);
SDL_GameController* SDL_GameControllerOpen(int index);
void SDL_GameControllerClose(SDL_GameController* gc);
SDL_GameController* SDL_GameControllerFromInstanceID(Sint32 id);
int  SDL_GameControllerAddMappingsFromFile(const char* file);
int  SDL_GameControllerRumble(SDL_GameController* gc, Uint16 low, Uint16 high, Uint32 ms);
SDL_Joystick* SDL_JoystickOpen(int index);
int  SDL_JoystickRumble(SDL_Joystick* j, Uint16 low, Uint16 high, Uint32 ms);
SDL_Haptic* SDL_HapticOpen(int index);
int  SDL_HapticRumbleInit(SDL_Haptic* h);
int  SDL_HapticRumblePlay(SDL_Haptic* h, float strength, Uint32 ms);

/* ---- audio -------------------------------------------------------------- */

typedef Uint16 SDL_AudioFormat;
#define AUDIO_U8     0x0008
#define AUDIO_S16LSB 0x8010
#define AUDIO_S16SYS AUDIO_S16LSB

typedef void (*SDL_AudioCallback)(void* userdata, Uint8* stream, int len);

typedef struct SDL_AudioSpec {
	int freq;
	SDL_AudioFormat format;
	Uint8 channels;
	Uint8 silence;
	Uint16 samples;
	Uint16 padding;
	Uint32 size;
	SDL_AudioCallback callback;
	void* userdata;
} SDL_AudioSpec;

typedef enum { SDL_AUDIO_STOPPED = 0, SDL_AUDIO_PLAYING, SDL_AUDIO_PAUSED } SDL_AudioStatus;

int  SDL_OpenAudio(SDL_AudioSpec* desired, SDL_AudioSpec* obtained);
void SDL_PauseAudio(int pause_on);
void SDL_LockAudio(void);
void SDL_UnlockAudio(void);
void SDL_CloseAudio(void);
SDL_AudioStatus SDL_GetAudioStatus(void);

/* ---- timers ------------------------------------------------------------- */

Uint32 SDL_GetTicks(void);
void   SDL_Delay(Uint32 ms);
Uint64 SDL_GetPerformanceCounter(void);
Uint64 SDL_GetPerformanceFrequency(void);

typedef Uint32 (*SDL_TimerCallback)(Uint32 interval, void* param);
typedef int SDL_TimerID;
SDL_TimerID SDL_AddTimer(Uint32 interval, SDL_TimerCallback callback, void* param);
SDL_bool    SDL_RemoveTimer(SDL_TimerID id);

/* ---- read/write streams ------------------------------------------------- */

typedef struct SDL_RWops SDL_RWops;
SDL_RWops* SDL_RWFromFile(const char* file, const char* mode);
SDL_RWops* SDL_RWFromMem(void* mem, int size);
SDL_RWops* SDL_RWFromConstMem(const void* mem, int size);
size_t SDL_RWread(SDL_RWops* ctx, void* ptr, size_t size, size_t maxnum);
size_t SDL_RWwrite(SDL_RWops* ctx, const void* ptr, size_t size, size_t num);
Sint64 SDL_RWtell(SDL_RWops* ctx);
int    SDL_RWclose(SDL_RWops* ctx);

#ifdef __cplusplus
}
#endif

#endif /* HAIKU_POP_SDL_H */
