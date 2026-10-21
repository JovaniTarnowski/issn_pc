inFAMOUS Second Son for PC (issn_pc) - TEST BUILD 0.3.0
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

What's new in 0.3.0
- New graphics core: axylpup's shadPS4 build for this game (github.com/axylpup/shadPS4, release
  1.0.2), about 2.5x the frame rate of 0.2 (city, standing: 38 -> 90-105 FPS on the tester's PC).
- Anti-aliasing (launcher: Graphics > Anti-aliasing): the game's own, FSR 4 Native AA (included;
  Radeon RX 7000 / 9000) or DLSS (NVIDIA RTX; NVIDIA's files are not included, the launcher shows
  where they go and where to download them).
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
  - Spray can: L3 + R3 together (or H) holds the pad like the spray can; L1 shakes it, the right
    stick aims. L3 + R3 again ends it.
  - Touchpad swipe (lift, push): hold Select/Back and push the right stick that way.
  - Two fingers on the touchpad (lifting the tree): hold Select/Back + RB (R1).
  - Touchpad click: Select/Back.
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
