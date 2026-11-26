# Fonts

The HCP65 console's font, which the kernal loads from the SD card's `\OS\Fonts\CONSOLE.SFT`.

| Files | Are | From |
|---|---|---|
| `MakeConsoleFont.py` | Source | |
| `CONSOLE.SFT`, `CONSOLE.bmp` | Generated | `python3 MakeConsoleFont.py /usr/share/consolefonts/Lat15-Terminus16.psf.gz CONSOLE.SFT CONSOLE.bmp`, from Terminus Font 16 as Debian's console-setup has it; `CONSOLE.bmp` is a picture of the font, to look at and for the tests. |
| `Terminus-OFL.txt` | Third-party | Terminus Font's licence, the SIL Open Font License 1.1, which the font is under. |
