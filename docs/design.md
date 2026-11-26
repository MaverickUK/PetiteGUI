# How PetiteGUI is built

Back to the [README](../README.md).

## The layers

```
main.c     events, commands, what changed and what to redraw
draw.c     the desktop's widgets, drawn from the state in App A
gfx.c      clipping, text, masks, save/restore: the only drawing calls draw.c makes
drv_*.c    one video mode's memory: fill, pattern, span, xor, save, restore, dump
mouse.c    INT 33h
games.c    the games list: PETITE.CFG, scanning for !GM_INFO.TXT, PETITE.IDX
launch.c   running a game as GameMinder does, and its play log
gminfo.c   GameMinder's formats and launch rules, pure C (host tested)
swap.c     EMS or the swap file, then swapcore.asm, which runs the game (or any program)
drives.c   the drives DOS has
field.c    the text-entry box's editing, pure C (host tested)
fileops.c  copying, moving, renaming and deleting files and folders
files.c    a directory window's listing (findfirst/findnext)
edit.c     the text editor: opening, saving, keys, the view
textbuf.c  its gap buffer, pure C (host tested)
saver.c    the screensavers: choosing, running, waking; savers_*.c the nine of them
saverlib.c their arithmetic, pure C (host tested): sine, Life, the maze, random numbers
```

A **driver** (`gfx.h`'s `Driver`) knows one mode's memory layout and nothing else; it gets coordinates already clipped to the screen. **gfx** clips everything to the current clip rectangle first, so the widgets can draw anywhere. A **theme** (`theme.c`) is the colour table the widgets use. Paper is `drv_cga` with `theme_paper`, Ink is `drv_ega` with `theme_ink`: every widget is shared.

## The video modes

**Paper, CGA mode 06h:** 640×200, 1 bit a pixel, 80 bytes a row, bit 7 the leftmost pixel. Even rows start at `B800:0000`, odd rows at `B800:2000` (two 8 KB banks), so a row's address is `(y & 1) * 0x2000 + (y >> 1) * 80`.

**Ink, EGA mode 0Eh:** 640×200 in 16 colours, as four bit planes at `A000:0000`, 80 bytes a row in each. The CPU sees one byte per 8 pixels, and the EGA's graphics controller (ports `3CEh`/`3CFh`) decides what a write does to the four planes:
- **Set/reset** holds a colour, and with it enabled one write paints that colour in all four planes.
- **The bit mask** picks which of the byte's 8 pixels change; the rest come from the **latches**, which the last read of that byte loaded. So a partial byte is read, then written.
- **The function select** can XOR instead of replace: the window outline while dragging.

Every driver function puts the registers back to the BIOS defaults when it's done, because the mouse driver draws the pointer assuming them.

## Byte-aligned windows

Window edges sit on multiples of 8 pixels (`SNAP8`), so a window's fills, copies and most of its text are whole bytes, with no shifting. Text at any x still works (`gfx_bits` shifts a 16-pixel span across up to 3 bytes); it's just slower.

## The pixels are tall

On a 4:3 monitor, a 640×200 pixel is 2.4 times taller than it is wide. So anything meant to look square is wider than it is tall in pixels: the scroll arrows are 16×7, the close box 12×5, the list icons are drawn 2 pixels wide per source pixel, and the mouse pointer uses every other row of a normal arrow.

## Text

The system BIOS keeps an 8×8 font for characters 0–127 at `F000:FA6E`, on every PC whatever the video card. PetiteGUI draws from it directly, so it needs no font file. Disabled text in Paper is dithered (every other pixel), since there's no grey.

## Redrawing

There's no off-screen buffer. When something changes, `redraw()` draws the rectangle that changed, back to front, clipped to it: the desktop pattern, the icons, the windows in z-order, the menu bar. A window moving redraws where it was and where it is.

What's drawn is kept to what changed, so the screen doesn't flash while it's covered and drawn again: if one window covers the whole rectangle, drawing starts from that window; selecting a row redraws the old and new rows; moving through a menu redraws the item left and the item reached.

Menus and alerts are different: they **save the pixels under them** (`gfx_save`) and put them back when they close, which is faster and needs no redraw of what's beneath. Paper's File menu, with its shadow, is about 1.9 KB; Ink's, in four planes, about 7.5 KB.

Dragging a window draws an XOR outline (drawn twice, it leaves the screen as it was), and only moves the window when the button is let go. Resizing works the same way from the size box: the outline's bottom-right corner follows the pointer, kept within `rect_resize`'s limits (no smaller than 176 x 72, on the screen, a whole number of bytes wide). A zoomed window remembers its size and place, to go back to.

## The mouse

The DOS mouse driver (`INT 33h`) reads the mouse and draws the pointer in modes 06h and 0Eh. PetiteGUI resets it after each mode change (function 0, so it learns the mode), sets the range to 640×200 and the pointer's shape (function 9), and hides the pointer around every drawing call (functions 2 and 1), since the driver saves and restores the pixels under it.

The program polls: `mouse_read` (function 3) for the position and buttons, `kbhit` for keys. A press acts at once; anything held (dragging, menus, scroll arrows) loops until the button is let go.

## Memory

Medium model: code in as many 64 KB segments as it needs (it passed 62 KB in one with the screensavers), and 64 KB of data with the stack. Open Watcom's startup shrinks the program's DOS memory block to just the program (about 66 KB with its 8 KB stack), so the rest of conventional memory is free.

Anything else PetiteGUI needs comes from DOS directly (`_dos_allocmem`): the pixels under a menu or alert, and the games list (500 games, 61 KB). Not from the C far heap: Open Watcom's `_ffree` keeps freed blocks for reuse, and a block kept would sit under a game while it runs.

## Running games with PetiteGUI swapped out

`src/swapcore.asm` is TurboPower's ExecWithSwap (1988, free with credit), by way of GameMinder's NASM port. The Makefile links it first, so it's the first code after the PSP, and everything after its `FirstToSave` label (the rest of the code, the data, the stack) can be moved out:

1. `swap.c` checks no interrupt vector points into that memory (it would crash the machine), then allocates EMS for it, or creates `SWAP.$$$` (hidden) in `%TEMP%`.
2. `swap_core` copies the program path and command line into its own code segment, switches to a stack there, copies everything from `FirstToSave` to the end of the block out, and shrinks the block to end at `FirstToSave` (DOS function 4Ah). About 1.8 KB, with the PSP, stays.
3. It runs the program (4B00h), grows the block back, copies everything back, and returns.

Before that, `run_game` frees the games list and any other DOS blocks, resets the mouse driver and returns to text mode; afterwards it sets the video mode again, re-initialises the mouse, reads the games list back from `PETITE.IDX` and redraws the desktop.

If a program leaves something resident (a TSR from a `before:` line, say) in the memory PetiteGUI needs back, the block can't grow again: the core says why and quits to DOS, as GameMinder does.

## Drives

`drives.c` finds the drives without reading any: DOS's filename parser (`INT 21h` function 29h) rejects a letter that isn't a drive, from its tables alone; MSCDEX (`INT 2Fh` AX=150Bh) knows its CD-ROMs; IOCTL calls (`INT 21h` AX=4409h, 4408h) say whether a drive is remote or removable. DOS answers for B: even with one floppy drive (it's A: again, asking for disks to be swapped), so B: shows only when the BIOS's equipment word (`INT 11h`) says there are two. The icons go in columns of five from the right.

There are four folder windows. A drive's icon brings up the window already showing it, else a closed one, else the one furthest back, which moves to that drive.

## Running programs

Games, programs double-clicked in a folder, Run… and the DOS prompt all go the same way: `leave_desktop` hands back every DOS block PetiteGUI holds outside its own (the games list, the editor's text), takes out its critical-error handler, resets the mouse and returns to text mode; `launch.c` runs the program with PetiteGUI swapped out; `return_desktop` puts it all back, reads the open folders again and redraws. Nothing runs while the editor has unsaved changes, since its text comes back from the disk.

An EXE or COM is run directly. A batch file, Run…'s command line and the DOS prompt go through `COMMAND.COM` (`/C`, or nothing for the prompt), which finds programs on the PATH and runs DOS's own commands; it stays in memory under the program, a few KB.

## Copying, moving and deleting

`fileops.c` does the work on full paths, folders and all, calling a progress function before each file (it draws the progress box, and Esc stops it):

- **Copying** goes through a 32 KB DOS block, freed afterwards; a copy keeps its file's date (`_dos_setftime`) and attributes, and one that fails part way is removed rather than left half-written. A folder copies by making the folder, then copying what's in it.
- **Moving** is a DOS rename where it can be: a file into any folder on the same drive, a folder within its own folder. Otherwise (another drive, or a folder to another folder) it's a copy, then a delete.
- **Deleting** clears read-only, hidden and system first (it's been confirmed), and empties a folder one entry at a time, starting its search again each time: removing entries while DOS is still searching can make it skip some.
- Nothing goes into itself (`path_inside`), and names are checked against the 8.3 rules (`path_valid_name`) before DOS is asked.

Dragging a row starts once the pointer moves a few pixels with the button down (less is a click); an XOR outline follows it. Let go on a folder window, a folder in one, or a drive's icon, and it moves (the same drive) or copies (another drive, or with Ctrl, read from the BIOS's keyboard flags at `0040:0017`).

## Folders

The A: and C: windows list a directory with DOS `findfirst`/`findnext`, up to 160 entries (the info line says "First 160 items" past that), folders first and `..` at the top in a subdirectory. The window's `path` ends with a backslash and is its title, cut from the left when it's too long.

While the desktop is up, PetiteGUI answers DOS's critical errors (`INT 24h`) with Fail, so an empty floppy drive makes the window say it isn't ready instead of DOS writing "Abort, Retry, Fail?" over the graphics. That handler is in PetiteGUI's memory, so it's taken out before a game runs and put back after.

## The text editor

One file at a time, up to 60,000 bytes, in a DOS memory block with 16 KB to grow. The text is a gap buffer (`textbuf.c`): the file's two halves sit at either end of the block with a gap at the caret, so typing moves only the text between this edit and the last one, never the whole file. CR LF becomes `\n` when the file's read, and goes back when it's saved, if the file had them.

The editor keeps its caret and view as character positions and line numbers, updated as it goes rather than counted from the start of the file at each key. A key redraws only the caret's old and new rows, unless the view scrolled or a line was added or removed.

Saving writes `PETITE.$$$` beside the file, then replaces the file with it, so a full disk leaves the file as it was. Before a game runs, the editor's text is freed (with any changes saved first: it won't run a game until they are) and read back after, at the same place.

## Screensavers

The main loop notes the BIOS tick count whenever the mouse moves, a button is down or a key arrives; once `saverdelay` minutes pass with none (and no menu or alert open) it runs the saver. `saver_run` calls the saver's `step` until there's a key (eaten), a button, or motion: the mouse driver's motion counters (`INT 33h` function `0Bh`) see a nudge too small to move the pointer. Then the desktop is redrawn from scratch, since saving Ink's 64 KB screen first would cost more than drawing it.

Each step is a frame, paced by the vertical retrace (port `3DAh` bit 3), so savers run at the same speed on fast machines and as fast as they can on slow ones. What each leans on:

| Saver | How |
|---|---|
| Colour cycle | A zone plate, every pixel's colour its squared distance from the middle in bands (worked out with additions only), written once a row at a time. After that only the palette moves: INT 10h AX=1002h for Ink's 16 registers, or Paper's one ink colour (the CGA's colour select register, port `3D9h`, or palette register 1 on an EGA or VGA). |
| Blank | The EGA attribute controller with its palette disconnected, or the CGA mode register with video off; the screen is cleared first, for cards that ignore both. |
| Starfield | 110 points in space, x/z and y/z on screen (y squashed for the tall pixels), 2-3 pixels wide. |
| Lines | Corners on Lissajous curves from a 256-step sine table (made with whole numbers by Taylor series: there may be no FPU); lines are XORed a row's run at a time, so the oldest of the trail can be drawn again to remove it. |
| Maze | A random depth-first search a step at a time, then another to solve it, on 16 x 7 blocks (square on the screen, and whole bytes). |
| Melt | Byte-wide columns moved down a few rows at a time: a memory copy per row on the CGA, write mode 1 (all four planes through the latches) on the EGA. |
| Life | 80 x 50 cells of 8 x 4 pixels; a cell starts alive where the screen is black at pixels the desktop pattern always lights, so windows, frames and text seed it. Only cells that change are drawn. |
| Game showcase | The games index (never a scan), each game's `!GM_INFO.TXT` and `!GM_LOG.TXT`, in the BIOS font scaled up. |
| Clock | Filled rectangles for segments, redrawn somewhere new each minute; the colon blinks with the seconds. |

## Games

`!GM_INFO.TXT` is GameMinder's: `key:value` lines, `;` for comments. PetiteGUI reads `name`, `year` and `genre` when scanning, and the whole file afresh when a game runs. The rules for running one are GameMinder's (`gminfo.c`):

- `exec:` is the program and its arguments; without a drive or leading backslash it's in the game's directory, which becomes the current one.
- A batch file can't be EXEC'd, so it runs as `COMSPEC /C file`.
- With `before:` or `after:`, the three run from one batch file (`PETITE.BAT`), the game with `CALL` if it's a batch file itself.
- Each play adds `>yyyy/mm/dd hh:mm` to `!GM_LOG.TXT` as it starts and `<minutes` when it ends.

Scanning also reads each game's `!GM_LOG.TXT`, so the index (`PETITE.IDX`, `key:value` like GameMinder's own cache) holds every detail and the play stats; after a game runs from PetiteGUI, its stats are read again and the index rewritten.

In memory the list is two DOS blocks: a small record per game (offsets, the play count, minutes, memory), and one pool of all the games' text, which the records point into. Fixed-size fields for every detail would put 500 games well past a 64 KB segment; as strings they take what they need. Sorting compares the records and pool strings where they are (`_fstricmp`), copying nothing.

The Games window's columns (`gamecols.c`, host tested) have a width in characters each; the window widens to fit those chosen, and any that still don't fit beside a 12-character name are left off from the right. A heading click sorts by it.

The scan walks every directory under each `ScanPath` entry. `PETITE.IDX` records the `ScanPath` it was made for, so changing `ScanPath` makes the next open scan again.

## Next

- Speed on a real 8088: the inner loops (fills, text, spans) into NASM, after measuring.
- More of GameMinder: sorting and filtering the games list, a game's details and play stats, adding and editing `!GM_INFO.TXT`.
- Resizing windows; more than one directory window per drive; copying, moving and new folders; deleting folders.
- The editor: selecting text, cut and paste, find, horizontal scrolling with the mouse.
- Arcade (CGA 320×200, 4 colours) and Bevel (EGA 640×350) as further drivers: see [style-studies.html](style-studies.html).
