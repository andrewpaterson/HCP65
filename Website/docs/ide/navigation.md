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

## Moving in a file

| Keys | What they do |
|---|---|
| Ctrl+F12 | File Structure: a chooser of everything the file declares. |
| Alt+7 | The Structure tool window: the same, as a tree that stays open. |
| Alt+Down, Alt+Up | Next and Previous Method: the caret goes to the next function's name, or the one before. |
| Ctrl+G | Go to Line: type a line, or a line and a column (`120` or `120:8`). |
| Ctrl+Shift+M | Move Caret to Matching Brace: from a bracket to the one that closes or opens it. |
| Ctrl+[, Ctrl+] | Move Caret to Code Block Start and End; with Shift the text between is selected. |

What a file declares is its **outline**: its namespaces, classes, enums, functions, fields, variables
and typedefs, each inside the one it is written in, with a function's parameters after its name.  The
outline is read from the file as it is in the editor, saved or not.

**File Structure** lists the outline in a chooser over the editor, each entry indented under the one it
is in with its line after it, and the one the caret is in selected.  Type and the entries are filtered
as Go to filters; Enter or a click puts the caret on the entry's name.

The **Structure** tool window, tabbed with Solution Explorer, shows the outline of the file you are
editing as a tree and keeps up as you edit.  The entry the caret is in is selected as the caret moves.
A click or Enter on an entry puts the caret on its name.  Its context menu sorts the entries by name in
place of the order they are in the file, and hides fields; both choices are kept.

**Go to Line** opens a small prompt over the editor with the caret's line and column in it.  Enter goes
there (a line past the end is the last line), and where you were goes on Back.  What isn't a line is
said to be wrong and the prompt stays; Escape leaves the caret alone.

### Breadcrumbs

Under the code is a strip that says what the caret is in, outermost first:

```text
Deep > CLadder > Climb
```

A namespace, a class in it, a function in the class.  It follows the caret.  A press on one puts the
caret on its name, and opens a chooser of what is beside it (a class's other members, say) so you can
go on to one of those: Escape if you don't want to.

### Sticky lines

When you scroll into a long function, its first line would be out of sight.  It is kept instead, at the
top of the view over the code, with the first lines of the class and the namespace it is in above it,
outermost first and five at most.  As you scroll on into the next function its first line takes the
place of the last one's.  A press on a kept line puts the caret on what it starts.

Breadcrumbs and sticky lines can each be turned off: File, Settings..., **Editor**.

### Bookmarks

| Keys | What they do |
|---|---|
| Ctrl+F11 | Toggle Bookmark on the caret's line. |
| Ctrl+Shift+F11 | Toggle Bookmark with Mnemonic: asks for a digit or a letter. |
| F2, Shift+F2 | Next and Previous Bookmark in this file, round its ends. |
| Ctrl+0 to Ctrl+9 | Go to the bookmark with that digit, in whichever file. |

A bookmarked line has a blue cell in the gutter, with its mnemonic in it if it has one, and a mark on
the scroll bar.  A press on the cell takes the bookmark off.  A mnemonic belongs to one line: giving it
to another takes it from the first.

The **Bookmarks** tool window (View, Bookmarks) lists them by file, each with its line's text and a
preview; Enter or a double click goes to one.

Bookmarks move with their lines as you edit, and are kept with the workspace, in your own
[`.hcp65user`](solutions.md#files-kept-beside-the-solution) file beside it.

### Changes, usages and overrides

| Keys | What they do |
|---|---|
| Ctrl+Alt+Shift+Down, Ctrl+Alt+Shift+Up | Next and Previous Change: the next lines changed since the last commit, or the ones before. |
| Ctrl+Shift+F7 | Highlight Usages in File. |

**Changed lines** are marked on the scroll bar as well as in the gutter, in the same colours, so you can
see where in a long file you have been working.

**Highlight Usages in File** keeps the usages of the name at the caret highlighted in this file while
you move about, marks them on the scroll bar, and says how many there are.  F3 and Shift+F3 step
through them; Escape, or an edit, ends it.  (Leaving the caret on a name highlights its usages too, but
only until the caret moves.)

**Override marks** are in the gutter beside what they are about:

| Mark | Beside | A press on it |
|---|---|---|
| `^` | a function that overrides one of a base class | goes to the function it overrides |
| `v` | a function that is overridden, or a class with derived classes | goes to what overrides it, or the derived classes |
| `x` | a function that does both | opens Go to Related Symbol's chooser |

### Typing to find a row

In Solution Explorer, the Structure tree, the Find Usages, Find and Duplicates trees, and a chooser
that has no field to type in: just type.  What you type is shown in the corner, and the first row from
the selected one that has it is selected.  It is matched as Go to matches names (`sexp` finds
`Solution Explorer`), or anywhere in the row whatever its case.  Up and Down go to the previous and
next row that matches, Backspace takes a character back, Escape ends it and Enter opens the row.  While
nothing matches, the corner's border is red and the selection stays where it was.

## Find and Replace in the file

| Keys | What they do |
|---|---|
| Ctrl+F | Find: a bar over the code. |
| Ctrl+R | Replace: the same bar with a second row. |
| F3, Shift+F3 | Find Next and Find Previous. |
| Ctrl+F3 | Find Word at Caret. |

**Find** opens a bar over the code, not a dialog, with the selected text in it (if it is on one line) or
the word at the caret.  As you type, every match in the file is highlighted and marked on the scroll
bar, the first one from the caret is selected, and the bar says which it is of how many: `3 of 17`.

- **Enter** or **F3** goes to the next match and **Shift+Enter** or **Shift+F3** to the previous, round
  the ends of the file.
- **Escape** closes the bar and leaves the match selected, with the keys back in the editor.
- **Match Case**, **Words** (whole words only) and **Regex** change what matches.  They are the same in
  every editor's bar and are remembered.
- **In Selection** keeps the search to the text that was selected when the bar opened.  It is ticked
  for you when that was more than a line.
- Edit the file while the bar is open and it searches again.

**Replace** adds what to replace with, **Replace** (the current match, then on to the next) and
**Replace All** (every match, as one edit: one Undo puts them all back).

With the bar closed, **F3** and **Shift+F3** look again for what you last searched for, from the caret,
and **Ctrl+F3** looks for the word the caret is in.

### Regular expressions

With **Regex** ticked, here and in [Find in Files](#find-in-files), what you type is a regular
expression, as IntelliJ IDEA's:

| Written | Matches |
|---|---|
| `.` | any character but a line's end |
| `[abc]` `[a-z0-9_]` `[^abc]` | one of a set, a range, or one that isn't |
| `\d` `\w` `\s` | a digit, a word character, white space; `\D` `\W` `\S` one that isn't |
| `^` `$` | the start and the end of a line |
| `\b` | the start or end of a word |
| `(x)` `(?:x)` | a group, and one that isn't remembered |
| `x\|y` | x or y |
| `x*` `x+` `x?` `x{2,3}` | none or more, one or more, none or one, two to three; add `?` for as few as will do |
| `\1` | what the first group matched, again |
| `\.` `\(` `\\` | the character itself |

A `{` that doesn't start a count is just a `{`, so `foo\(\) {` finds what it looks like.  In the
replacement `$1` is what the first group matched (`$0` all of the match), `\n` a new line and `\t` a
tab.  What isn't a regular expression is said to be wrong, in the bar, and finds nothing.  An expression
that would take for ever on some text (`(a+)+b` on a long run of `a`) is given up on.

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
case, Words (whole words only) and Regex (a [regular expression](#regular-expressions)), a file mask,
and a preview of the first results as you type, each with its file's code under it.  Text selected in the editor (on one line) is what it looks for; otherwise what you looked
for last.  Enter opens the result selected.

**Open in Find Window** lists every result in the **Find** tool window beside Find Usages: a tree of the
files and their lines, with the same keys, preview and toolbar as Find Usages (without the groupings and
the filters).  Open files are searched as their editors have them, unsaved changes included.  The search
runs a little at a time, so the IDE never stops answering keys.

Each find is shown in the Find tool window's tab that is chosen, named for the text.  **Pin Tab**, on the
toolbar, keeps a tab as it is (a `*` goes before its name) and the next find gets a tab of its own.
**Rerun** looks again for the tab's own text, in its own scope.  A tab is closed by its cross.

On the **Directory** tab, the `...` button beside the directory opens a chooser of folders.

The **File mask** keeps the search to some of the scope's files: `*.cpp, *.h` is names with `*` and
`?`, parted by commas, whatever their case.  A `!` before one leaves its files out: `*.h, !Test*`.

### Replace in Files

Ctrl+Shift+R is the same dialog with **Replace with** (a regular expression's `$1` is a group).

- **Replace All** asks first, saying how many matches in how many files, then replaces them all.
- **Open in Find Window** lists the matches in the Find tool window, each with what it becomes after
  it:

    ```text
    Replace 'Area' with 'Size' in the solution  3 matches in 2 files
      Shapes.h  2 matches
        13: int  Area(void) override;  ->  Size
    ```

    Its toolbar's **Replace All** replaces them all, and **Replace Selected** the selected one, or every
    one in the selected file.

Each file is opened and changed in its editor, and all of them are one Undo.  They are left changed, not
saved: Save All (Ctrl+Shift+S) keeps them.  A match whose text has changed since it was found is left
alone, and the status line says how many were.

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
