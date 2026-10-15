inFAMOUS Second Son for PC (issn_pc) - IN DEVELOPMENT, NOT PLAYABLE YET
=======================================================================

No game files are included. You need your own dump of inFAMOUS Second Son, made from your own
copy of the game on your own PS4: CUSA00004 (US) or CUSA00223 (EU), base game version 01.00, as
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

Data
- Saves, shader cache and the log: user\ next to inFAMOUS Second Son.exe.
- Settings: issn.ini next to inFAMOUS Second Son.exe; launcher options in %APPDATA%\issn-launcher.
- Generated files (prepared game image, patches): out\.
- The game process is bin\issn-probe.exe (for RTSS, Afterburner or ReShade with Vulkan).

Problems
- The game does not open: check the Log page, and send user\last_run.log with your report.
- Check your dump: python tools\issn\inspect_dump.py "<game folder>".

This project is not affiliated with Sony Interactive Entertainment or Sucker Punch Productions.
inFAMOUS is a trademark of Sony Interactive Entertainment.
