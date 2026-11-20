# Finding Your Way

## Go to Class, File and Symbol

| Keys | Opens |
|---|---|
| Ctrl+N | Go to Class |
| Ctrl+Shift+N | Go to File |
| Ctrl+Alt+Shift+N | Go to Symbol: any function, variable, type or namespace |

All three are one dialog with a tab for each.  Type part of a name and the best matches are listed as
you type, each with where it is, dimmer, after it.  Up, Down, Page Up and Page Down move through them
while you type; Tab and Shift+Tab change tab (so do the three keys above); Enter or a double click opens
the one selected; Escape closes the dialog.

What you type is matched as IntelliJ IDEA matches it:

- The characters are found in the name in order, whatever their case, each straight after the one before
  or at the start of a word of the name.  `CTA` and `TeArea` both find `CTextArea`; `tarea` finds
  `TextArea`.  Words start at capitals, digits, and after underscores and dots.
- A `*` or a space lets the next character be anywhere further on.
- In Go to File, `/` separates folders: `wl/tf` finds `WindowLib/TextField.h`.
- A colon and a number at the end opens the file at that line: `TextArea.cpp:120`.

Exact names come first, then names that start with what you typed, then the closest matches.

## Going to a definition

| Keys | What they do |
|---|---|
| Ctrl+B, Ctrl+click | Go to Source.  On a use: its definition, or its declaration if it has no definition.  On an `#include`: the file it includes.  On a declaration: a chooser of its usages, one of which opens. |
| F12 | Go to Definition, as Visual Studio's. |
| Ctrl+Alt+B | Go to Implementation(s).  From a virtual function: the functions that override it.  From a class: the classes derived from it.  From a function's declaration: its definition. |
| Ctrl+U | Go to Super Method.  From a member function (named at the caret, or the one the caret is anywhere in): what it overrides in its base classes.  From a class: its base classes. |
| Ctrl+Shift+B | Go to Type Declaration: the class, enum or typedef that the variable, field, parameter or function at the caret is declared with. |
| Ctrl+Alt+Home | Go to Related Symbol: everything below, and the file's header or source, in one chooser under headings. |
| Ctrl+Alt+F7 | Show Usages: the chooser of the usages of the name at the caret, wherever it is. |
| Alt+O | Switch between a `.cpp` and its header. |
| Ctrl+Alt+Left, Ctrl+Alt+Right | Back and Forward. |

When one of these has a single place to go, it goes there.  When it has several it lists them in a
**chooser** under the caret, the nearest class's first, with a preview of the selected one's code beside
them:

- type and the rows are filtered as Go to filters (`rsq` finds `CRoundedSquare::Draw`);
- Up and Down move through them, Enter or a click goes to the one selected, Escape closes it;
- the usages' chooser has **Open in Find Window**, which lists them all in Find Usages.

When it has nowhere to go, the status line says so ("No implementations of CRoundedSquare.").
Go to Type Declaration reads the type from the declaration, so a built in type (`int`) or `auto` has
nothing to go to.

**Ctrl+click** puts the caret on the name clicked and goes as Ctrl+B does from there.  Shift+click
still extends the selection; Ctrl+click no longer does.

**Back and Forward** go through the places the caret has been, as a browser's history does.  A jump
(Go to, Find Usages, F4, Next Occurrence, a file opened from Solution Explorer) puts where you were on
Back, as does a big move of the caret (a click far away, Ctrl+End) or choosing another tab.  Back reopens
a file if it was closed since.  The history is kept for the session only.

## Find Usages

Alt+F7 finds every use of the symbol at the caret across the workspace and shows them in the **Find
Usages** tool window.  Shift+F12, Find All References, does the same and also lists its declarations and
definition, as Visual Studio's does.

```text
Usages of Init in Solution  14 usages
  Function call  9 usages
    BaseLib  9 usages
      Chars.cpp  3 usages
        CChars
          128  Init();
```

The tree groups the usages by **how each is used** (a call, a value read, a value written, a type in a
field's declaration, a base class...), then **project**, **file** and **class**, with each usage's line
under them, its line number dim and the name itself highlighted.  Select a line and its code is shown in
the preview on the right.  Every group and file starts open, however many usages there are: tens of
thousands in one file open and scroll as quickly as ten.

| In the tree | What it does |
|---|---|
| Enter, double click | Opens the usage; the editor gets the keys. |
| F4 | Jump to Source: goes to the symbol's own declaration or definition, whichever line is selected. |
| A click, the arrows | Only select (and preview), unless Navigate with Single Click is on. |
| Escape | Gives the keys back to the editor. |
| Space, typing | Nothing.  Space doesn't open a usage as Enter does, and nothing typed in the tree reaches the editor. |

Its toolbar, down the left:

| Button | What it does |
|---|---|
| Rerun | Finds the usages again, of the same symbol, wherever the caret is now. |
| Previous Occurrence, Next Occurrence | Step through the usages. |
| Expand All, Collapse All | Open or close every group. |
| Group by Usage Type, Project, Class | Each grouping on or off.  With all off, the tree is files and their lines. |
| Show Read Access, Show Write Access | For a variable or field, both on to begin with: turn one off to leave out the usages that read it, or that write it.  The first row says how many are hidden. |
| Scope | Shows this tab's usages in another [scope](#scopes), without finding them again. |
| Pin Tab | Keeps this tab: the next Find Usages makes a new one. |
| Preview | The preview on or off. |
| Navigate with Single Click | Open each usage as it is selected, by a click or the arrows. |

Each Find Usages goes into the chosen tab, named for the symbol, unless that tab is pinned (a pinned tab's
name starts with `* `), in which case a new tab is made.  A tab is closed by its cross.  The groupings and
filters are the tool window's, the same for every tab, and are remembered between sessions.  The usages
follow edits: change a file and they stay on the same text.

**Find Usages Settings** (Ctrl+Alt+Shift+F7) chooses the scope usages are shown in, the directory for the
Directory scope, and whether Find Usages includes the declarations.  OK finds the usages of the symbol at
the caret with them, and Alt+F7 uses them from then on.  Shift+F12 uses the scope too, and always has the
declarations.  They are remembered between sessions.

### Scopes

| Scope | The files in it |
|---|---|
| Solution | Every solution of the workspace. |
| Startup Solution | The startup solution alone (only offered with more than one solution open). |
| Project | The active file's project. |
| Current File | The file in the editor. |
| Open Files | The files with a tab. |
| Directory | The files in a directory, and under it. |

The index is always of the whole workspace: a scope leaves usages out of the tree, it doesn't search
less.  The active file and its project are those of when the usages were found, so a tab shows the same
whatever is opened afterwards.  A header no project lists is in Solution only.  The Ctrl+B usages popup
always shows every usage, and [Rename](refactoring/rename.md) always changes every one.

## Find in Files

Ctrl+Shift+F looks for text in a scope's files.  Its dialog has a tab for each [scope](#scopes), Match
case and Words (whole words only), and a preview of the first results as you type, each with its file's
code under it.  Text selected in the editor (on one line) is what it looks for; otherwise what you looked
for last.  Enter opens the result selected.

**Open in Find Window** lists every result in the **Find** tool window beside Find Usages: a tree of the
files and their lines, with the same keys, preview and toolbar as Find Usages (without the groupings, the
filters and the tabs).  Open files are searched as their editors have them, unsaved changes included.  The
search runs a little at a time, so the IDE never stops answering keys.

There are no regular expressions, file masks or Replace in Files yet.

## Find Duplicates

Edit, Find Duplicates... (Ctrl+Alt+Shift+D), as IntelliJ IDEA's Locate Duplicates, finds where else the
code at the caret is written, whatever its variables are called there.

- **An expression selected** is looked for inside any statement: `x * x + y * y` is found in
  `d = sqrt(a * a + b * b);`.
- **Statements selected** (whole statements of one block) are looked for as a run of statements.
- **Nothing selected**: the function the caret is in.  First the places its whole body is written, then
  the runs of its statements written elsewhere, the longest first.  A run must be at least as big as the
  [Find Duplicates settings](settings.md#find-duplicates) ask (3 statements and 20 tokens to begin with,
  counting the statements inside an `if` or a loop).

The function's variables and parameters may have other names at another place, as long as each is
always the same other name there and that name is a variable or parameter of the function it is in.
Members, types and functions called must be written the same.  Literals must be the same too, unless
**Literals may differ** is ticked: then a number matches any number, a string any string and a
character any character.

The dialog says what it will look for, and asks for the [scope](#scopes) and the tick.  **Find** lists
what it found in the **Duplicates** tool window: a part of the function each (for a function), each
file under it, and each place's first line, which opens the place.  It has the same keys, preview and
toolbar as Find, and **Rerun** looks again for the same code in the same scope.  The search runs a
little at a time, so the IDE carries on meanwhile, and only files with the names the code is written
with are parsed.

## Next and Previous Occurrence

Ctrl+Alt+Down and Ctrl+Alt+Up step through the results of the tool window with the keys: Find Usages',
Find's, Duplicates', or the [Build tab's](building.md#the-build-tab) errors.  From an editor, they step through the one
such tool window that can be seen and has something to step to; if there are none, or more than one, the
status line says so.  Each step opens the place in the editor.
