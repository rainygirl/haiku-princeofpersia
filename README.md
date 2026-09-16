<img src="icon.png" width="64" align="left" alt="">

# Prince of Persia for Haiku

[한국어](README.ko.md) | [日本語](README.ja.md)

Prince of Persia (1990) running as a native Haiku application. There is no DOS
emulator and no SDL underneath. The game runs as an ordinary Haiku process,
with a BWindow for video, a BSoundPlayer for sound, and Haiku key codes for
input.

![The title screen on Haiku](screenshots/title.png)

The game logic comes from [SDLPoP](https://github.com/NagyD/SDLPoP), a C
reconstruction of the DOS game made from its disassembly. SDLPoP normally
sits on SDL2. In this port every SDL call it makes goes to a small platform
layer written against the Be API (`src/haiku`). The game data is the DOS
v1.4 release: the `.DAT` files in `data/` hold the graphics, levels,
digitised sounds and music. The DOS `PRINCE.EXE` itself is not run.

Checked on Haiku x86_gcc2 (hrev99002) on 2026-09-16:

- The title sequence, the intro cutscene and the demo play.
- Level 1 starts and the prince can be controlled.
- The screen changes rooms when the prince walks or falls off an edge.
- Sound plays: title music, footsteps and effects.
- Alt+Enter switches to full screen, and Ctrl+Q quits.
- The app launches from the Desktop and from the Deskbar.

![Level 1](screenshots/level1.png)

## Building and installing

The game data is not part of this repository (`data/` is in `.gitignore`).
Copy the `.DAT` files from your own DOS copy of Prince of Persia 1.4 into
`data/` first:

```sh
mkdir -p data
cp /path/to/PRINCE/*.DAT data/
```

Then, on Haiku:

```sh
./install.sh              # build, then install to ~/config/non-packaged/apps
./install.sh --build-only # build only: build/PrinceOfPersia
./install.sh --uninstall  # remove the app, keep save games and settings
```

The installer puts a "Prince of Persia" link on the Desktop and in
Deskbar > Applications. On the 32-bit hybrid the build uses the secondary
compiler (`setarch x86 make`). It needs only the system libraries `libbe`,
`libmedia` and `libtranslation`.

To run it from the source tree without installing, start
`build/PrinceOfPersia`. The program finds `data/` next to itself or one level
up.

## Controls

The keys of the DOS version work as they did there.

| Key | Action |
| --- | --- |
| Arrow keys | Move, jump, crouch |
| Shift | Grab a ledge, pick up, strike, step carefully |
| Esc | Pause |
| Space | Show the time left |
| Ctrl+A | Restart the level |
| Ctrl+G / Ctrl+L | Save game / load game |
| Ctrl+S | Sound on or off |
| Ctrl+R | Back to the title |
| Ctrl+Q | Quit |

A few additions come from SDLPoP.

| Key | Action |
| --- | --- |
| Backspace | Settings menu |
| F6 / F9 | Quick save / quick load |
| Alt+Enter | Full screen on or off |
| F12 | Screenshot, saved to `screenshots/` |

## Settings

`SDLPoP.ini` is set up so the game behaves like the DOS version:

- The SDLPoP information screen is off.
- Esc pauses without opening a menu.
- SDLPoP's gameplay bug fixes are off.
- The copy-protection potion level is skipped. The cracked v1.4 in the source folder skips it as well.

After installation the file lives in
`~/config/non-packaged/apps/Prince of Persia/`, along with save games.
Settings changed in the in-game menu go to `SDLPoP.cfg` in the same folder.

## Layout

| Path | Purpose |
| --- | --- |
| `src/engine/` | SDLPoP game code (GPLv3), unchanged except the window title in `config.h` |
| `src/haiku/SDL2/SDL.h`, `SDL_image.h` | The part of the SDL2 API the engine calls, declared for the Haiku layer |
| `src/haiku/platform.cpp` | BApplication and BWindow, drawing frames, keyboard and mouse, timers, full screen |
| `src/haiku/surface.cpp` | Software surfaces: palettes, color keys, blending, blits, format conversion |
| `src/haiku/audio.cpp` | BSoundPlayer output for the engine's mixer (digital sound plus OPL music synthesis) |
| `src/haiku/image.cpp` | PNG load and save through the Translation Kit |
| `src/haiku/rwops.cpp` | File and memory streams for saves, settings and replays |
| `data/` | Game data from the DOS v1.4 release |
| `tools/make_icon.py` | Builds the vector icon and `resources/PrinceOfPersia.rdef` |
| `tools/rendericon.cpp` | Renders an HVIF icon to PNG, for checking |
| `tools/sendkey.cpp` | Sends key messages to the running game, for testing over SSH |
| `tools/run-remote.sh` | Builds on a Haiku box over SSH, runs the game, collects log and screenshots |

## Limits

- Joysticks and gamepads are not supported. Only the keyboard and mouse work.
- The engine reproduces DOS version 1.0 logic and loads v1.4 data. Where 1.0 and 1.4 differ in behaviour, 1.0 wins.

## License

The engine and the Haiku layer are under GPLv3 (`COPYING`). The files in
`data/` are the original game's copyrighted data. They come from your own
copy of the game, so `data/` is kept out of version control and must not be
redistributed with this code.

## AI disclosure

This program was written with Claude.
