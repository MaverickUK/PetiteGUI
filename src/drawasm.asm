; The drivers' innermost loop, text, in assembly: one row of 8 x 8 glyphs across a run of
; characters. Timing at PC/XT speed (tools/bench.sh) found text the biggest part of drawing
; a window or a menu, and the C version spent most of its time on far pointers and shifts.
;
; Each is a far __cdecl function (the medium model): arguments on the stack, [bp+6] onwards
; after the 4-byte return address; AX, BX, CX, DX and ES may change; SI, DI, BP and DS are
; kept. The string is near, in DGROUP; in the medium model SS is DGROUP too, so it's read
; with SS while DS holds the font's segment and ES the screen's.
;
; void text_row_cga(unsigned off, const char *s, int n, unsigned font_off, unsigned font_seg,
;                   int shift, int ink)
; void text_row_ega(unsigned off, const char *s, int n, unsigned font_off, unsigned font_seg,
;                   int shift)
;   off       the screen offset of the first character's byte in this row (B800h or A000h)
;   s, n      the characters (0-127)
;   font_off  the font's offset plus the row (0-7): a character's byte is at font_off + ch*8
;   shift     x & 7: each glyph byte spreads over two screen bytes, shifted right this much
;   ink       Paper only: 1 sets the glyph's pixels, 0 clears them
; The EGA one expects set/reset to hold the colour (drv_ega.c sets it, and puts the
; registers back after); it sets the bit mask for each screen byte and writes through the
; latches.

        segment drawasm_TEXT public align=16 class=CODE use16

        global _text_row_cga
        global _text_row_ega

%define ARG_OFF   [bp+6]
%define ARG_S     [bp+8]
%define ARG_N     [bp+10]
%define ARG_FONT  [bp+12]
%define ARG_FSEG  [bp+14]
%define ARG_SHIFT [bp+16]
%define ARG_INK   [bp+18]

_text_row_cga:
        push    bp
        mov     bp, sp
        push    si
        push    di
        push    ds
        mov     ax, 0B800h
        mov     es, ax
        mov     di, ARG_OFF
        mov     bx, ARG_S
        mov     dx, ARG_FONT
        mov     cl, ARG_SHIFT
        mov     si, ARG_N
        mov     ax, ARG_INK
        mov     ds, ARG_FSEG            ; from here, arguments only through bp (SS)
        or      si, si
        jz      .done
        or      ax, ax
        jz      .paper

.ink:   ; set the glyph's bits
        push    si                      ; the count, on the stack (SI is the glyph pointer)
.ink_next:
        mov     al, [ss:bx]             ; the character
        inc     bx
        xor     ah, ah
        shl     ax, 1                   ; * 8: its glyph
        shl     ax, 1
        shl     ax, 1
        add     ax, dx
        mov     si, ax
        mov     ah, [si]                ; this row of it, in the high byte
        or      ah, ah
        jz      .ink_skip
        xor     al, al
        shr     ax, cl                  ; AH: the left screen byte's bits, AL: the right's
        or      [es:di], ah
        or      al, al
        jz      .ink_skip
        or      [es:di+1], al
.ink_skip:
        inc     di
        dec     word [bp-8]             ; the count pushed above
        jnz     .ink_next
        pop     si
        jmp     .done

.paper: ; clear them
        push    si
.paper_next:
        mov     al, [ss:bx]
        inc     bx
        xor     ah, ah
        shl     ax, 1
        shl     ax, 1
        shl     ax, 1
        add     ax, dx
        mov     si, ax
        mov     ah, [si]
        or      ah, ah
        jz      .paper_skip
        xor     al, al
        shr     ax, cl
        not     ax
        and     [es:di], ah
        cmp     al, 0FFh
        je      .paper_skip
        and     [es:di+1], al
.paper_skip:
        inc     di
        dec     word [bp-8]
        jnz     .paper_next
        pop     si

.done:  pop     ds
        pop     di
        pop     si
        pop     bp
        retf

_text_row_ega:
        push    bp
        mov     bp, sp
        push    si
        push    di
        push    ds
        mov     ax, 0A000h
        mov     es, ax
        mov     di, ARG_OFF
        mov     bx, ARG_S
        mov     cl, ARG_SHIFT
        mov     si, ARG_N
        mov     ds, ARG_FSEG
        or      si, si
        jz      .done
        push    si                      ; the count, at [bp-8]
        mov     dx, 3CEh                ; the graphics controller: index 8 is the bit mask
.next:
        mov     al, [ss:bx]
        inc     bx
        xor     ah, ah
        shl     ax, 1
        shl     ax, 1
        shl     ax, 1
        add     ax, ARG_FONT
        mov     si, ax
        mov     ah, [si]
        or      ah, ah
        jz      .skip
        xor     al, al
        shr     ax, cl                  ; AH: the left byte's pixels, AL: the right's
        mov     ch, al                  ; keep the right's
        mov     al, 8
        out     dx, ax                  ; bit mask = the left's pixels
        mov     al, [es:di]             ; load the latches (the pixels to keep)
        mov     byte [es:di], 0FFh      ; write: set/reset's colour where the mask says
        or      ch, ch
        jz      .skip
        mov     ah, ch
        mov     al, 8
        out     dx, ax
        mov     al, [es:di+1]
        mov     byte [es:di+1], 0FFh
.skip:
        inc     di
        dec     word [bp-8]
        jnz     .next
        pop     si

.done:  pop     ds
        pop     di
        pop     si
        pop     bp
        retf
