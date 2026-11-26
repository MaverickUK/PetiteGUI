# Roadmap

What's next for PetiteGUI, and why. The aim is a desktop that does the basics well: looking after the file system, running software and editing common files. Anything else is better done by a DOS program run from it, so it stays small.

Each step is a minor version. The numbers are the items' order in the original list, so they stay put as things move between releases.

## 0.7: run anything, see every drive (done in 0.7.0)

- [x] **1. Run any program from a folder window.** Double-click an EXE, COM or BAT file (or Enter, or File › Open) to run it with PetiteGUI swapped out of memory, as games run now.
- [x] **2. Run with arguments.** File › Run… (Ctrl+R) asks for a command line, filled in with the selected program, and runs it in the folder's directory. It waits for a key afterwards, so a command's output can be read.
- [x] **3. DOS prompt.** File › DOS prompt opens `COMMAND.COM` with PetiteGUI swapped out, in the front folder's directory; `EXIT` comes back. It covers what PetiteGUI doesn't do itself (FORMAT, CHKDSK, SYS) without reimplementing those tools.
- [x] **4. A text-entry box.** One-line text in an alert, with a caret, Backspace and Delete, Home and End, and Enter or Esc. Run… needs it first; Rename, New folder, Copy, Move and Find use it later.
- [x] **7. Every drive.** An icon for each drive DOS has (B:, a CD-ROM, a network drive), found without touching a floppy drive, and B: only if there's a second floppy drive. Up to four folder windows open at once.

## 0.8: look after files (done in 0.8.0)

- [x] **5. Rename, New folder, and deleting folders.** Deleting a folder that isn't empty says how much it holds first ("Delete GAMES and its 214 files?").
- [x] **6. Copy and move,** from File › Copy… and Move…, or by dragging a file to another folder window or a drive icon. A move within a drive is a DOS rename, so it's instant; across drives it's a copy, then a delete, with progress shown.

## 0.9: edit common files

- [ ] **10. New file, and Save as.**
- [ ] **11. Find, and Find next** (Ctrl+F, F3).
- [ ] **12. Selecting text, and cut, copy and paste** (Shift+arrows; Ctrl+X, Ctrl+C, Ctrl+V), with the clipboard in a DOS block of its own.
- [ ] **13. AUTOEXEC.BAT and CONFIG.SYS** open in the editor on a double-click, rather than running or showing their size.

## 0.10: details

- [ ] **8. Dates, and selecting several files.** A date column, sorting by date, and Shift-click or Ctrl-click to select several files to copy, move or delete together.
- [ ] **9. File attributes** in Info: read-only, hidden, system and archive, with read-only and hidden switchable (a file that won't delete is often read-only).

## Later, if they earn their place

- Keyboard-only use: F10 or Alt+letter for the menus, Tab between windows.
- A system information window: the DOS version, conventional, EMS and XMS memory, each drive's free space.
- Finding files by name on a drive.

## Left out on purpose

Image viewers, a calculator, formatting disks (the DOS prompt runs FORMAT), long filenames, a Trash that can be emptied or undone (moving files between drives to keep them is more trouble than it saves), and theme editors.

## Done

The [CHANGELOG](CHANGELOG.md) has every release so far.
