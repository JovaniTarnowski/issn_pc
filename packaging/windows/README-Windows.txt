inFAMOUS Second Son for PC (issn_pc) - TEST BUILD 0.2.0
======================================================

A test build for a few testers: the game plays from the start, with stutter (shaders compiled as
they are first used), about 40-45 FPS in the open city and 35-40 FPS in combat on an RX 9060 XT,
and some flickering effects. Please report how far you get and what goes wrong (see Reporting
problems).

What's new in 0.2.0
- Faster: the effects the game waits for (smoke, sparks, neon, bullets) are handled before the
  rest of the frame (combat about 29 -> 39 FPS on an RX 9060 XT). Launcher: Performance >
  inFAMOUS > "Effects work goes ahead of drawing"; switch it off if the game freezes.
- New launcher look, with Effects (Full, Light, Off) and Draw distance in Performance > inFAMOUS.
- "Save a bug report" (Advanced page): a zip on the desktop with the last log and your settings.
- The launcher checks this port's GitHub releases for updates once they are public.
- Faster in the open city (about 36 -> 39-41 FPS standing still on an RX 9060 XT): the thread
  that feeds the graphics card no longer frees memory nor waits on a lock for every draw, and
  the game's main thread no longer allocates several MB each frame.
- Fixed: on AMD cards the first start with an empty shader cache could stop on a black window
  (pipeline binary cache, now off by default; Performance > Shaders turns it back on).
- The first start after updating compiles the shaders again (more stutter for a while).

No game files are included. You need your own dump of inFAMOUS Second Son, made from your own
copy of the game on your own PS4: CUSA00004 (Europe) or CUSA00223, base game version 01.00, as
a folder that contains eboot.bin, sce_module and sce_sys. How to dump it: docs/issn/DUMP.md.

Built on the God of War III Remastered port (gow3_pc, by Felipe Prado), which comes from
bloodborne_pc (deadinside28, Supermedo). Graphics: the shadPS4 video core, with the inFAMOUS
Second Son fixes of the shadPS4-AYOUB1080p build (Ayoub).

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
- Pads without a touchpad or motion sensors (Xbox, 8BitDo, ...):
  - Spray can: L3 + R3 together (or H) holds the pad like the spray can; L1 shakes it, the right
    stick aims. L3 + R3 again ends it.
  - Touchpad swipe (lift, push): hold Select/Back and push the right stick that way.
  - Two fingers on the touchpad (lifting the tree): hold Select/Back + RB (R1).
  - Touchpad click: Select/Back.
- Text and voices are in English.

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
