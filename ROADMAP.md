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

## 0.9: edit common files (done in 0.9.0)

- [x] **10. New file, and Save as.** File › New text file (Ctrl+N) starts an Untitled one; its first save, or Save as…, asks where.
- [x] **11. Find, and Find next** (Ctrl+F, F3), ignoring case, going round to the top.
- [x] **12. Selecting text, and cut, copy and paste** (Shift+arrows; Ctrl+X, Ctrl+C, Ctrl+V), with the clipboard in a DOS block of its own.
- [x] **13. AUTOEXEC.BAT and CONFIG.SYS** open in the editor on a double-click, rather than running or showing their size.

## 0.10: details

- [ ] **8. Dates, and selecting several files.** A date column, sorting by date, and Shift-click or Ctrl-click to select several files to copy, move or delete together.
- [ ] **9. File attributes** in Info: read-only, hidden, system and archive, with read-only and hidden switchable (a file that won't delete is often read-only).

## Alongside: smoother, faster drawing

0.8.1 began this: menus and lists redraw only the items that change, and a redraw inside one window starts from that window. The rest, roughly in the order that does most for the flicker people see and the speed of a real 8088 or 286:

- [ ] **14. Draw less, and draw it faster.**
  - **Scroll by moving pixels:** a list or the editor scrolling a line copies what's already on the screen up or down (a byte copy per row on the CGA, the latches on the EGA) and draws only the new line, rather than redrawing the window.
  - **The pointer only where it's needed:** hide it (INT 33h function 10h) only over the rectangle being drawn, rather than everywhere, so it stops blinking while things change elsewhere.
  - **Typing in the editor:** redraw from the caret to the end of its line, not the whole line.
  - **Opening and closing windows:** draw a window once, in its final place, with nothing drawn and then covered.
  - **The inner loops in assembly:** fills, text and spans in NASM, after timing them on a slow machine (or DOSBox at XT speed) to find what's worth it.
  - **Waiting for the retrace** before larger redraws, if tearing shows on real hardware.

## Later, if they earn their place

- Keyboard-only use: F10 or Alt+letter for the menus, Tab between windows.
- A system information window: the DOS version, conventional, EMS and XMS memory, each drive's free space.
- Finding files by name on a drive.

## Left out on purpose

Image viewers, a calculator, formatting disks (the DOS prompt runs FORMAT), long filenames, a Trash that can be emptied or undone (moving files between drives to keep them is more trouble than it saves), and theme editors.

## Done

The [CHANGELOG](CHANGELOG.md) has every release so far.
