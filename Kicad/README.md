# Kicad

The HCP65's circuit boards, drawn in KiCad 8, and the libraries they are drawn with.  The schematics are a source
of the website's hardware pages (`HCP65-Emulator/HCP65Lib/Docs/hcp65`): what a page says about a board is made from
or checked against its schematic (`Website/DocsPlan.txt` I).

| Folder | Has |
|---|---|
| `Computer` | The computer's boards, by what they do: Main, Memory, Audio, Video, the device boards, and `Component Boards`, the small boards the others are built from. |
| `Boards` | Blank boards (`Blank`) and pin boards (`Pins`) of each size of socket, to start a board from. |
| `Pico`, `Programmers`, `Tester`, `Speed`, `Keypad` | Test, programming and other boards beside the computer. |
| `X65C64` | The 65C64 drawn as a part: its schematic and board. |
| `Old` | Boards no longer built, kept to look back at. |
| `Symbols`, `HCP65_Parts.pretty` | The symbol libraries (one a maker, and `HCP65.kicad_sym`, a symbol a board) and the footprints of the boards and sockets. |
| `SamacSys_Parts.pretty`, `SamacSys_Parts.3dshapes` | Footprints and 3D models of bought parts. |

A board's `<board> Faults.txt` (and `Symbols/Symbol Faults.txt`) lists what a review found wrong with it.  Export
from a copy of a project, not the project itself: `kicad-cli` rewrites the `.kicad_prl` beside every schematic it
reads.

| Files | Are | From |
|---|---|---|
| `**/*.kicad_sch`, `**/*.kicad_pcb`, `**/*.kicad_pro`, `*.kicad_wks`, `**/*.round-tracks-config`, `**/*.par`, `**/*.frb`, `**/sym-lib-table`, `Old/**/*.sch`, `Old/**/*.pro`, `Old/**/*.dcm` | Source | Drawn in KiCad 8 (`Old`'s `.sch`, `.pro` and `.dcm` in KiCad 5); the round tracks plugin's settings and Freerouting's files. |
| `**/*.txt` | Source | Notes: each review's faults, pin lists. |
| `Symbols/*.kicad_sym`, `HCP65_Parts.pretty/*.kicad_mod` | Source | Drawn and kept here (the makers' symbols were begun from SamacSys's, and corrected as `Symbols/Symbol Faults.txt` says). |
| `SamacSys_Parts.pretty/*.kicad_mod`, `SamacSys_Parts.3dshapes/*` | Third-party | SamacSys's Component Search Engine: each part's footprint and 3D model as its Library Loader downloaded it. |
| `**/*.g[bt][losp]`, `**/*.gm1`, `**/*.gbr`, `**/*.gbrjob`, `**/*.drl` | Generated | A board's Gerber and drill files, plotted from its `.kicad_pcb` by KiCad 8's Pcbnew (File, Fabrication Outputs: Gerbers, then Drill Files; `kicad-cli pcb export gerbers` and `kicad-cli pcb export drill` make the same) into a folder named for the board beside it. |
| `Boards/**/*.wrl`, `Computer/**/*.wrl`, `Old/**/*.wrl`, `Pico/**/*.wrl`, `Tester/**/*.wrl` | Generated | A board's 3D model, exported from its `.kicad_pcb` by Pcbnew (File, Export, VRML; `kicad-cli pcb export vrml`). |
| `Old/**/*.step` | Generated | A board's 3D model, exported from its `.kicad_pcb` by Pcbnew (File, Export, STEP; `kicad-cli pcb export step`). |
| `**/*.cad` | Generated | A board in GenCAD, exported from its `.kicad_pcb` by Pcbnew (File, Export, GenCAD). |
| `Old/**/*.net` | Generated | A schematic's netlist, exported by Eeschema (File, Export, Netlist; `kicad-cli sch export netlist`). |
| `Keypad/Fabrication/*.7z` | Generated | The Keypad's Gerber and drill files beside it, packed to send to the board maker. |
