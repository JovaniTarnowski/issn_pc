inFAMOUS Second Son for PC (issn_pc) - TEST BUILD 0.3.1
======================================================

A test build: the game plays from the start, at about 90-105 FPS standing in the open city on a
Ryzen 7 3700X + RX 9060 XT once its shaders are compiled. The first time each place or effect
appears its shaders compile and the game stutters; that goes away as the cache fills. Please report
how far you get and what goes wrong (see DLSS (optional, NVIDIA RTX)
NVIDIA's files are not included (licence). Download Streamline SDK 2.14.16 from
https://github.com/NVIDIA-RTX/Streamline/releases (exactly that version) and copy these five files
from its bin\x64 folder (not development) into bin\dlss next to issn-probe.exe (launcher:
Graphics > Open the DLSS folder): sl.interposer.dll, sl.common.dll, sl.dlss.dll, sl.pcl.dll,
nvngx_dlss.dll. Then choose DLSS in Graphics > Anti-aliasing. Untested by us (no NVIDIA card):
please report how it goes. Without the files the game keeps its own anti-aliasing.

Reporting problems).

What's new in 0.3.1
- axylpup's 1.1 fixes in the graphics core: texture-cache hangs, 8 GB AMD cards (RX 6600)
  closing while the world loads, less video memory below 10 GB and less RAM below 24 GB.
- CPUs with fewer than 20 threads no longer give a whole core to the GPU feeder (axylpup's
  measurement: 8-12% more FPS).
- Performance mode (launcher: Performance > Performance mode): Balanced / Maximum use the
  game's own lower settings. On the tester's Ryzen 7 3700X + RX 9060 XT in the city: about
  55-65 FPS before, 80+ with Maximum.
- FSR 4 / DLSS find the game's images by themselves on every game version; Home switches them
  and the game's own AA live; FSR 4's GPU time is in the log (about 1.9 ms on an RX 9060 XT).
- 1:1 mouse aim (axylpup's camera hook): click in the window; Y / F7 lets the mouse go.
- A damaged shader cache file is skipped instead of stopping the game.
- Pads keep working when the game is started from Steam.
- The first start after updating prepares the shaders again (a few minutes, some stutter).

What's new in 0.3.0
- New graphics core: axylpup's shadPS4 build for this game (github.com/axylpup/shadPS4, release
  1.0.2), about 2.5x the frame rate of 0.2 (city, standing: 38 -> 90-105 FPS on the tester's PC).
- Anti-aliasing (launcher: Graphics > Anti-aliasing): the game's own, FSR 4 Native AA (included;
  Radeon RX 7000 / 9000) or DLSS (NVIDIA RTX; NVIDIA's files are not included, the launcher shows
  where they go and where to download them). In the game, Home switches FSR 4 / DLSS and the
  game's own anti-aliasing live (the FPS bar shows which one runs).
- Internal resolution (launcher: Performance > Internal resolution): the game draws at 83%, 67%,
  58% or 50% of 1080 lines. With FSR 4 the picture is rebuilt at the window size by the matching
  preset (67% = Quality, 58% = Balanced, 50% = Performance); with the game's own AA it is only
  scaled up. FSR 4 also upscales the game's 1080p to a larger window (1440p, 4K).
- Performance mode (launcher: Performance > Performance mode, from axylpup's 1.1): the game's
  own lower settings for the open city. Balanced is about 13% more FPS there, Maximum about 19%
  with fewer pedestrians.
- Frame rate limit and VSync from the launcher and the in-game menu (applied on restart);
  Unlimited by default, the game keeps its speed at any frame rate.
- Draw distance back to the game's own, with Long and Very long for less pop-in.
- F12 saves a screenshot (user\screenshots); a loading screen while the shader cache loads.
- New launcher: inFAMOUS look, the dump's art on the home page; "Add to Steam" closes and reopens
  Steam by itself and makes inFAMOUS pictures.

No game files are included. You need your own dump of inFAMOUS Second Son, made from your own
copy of the game on your own PS4: CUSA00004 (Europe) or CUSA00223, base game version 01.00, as
a folder that contains eboot.bin, sce_module and sce_sys. How to dump it: docs/issn/DUMP.md.

Built on the God of War III Remastered port (gow3_pc, by Felipe Prado), which comes from
bloodborne_pc (deadinside28, Supermedo). Graphics: axylpup's shadPS4 build for inFAMOUS Second Son
(github.com/axylpup/shadPS4), from the shadPS4 video core, with earlier work by Ayoub
(shadPS4-AYOUB1080p) and HayatoG (iss_pc). FSR 4: AMD FidelityFX (MIT). Launcher title face: Bebas
Neue (SIL Open Font License).

Requirements
- Windows 10 (1903 or newer) or Windows 11, 64-bit.
- A 64-bit processor with AVX.
- A graphics card with Vulkan 1.3 and an up-to-date driver.

Starting
1. Extract this folder anywhere, for example C:\Games\issn-windows (not inside the zip).
2. Open "inFAMOUS Second Son.exe". If Windows shows "Windows protected your PC", click More info,
   then Run anyway: the program is not digitally signed.
3. Choose the game folder (the one with eboot.bin), then click PLAY.

Controller
- DualShock 4 and DualSense: touchpad, motion sensors (shake and tilt the spray can) and the
  light bar work as on the PS4.
- Pads without a touchpad or motion sensors (Xbox, 8BitDo, ...), on by itself for them (launcher:
  Game > Controls > Controller without touchpad):
  - Spray can: L3 + R3 together tilts the pad like the spray can; L1 rattles it, the right
    stick aims. L3 + R3 again levels it.
  - Touchpad swipe (lift, push): hold Select/Back and push the right stick that way.
  - Two fingers on the touchpad (lifting the tree): hold Select/Back + RB (R1).
  - Touchpad click: Select/Back.
- Keyboard and mouse (axylpup's layout; launcher: Game > Controls changes the buttons):
  - WASD moves, IJKL turns the camera. Click in the game window and the mouse aims 1:1 (axylpup's
    camera hook); Y or F7 lets the mouse go and takes it back; - / = change the mouse speed.
  - Space Cross, Left Shift Circle, C Square, F Triangle (numpad 2/6/4/8 too), Q L1, E R1,
    U L3, O R3, Enter Options, arrows d-pad.
  - Left click R2 (shoot), right click L2 (aim) while the mouse aims; 1 / 3 also work.
  - R / G: left / right side of the touchpad click (G + IJKL: swipe).
  - Spray can: hold Z to tilt the pad, tap Z or hold X to rattle it, move the mouse to aim.
  - F12 screenshot, Insert the port's menu.
- Started from Steam: the port ignores Steam Input's device filter so the controller is seen
  (ISSN_KEEP_STEAM_PAD_FILTER=1 keeps it). If a pad still is not found, the log lists what SDL
  sees ("joystick(s) seen by SDL").
- Text and voices: launcher > Game > Game language (Automatic follows the launcher: Portuguese
  gives the Brazilian dub on the US release).

Data
- Saves, shader cache and the log: user\ next to inFAMOUS Second Son.exe.
- Settings: issn.ini next to inFAMOUS Second Son.exe; launcher options in %APPDATA%\issn-launcher.
- Generated files (prepared game image, patches): out\.
- The game process is bin\issn-probe.exe (for RTSS, Afterburner or ReShade with Vulkan).

Reporting problems
- Click "Save a bug report" on the Advanced page (or send user\last_run.log) with a screenshot or a
  short video, and say where in the game it happened.
- If the game freezes, press F10 once, wait a few seconds, then close it: the log then shows
  what each thread was doing.
- The game does not open: check the Log page, and send user\last_run.log with your report.
- Check your dump: python tools\issn\inspect_dump.py "<game folder>".

This project is not affiliated with Sony Interactive Entertainment or Sucker Punch Productions.
inFAMOUS is a trademark of Sony Interactive Entertainment.
