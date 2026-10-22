# inFAMOUS Second Son PC (issn_pc) — test builds

Native PC port of inFAMOUS Second Son for Windows (Vulkan). These are **test builds**: expect bugs and
stutter the first time each place's shaders compile.

**No game files are included.** You need your own dump of the game, made from your own copy on your
own PS4 (CUSA00004 or CUSA00223).

## Download

[Releases](../../releases) -> `issn-windows.zip`. Extract it anywhere (not inside the zip) and open
`inFAMOUS Second Son.exe`. Choose the game folder (the one with `eboot.bin`) and click PLAY.
If Windows shows "Windows protected your PC": More info -> Run anyway (the program is not signed).

Requirements: Windows 10 (1903+) or 11 64-bit, a CPU with AVX, a GPU with Vulkan 1.3 and an
up-to-date driver. Updating: extract the new zip over the old folder (saves and settings stay).

**Anti-aliasing:** the game's own, FSR 4 (included, Radeon RX 7000 / 9000) or DLSS (NVIDIA RTX:
NVIDIA's files are not included; download Streamline SDK 2.14.16 and copy `sl.interposer.dll`,
`sl.common.dll`, `sl.dlss.dll`, `sl.pcl.dll`, `nvngx_dlss.dll` from its `bin\x64` folder into
`bin\dlss`; the launcher opens that folder for you).

## Português

Port nativo de inFAMOUS Second Son para PC (Windows, Vulkan). **Builds de teste**: espere bugs e
engasgos na primeira vez que os shaders de cada lugar compilam.

**Nenhum arquivo do jogo vem junto.** Você precisa do seu próprio dump, feito da sua cópia no seu
PS4 (CUSA00004 ou CUSA00223). Baixe o `issn-windows.zip` em [Releases](../../releases), extraia e
abra `inFAMOUS Second Son.exe`. Para atualizar, extraia o zip novo por cima da pasta antiga (os
saves e as configurações ficam).

## Credits

Graphics: axylpup's shadPS4 build for inFAMOUS Second Son ([axylpup/shadPS4](https://github.com/axylpup/shadPS4)),
built on [shadPS4](https://github.com/shadps4-emu/shadPS4), with earlier inFAMOUS work by Ayoub
(shadPS4-AYOUB1080p) and HayatoG (iss_pc). The port itself is built on the God of War III Remastered port
(gow3_pc, Felipe Prado), which comes from bloodborne_pc. FSR 4: AMD FidelityFX (MIT). Launcher title
face: Bebas Neue (SIL OFL). Licensed GPL-2.0-or-later: the source of each build is available from the
authors on request.

Not affiliated with Sony Interactive Entertainment or Sucker Punch Productions. inFAMOUS is a
trademark of Sony Interactive Entertainment.
