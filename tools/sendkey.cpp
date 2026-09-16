/*
 * Test helper (Haiku only): drives the running game with synthetic keyboard
 * messages, the same B_KEY_DOWN / B_KEY_UP / B_MODIFIERS_CHANGED messages the
 * input server would deliver, so the input path can be exercised over SSH.
 *
 *   sendkey key:<raw>[:<text>]   press and release a key (raw code in hex, as in `keymap -d`)
 *   sendkey down:<raw>[:<text>]  press
 *   sendkey up:<raw>             release
 *   sendkey mod:<raw>:<0|1>      press (1) or release (0) a modifier key
 *   sendkey sleep:<ms>
 *   sendkey close                ask the game window to close
 *
 * Build: setarch x86 g++ -o sendkey tools/sendkey.cpp -lbe
 */
#include <Application.h>
#include <Message.h>
#include <Messenger.h>
#include <OS.h>
#include <InterfaceDefs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* kSignature = "application/x-vnd.rainygirl-princeofpersia";
static uint8 sStates[16];
static int32 sModifiers = 0;

static void set_state(int raw, bool down) {
	uint8 bit = 1 << (7 - (raw & 7));
	if (down) sStates[raw >> 3] |= bit; else sStates[raw >> 3] &= ~bit;
}

static int32 modifier_flag(int raw) {
	switch (raw) {
		case 0x4b: return B_SHIFT_KEY | B_LEFT_SHIFT_KEY;
		case 0x56: return B_SHIFT_KEY | B_RIGHT_SHIFT_KEY;
		case 0x5c: return B_CONTROL_KEY | B_LEFT_CONTROL_KEY;
		case 0x60: return B_CONTROL_KEY | B_RIGHT_CONTROL_KEY;
		case 0x5d: return B_COMMAND_KEY | B_LEFT_COMMAND_KEY;
		case 0x5f: return B_COMMAND_KEY | B_RIGHT_COMMAND_KEY;
		default: return 0;
	}
}

static bool window_messenger(BMessenger* out) {
	BMessenger app(kSignature);
	if (!app.IsValid()) { fprintf(stderr, "game is not running\n"); return false; }
	BMessage request(B_GET_PROPERTY), reply;
	request.AddSpecifier("Window", (int32)0);
	if (app.SendMessage(&request, &reply) != B_OK || reply.FindMessenger("result", out) != B_OK) {
		fprintf(stderr, "no game window\n");
		return false;
	}
	return true;
}

static void send_key(BMessenger& win, int raw, bool down, const char* text) {
	set_state(raw, down);
	BMessage msg(down ? B_KEY_DOWN : B_KEY_UP);
	msg.AddInt64("when", system_time());
	msg.AddInt32("key", raw);
	msg.AddInt32("modifiers", sModifiers);
	msg.AddData("states", B_UINT8_TYPE, sStates, sizeof(sStates));
	const char* bytes = text ? text : "";
	msg.AddInt8("byte", bytes[0]);
	msg.AddString("bytes", bytes);
	msg.AddInt32("raw_char", (unsigned char)bytes[0]);
	win.SendMessage(&msg);
}

static void send_modifier(BMessenger& win, int raw, bool down) {
	int32 old = sModifiers;
	set_state(raw, down);
	if (down) sModifiers |= modifier_flag(raw); else sModifiers &= ~modifier_flag(raw);
	BMessage msg(B_MODIFIERS_CHANGED);
	msg.AddInt64("when", system_time());
	msg.AddInt32("modifiers", sModifiers);
	msg.AddInt32("be:old_modifiers", old);
	msg.AddData("states", B_UINT8_TYPE, sStates, sizeof(sStates));
	win.SendMessage(&msg);
}

int main(int argc, char** argv) {
	BApplication app("application/x-vnd.rainygirl-popsendkey");
	BMessenger win;
	if (!window_messenger(&win)) return 1;
	for (int i = 1; i < argc; ++i) {
		char* arg = strdup(argv[i]);
		char* cmd = strtok(arg, ":");
		char* a1 = strtok(NULL, ":");
		char* a2 = strtok(NULL, "");
		if (cmd == NULL) continue;
		if (strcmp(cmd, "sleep") == 0 && a1) {
			snooze(atol(a1) * 1000);
		} else if (strcmp(cmd, "key") == 0 && a1) {
			int raw = (int)strtol(a1, NULL, 16);
			send_key(win, raw, true, a2);
			snooze(60000);
			send_key(win, raw, false, a2);
		} else if (strcmp(cmd, "down") == 0 && a1) {
			send_key(win, (int)strtol(a1, NULL, 16), true, a2);
		} else if (strcmp(cmd, "up") == 0 && a1) {
			send_key(win, (int)strtol(a1, NULL, 16), false, a2);
		} else if (strcmp(cmd, "mod") == 0 && a1 && a2) {
			send_modifier(win, (int)strtol(a1, NULL, 16), atoi(a2) != 0);
		} else if (strcmp(cmd, "close") == 0) {
			win.SendMessage(B_QUIT_REQUESTED);
		} else {
			fprintf(stderr, "unknown command: %s\n", argv[i]);
		}
		free(arg);
		snooze(20000);
	}
	return 0;
}
