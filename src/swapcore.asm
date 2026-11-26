; SWAPCORE.ASM - swap PetiteGUI out of memory, run a program, swap back in
;
; Based on EXECSWAP.ASM
;   Copyright (c) 1988 TurboPower Software
;   May be used freely as long as due credit is given
; by way of GameMinder's NASM port for Free Pascal (EXECSWAP/FPC/SWAPCORE.ASM), changed for
; Open Watcom C:
; - a near __cdecl function (the small model), taking C strings rather than Pascal ones;
; - it must be the first code in the program: the Makefile links SWAPCORE.OBJ first, so it
;   starts the _TEXT segment, right after the PSP. Everything from FirstToSave up (the rest of
;   the code, the data, the stack) is copied to EMS or a file, and the program's memory
;   block is shrunk to end at FirstToSave while the child runs, leaving about 1 KB resident.
;
; Assemble with: nasm -f obj -o SWAPCORE.OBJ SWAPCORE.ASM
;
; unsigned swap_core(const char far *path, const char far *cmdline, SwapInfo far *info);
;   path     the program to run, with its drive and directory
;   cmdline  its arguments (copied to the command tail with a leading space by the caller)
;   info     SwapInfo (swap.h; offsets below), with EMS or the swap file already allocated
;   returns 0, a DOS error code from EXEC, or 1 if swapping out failed
; unsigned swap_first_seg(void);
;   the segment of FirstToSave: everything from it to the end of the block is swapped

        cpu 8086

        segment _TEXT public align=16 class=CODE use16

        global _swap_core
        global _swap_first_seg

;-----------------------------------------------------------------------------
EmsPageSize     equ     16384           ;Size of EMS page
FileBlockSize   equ     32768           ;Size of a file block
StkSize         equ     256             ;Bytes in temporary stack

;SwapInfo (packed)
InfoBytesSwapped  equ   0               ;long - bytes to swap to EMS/disk
InfoEmsAllocated  equ   4               ;byte - 1 when EMS allocated for swap
InfoFileAllocated equ   5               ;byte - 1 when file allocated for swap
InfoEmsHandle     equ   6               ;word - handle of EMS allocation block
InfoFrameSeg      equ   8               ;word - segment of EMS page frame
InfoFileHandle    equ   10              ;word - DOS handle of swap file
InfoPrefixSeg     equ   12              ;word - PSP segment

;Parameters (near __cdecl: return address at bp+2)
ParamPath       equ     4
ParamCmdLine    equ     8
ParamInfo       equ     12

;-----------------------------------------------------------------------------
;Variables in CS - they stay resident, so they're before FirstToSave
UsedEms         db      0               ;1 if swapping to EMS, 0 if to file
EmsAllocCS      db      0
FileAllocCS     db      0
                align   2
BytesSwappedCS  dd      0               ;Bytes to move during a swap
EmsHandleCS     dw      0               ;EMS handle
FrameSegCS      dw      0               ;Segment of EMS page window
FileHandleCS    dw      0               ;DOS file handle
PrefixSegCS     dw      0               ;Segment of base of program
Status          dw      0               ;Status code
LeftToSwap      dd      0               ;Bytes left to move
SaveSP          dw      0               ;Original stack pointer
SaveSS          dw      0               ;Original stack segment
ParasWeHave     dw      0               ;Paragraphs allocated to process
CmdLine         times 128 db 0          ;Command tail passed to DOS: length, text, CR
PathBuf         times 64 db 0           ;ASCIIZ path passed to DOS
FileBlock1      times 16 db 0           ;FCB passed to DOS
FileBlock2      times 16 db 0           ;FCB passed to DOS
EnvironSeg      dw      0               ;Segment of environment for child
CmdLinePtr      dd      0               ;Pointer to command tail
FilePtr1        dd      0               ;Pointer to FCB 1
FilePtr2        dd      0               ;Pointer to FCB 2
TempStack       times StkSize db 0      ;Temporary stack
StackTop:                               ;Initial top of stack
NoRestoreMsg    db      13,10,'PetiteGUI could not get its memory back: a program left loaded in',13,10
                db      'memory (such as a TSR started by a before: command) is using it.',13,10
                db      'Unload it in an after: command, or load it before starting PetiteGUI.',13,10,'$'

;-----------------------------------------------------------------------------
;Macros
%macro InitSwapCount 0                  ;Initialize counter for bytes to swap
        mov     ax, [cs:BytesSwappedCS]
        mov     [cs:LeftToSwap], ax
        mov     ax, [cs:BytesSwappedCS+2]
        mov     [cs:LeftToSwap+2], ax
%endmacro

%macro SetSwapCount 1                   ;Return CX = bytes to move this block
        mov     cx, %1                  ;...and reduce total bytes left to move
        cmp     word [cs:LeftToSwap+2], 0 ;Is high word still non-zero?
        jnz     %%FullBlk               ;Jump if so
        cmp     word [cs:LeftToSwap], %1 ;Low word still a block or more?
        jae     %%FullBlk               ;Jump if so
        mov     cx, [cs:LeftToSwap]     ;Otherwise, move what's left
%%FullBlk:
        sub     [cs:LeftToSwap], cx     ;Reduce number left to move
        sbb     word [cs:LeftToSwap+2], 0
%endmacro

%macro NextBlock 2                      ;Point segment register to next block
        mov     ax, %1
        add     ax, %2/16               ;Add paragraphs to next segment
        mov     %1, ax                  ;Next block to move
        mov     ax, [cs:LeftToSwap]
        or      ax, [cs:LeftToSwap+2]   ;Bytes left to move?
%endmacro

%macro EmsCall 1                        ;Call EMM and prepare to check result
        mov     ah, %1
        int     67h
        or      ah, ah                  ;Error code in AH
%endmacro

%macro InitSwapFile 0                   ;Seek to start of swap file
        mov     bx, [cs:FileHandleCS]
        xor     cx, cx
        xor     dx, dx
        mov     ax, 4200h
        int     21h
%endmacro

%macro HaltWithError 1                  ;Halt if non-recoverable error occurs
        mov     ax, 4C00h + %1
        int     21h
%endmacro

%macro MoveFast 0                       ;Move CX bytes from DS:SI to ES:DI
        cld
        shr     cx, 1                   ;Convert to words
        rep     movsw                   ;Move the words
        rcl     cx, 1                   ;Get the odd byte, if any
        rep     movsb                   ;Move it
%endmacro

%macro SetTempStack 0                   ;Switch to temporary stack, leaves BX = CS
        mov     ax, StackTop
        mov     bx, cs
        cli
        mov     ss, bx
        mov     sp, ax
        sti
%endmacro

;-----------------------------------------------------------------------------
_swap_first_seg:
        push    cx
        mov     ax, FirstToSave         ;Paragraph aligned
        mov     cl, 4
        shr     ax, cl
        mov     cx, cs
        add     ax, cx
        pop     cx
        ret

;-----------------------------------------------------------------------------
_swap_core:
        push    bp
        mov     bp, sp
        push    bx
        push    cx
        push    dx
        push    si
        push    di
        push    ds
        push    es

        mov     word [cs:Status], 1     ;Assume failure
        mov     [cs:SaveSP], sp         ;Save stack position
        mov     [cs:SaveSS], ss

;Copy swap state to CS where we can reach it while swapped out
        les     di, [bp+ParamInfo]      ;ES:DI -> Info
        mov     ax, [es:di+InfoBytesSwapped]
        mov     [cs:BytesSwappedCS], ax
        mov     ax, [es:di+InfoBytesSwapped+2]
        mov     [cs:BytesSwappedCS+2], ax
        mov     ax, [es:di+InfoEmsHandle]
        mov     [cs:EmsHandleCS], ax
        mov     ax, [es:di+InfoFrameSeg]
        mov     [cs:FrameSegCS], ax
        mov     ax, [es:di+InfoFileHandle]
        mov     [cs:FileHandleCS], ax
        mov     ax, [es:di+InfoPrefixSeg]
        mov     [cs:PrefixSegCS], ax
        mov     al, [es:di+InfoEmsAllocated]
        mov     [cs:EmsAllocCS], al
        mov     al, [es:di+InfoFileAllocated]
        mov     [cs:FileAllocCS], al

;Copy the path and command line to CS while they're still in memory
        push    cs
        pop     es
        cld
        lds     si, [bp+ParamPath]      ;DS:SI -> ASCIIZ path
        mov     di, PathBuf             ;ES:DI -> local copy
        mov     cx, 63                  ;Truncate past the space set aside
.Path:  lodsb
        or      al, al
        jz      .PathEnd
        stosb
        loop    .Path
.PathEnd:
        xor     al, al
        stosb                           ;ASCIIZ terminate

        lds     si, [bp+ParamCmdLine]   ;DS:SI -> ASCIIZ arguments
        mov     di, CmdLine+1           ;ES:DI -> text of the command tail
        xor     cx, cx                  ;CX = length
.Cmd:   lodsb
        or      al, al
        jz      .CmdEnd
        cmp     cx, 126                 ;Truncate past the space set aside
        jae     .CmdEnd
        stosb
        inc     cx
        jmp     .Cmd
.CmdEnd:
        mov     al, 0Dh                 ;Terminate with ^M
        stosb
        mov     [cs:CmdLine], cl        ;Length first

        mov     es, [cs:PrefixSegCS]
        mov     ax, [es:002Ch]          ;Get environment segment
        mov     [cs:EnvironSeg], ax

        InitSwapCount                   ;Initialize bytes LeftToSwap

;Check for swapping to EMS or file
        cmp     byte [cs:EmsAllocCS], 0 ;Check flag for EMS method
        jz      NotEms                  ;Jump if EMS not used
        jmp     WriteE                  ;Swap to EMS
NotEms: cmp     byte [cs:FileAllocCS], 0 ;Check flag for swap file method
        jnz     WriteF                  ;Swap to file
        jmp     ESDone                  ;Exit if no swapping method set

;Write to swap file
WriteF: push    cs
        pop     ds                      ;DS = CS
        InitSwapFile                    ;Seek to start of swap file
        jnc     EF0                     ;Jump if success
        jmp     ESDone                  ;Exit if error
EF0:    SetSwapCount FileBlockSize      ;CX = bytes to write
        mov     dx, FirstToSave         ;DS:DX -> start of region to save
        mov     ah, 40h                 ;File write
        int     21h
        jc      EF1                     ;Jump if write error
        cmp     ax, cx                  ;All bytes written?
        jz      EF2                     ;Jump if so
EF1:    jmp     ESDone                  ;Exit if error
EF2:    NextBlock ds, FileBlockSize     ;Point DS to next block to write
        jnz     EF0                     ;Loop if bytes left to write
        mov     byte [cs:UsedEms], 0    ;Flag we used swap file for swapping
        mov     ah, 45h                 ;DUP handle
        int     21h
        jc      EF2a                    ;Jump if failed
        mov     bx, ax                  ;Prepare to close dup'ed handle
        mov     ah, 3Eh                 ;Flush swap file
        int     21h
EF2a:   jmp     SwapDone                ;Done swapping out

;Write to EMS
WriteE: mov     es, [cs:FrameSegCS]     ;ES -> page window
        mov     dx, [cs:EmsHandleCS]    ;DX = handle of our EMS block
        EmsCall 47h                     ;Save EMS context
        xor     bx, bx                  ;BX = initial logical page
        push    cs
        pop     ds                      ;DS = CS
EE0:    xor     al, al                  ;Physical page 0
        EmsCall 44h                     ;Map physical page
        jz      EE1                     ;Jump if success
        jmp     ESDone                  ;Exit if error
EE1:    SetSwapCount EmsPageSize        ;CX = Bytes to move
        xor     di, di                  ;ES:DI -> base of EMS page
        mov     si, FirstToSave         ;DS:SI -> region to save
        MoveFast                        ;Move CX bytes from DS:SI to ES:DI
        inc     bx                      ;Next logical page
        NextBlock ds, EmsPageSize       ;Point DS to next page to move
        jnz     EE0                     ;Loop if bytes left to move
        EmsCall 48h                     ;Restore EMS context
        mov     byte [cs:UsedEms], 1    ;Flag we used EMS for swapping

;Shrink memory allocated to this process
SwapDone:
        mov     ax, [cs:PrefixSegCS]
        mov     es, ax                  ;ES = segment of our memory block
        dec     ax
        mov     ds, ax                  ;DS = segment of memory control block
        mov     cx, [ds:0003h]          ;CX = current paragraphs owned
        mov     [cs:ParasWeHave], cx    ;Save current paragraphs owned
        SetTempStack                    ;Switch to temporary stack, BX = CS
        mov     ax, FirstToSave+15
        mov     cl, 4
        shr     ax, cl                  ;Convert offset to paragraphs
        add     bx, ax
        sub     bx, [cs:PrefixSegCS]    ;BX = new paragraphs to keep
        mov     ah, 4Ah                 ;SetBlock
        int     21h
        jnc     EX0                     ;Jump if successful
        jmp     EX5                     ;Swap back and exit

;Set up parameters and call DOS Exec
EX0:    push    cs
        pop     ds                      ;DS = CS
        push    cs
        pop     es                      ;ES = CS
        mov     si, CmdLine
        mov     [cs:CmdLinePtr], si
        mov     [cs:CmdLinePtr+2], ds   ;Store pointer to command line
        inc     si
        mov     di, FileBlock1
        mov     [cs:FilePtr1], di
        mov     [cs:FilePtr1+2], es     ;Store pointer to filename 1, if any
        mov     ax, 2901h               ;Parse FCB
        int     21h
        mov     di, FileBlock2
        mov     [cs:FilePtr2], di
        mov     [cs:FilePtr2+2], es     ;Store pointer to filename 2, if any
        mov     ax, 2901h               ;Parse FCB
        int     21h
        mov     dx, PathBuf
        mov     bx, EnvironSeg
        mov     ax, 4B00h               ;Exec
        int     21h
        jc      EX3                     ;Jump if error in DOS call
        xor     ax, ax                  ;Return zero for success
EX3:    mov     [cs:Status], ax         ;Save DOS error code

;Set up temporary stack and reallocate original memory block
        SetTempStack                    ;Set up temporary stack
        mov     es, [cs:PrefixSegCS]
        mov     bx, [cs:ParasWeHave]
        mov     ah, 4Ah                 ;SetBlock
        int     21h
        jnc     EX4                     ;Jump if no error
        push    cs
        pop     ds
        mov     dx, NoRestoreMsg        ;Explain why before halting
        mov     ah, 09h
        int     21h
        HaltWithError 0FFh              ;Must halt if failure here
EX4:    InitSwapCount                   ;Initialize LeftToSwap

;Check which swap method is in use
EX5:    cmp     byte [cs:UsedEms], 0
        jz      ReadF                   ;Jump to read back from file
        jmp     ReadE                   ;Read back from EMS

;Read back from swap file
ReadF:  push    cs
        pop     ds                      ;DS = CS
        InitSwapFile                    ;Seek to start of swap file
        jnc     EF3                     ;Jump if we succeeded
        HaltWithError 0FEh              ;Must halt if failure here
EF3:    SetSwapCount FileBlockSize      ;CX = bytes to read
        mov     dx, FirstToSave         ;DS:DX -> start of region to restore
        mov     ah, 3Fh                 ;Read file
        int     21h
        jnc     EF4                     ;Jump if no error
        HaltWithError 0FEh              ;Must halt if failure here
EF4:    cmp     ax, cx
        jz      EF5                     ;Jump if full block read
        HaltWithError 0FEh              ;Must halt if failure here
EF5:    NextBlock ds, FileBlockSize     ;Point DS to next page to read
        jnz     EF3                     ;Jump if bytes left to read
        jmp     ESDone                  ;We're done

;Copy back from EMS
ReadE:  mov     dx, [cs:EmsHandleCS]    ;DX = handle of our EMS block
        EmsCall 47h                     ;Save EMS context for next time
        mov     ds, [cs:FrameSegCS]     ;DS -> page window
        xor     bx, bx                  ;BX = initial logical page
        push    cs
        pop     es                      ;ES = CS
EE3:    xor     al, al                  ;Physical page 0
        EmsCall 44h                     ;Map physical page
        jz      EE4                     ;Jump if success
        HaltWithError 0FDh              ;Must halt if failure here
EE4:    SetSwapCount EmsPageSize        ;CX = Bytes to move
        xor     si, si                  ;DS:SI -> base of EMS page
        mov     di, FirstToSave         ;ES:DI -> region to restore
        MoveFast                        ;Move CX bytes from DS:SI to ES:DI
        inc     bx                      ;Next logical page
        NextBlock es, EmsPageSize       ;Point ES to next page to move
        jnz     EE3                     ;Jump if so
        EmsCall 48h                     ;Restore EMS context

ESDone: cli                             ;Switch back to original stack
        mov     ss, [cs:SaveSS]
        mov     sp, [cs:SaveSP]
        sti
        pop     es
        pop     ds
        pop     di
        pop     si
        pop     dx
        pop     cx
        pop     bx
        mov     ax, [cs:Status]         ;Return status
        pop     bp
        ret                             ;Near __cdecl: the caller removes the parameters

;-----------------------------------------------------------------------------
;Everything from here up is swapped out
        align   16
FirstToSave:
