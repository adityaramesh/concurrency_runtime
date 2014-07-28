       page    ,132
        title   memcpy - Copy source memory bytes to destination
;***
;memcpy.asm - contains memcpy and memmove routines
;
;       Copyright (c) Microsoft Corporation. All rights reserved.
;
;Purpose:
;       memcpy() copies a source memory buffer to a destination buffer.
;       Overlapping buffers are not treated specially, so propogation may occur.
;       memmove() copies a source memory buffer to a destination buffer.
;       Overlapping buffers are treated specially, to avoid propogation.
;
;*******************************************************************************

        .xlist
        include cruntime.inc
        .list
        .xmm

M_EXIT  macro
        ret                     ; _cdecl return
        endm    ; M_EXIT

PALIGN_memcpy macro d
MovPalign&d&:
        movdqa      xmm1,xmmword ptr [esi-d]
        lea         esi, byte ptr [esi-d]
    align   @WordSize
PalignLoop&d&:
        movdqa  xmm3,xmmword ptr [esi+10h]
        sub     ecx,30h
        movdqa  xmm0,xmmword ptr [esi+20h]
        movdqa  xmm5,xmmword ptr [esi+30h]
        lea     esi, xmmword ptr [esi+30h]
        cmp     ecx,30h
        movdqa  xmm2,xmm3

        palignr xmm3,xmm1,d

        movdqa  xmmword ptr [edi],xmm3
        movdqa  xmm4,xmm0

        palignr xmm0,xmm2,d

        movdqa  xmmword ptr [edi+10h],xmm0
        movdqa  xmm1,xmm5

        palignr xmm5,xmm4,d

        movdqa  xmmword ptr [edi+20h],xmm5
        lea     edi, xmmword ptr [edi+30h]
        jge     PalignLoop&d&
        lea     esi, xmmword ptr [esi+d]

        endm    ; PALIGN_memcpy

        CODESEG

    extrn   __isa_available:dword
    extrn   __isa_enabled:dword
    extrn   __favor:dword

page
;***
;memcpy - Copy source buffer to destination buffer
;
;Purpose:
;       memcpy() copies a source memory buffer to a destination memory buffer.
;       This routine does NOT recognize overlapping buffers, and thus can lead
;       to propogation.
;       For cases where propogation must be avoided, memmove() must be used.
;
;       Algorithm:
;
;           Same as memmove. See Below
;
;
;memmove - Copy source buffer to destination buffer
;
;Purpose:
;       memmove() copies a source memory buffer to a destination memory buffer.
;       This routine recognize overlapping buffers to avoid propogation.
;       For cases where propogation is not a problem, memcpy() can be used.
;
;   Algorithm:
;
;       void * memmove(void * dst, void * src, size_t count)
;       {
;               void * ret = dst;
;
;               if (dst <= src || dst >= (src + count)) {
;                       /*
;                        * Non-Overlapping Buffers
;                        * copy from lower addresses to higher addresses
;                        */
;                       while (count--)
;                               *dst++ = *src++;
;                       }
;               else {
;                       /*
;                        * Overlapping Buffers
;                        * copy from higher addresses to lower addresses
;                        */
;                       dst += count - 1;
;                       src += count - 1;
;
;                       while (count--)
;                               *dst-- = *src--;
;                       }
;
;               return(ret);
;       }
;
;
;Entry:
;       void *dst = pointer to destination buffer
;       const void *src = pointer to source buffer
;       size_t count = number of bytes to copy
;
;Exit:
;       Returns a pointer to the destination buffer in AX/DX:AX
;
;Uses:
;       CX, DX
;
;Exceptions:
;*******************************************************************************

ifdef MEM_MOVE
        _MEM_     equ <memmove>
else  ; MEM_MOVE
        _MEM_     equ <memcpy>
endif  ; MEM_MOVE

%       public  _MEM_
_MEM_   proc \
        dst:ptr byte, \
        src:ptr byte, \
        count:IWORD

        ; destination pointer
        ; source pointer
        ; number of bytes to copy

        OPTION PROLOGUE:NONE, EPILOGUE:NONE

        push    edi             ;U - save edi
        push    esi             ;V - save esi

;                   size param/4   prolog byte  #reg saved
        .FPO ( 0, 3           , $-_MEM_     , 2, 0, 0 )

        mov     esi,[esp + 010h]     ;U - esi = source
        mov     ecx,[esp + 014h]     ;V - ecx = number of bytes to move
        mov     edi,[esp + 0Ch]      ;U - edi = dest

;
; Check for overlapping buffers:
;       If (dst <= src) Or (dst >= src + Count) Then
;               Do normal (Upwards) Copy
;       Else
;               Do Downwards Copy to avoid propagation
;

        mov     eax,ecx         ;V - eax = byte count...

        mov     edx,ecx         ;U - edx = byte count...
        add     eax,esi         ;V - eax = point past source end

        cmp     edi,esi         ;U - dst <= src ?
        jbe     short CopyUp    ;V - yes, copy toward higher addresses

        cmp     edi,eax         ;U - dst < (src + count) ?
        jb      CopyDown        ;V - yes, copy toward lower addresses

;
; Copy toward higher addresses.
;
CopyUp:
;
        ; See if Enhanced Fast Strings is supported.
        ; ENFSTRG supported?
        bt      __favor, __FAVOR_ENFSTRG
        jnc     CopyUpSSE2Check                 ; no jump
        ;
        ; use Enhanced Fast Strings
        rep     movsb
        jmp     TrailUp0         ; Done
CopyUpSSE2Check:
;
; Next, see if we can use a "fast" copy SSE2 routine
        ; block size greater than min threshold?
        cmp     ecx,080h
        jb      Dword_align  ; length too small go use dwords
        ; alignments equal?
        mov     eax,edi
        xor     eax,esi
        test    eax,15
        jne     AtomChk   ; Not aligned go check Atom
        bt      __isa_enabled, __ISA_AVAILABLE_SSE2
        jc      VEC_memcpy ; yes, go SSE2 copy (params already set)
AtomChk:
        ; Is Atom supported?
        bt      __favor, __FAVOR_ATOM
        jnc     Dword_align ; no,jump

        ; check if dst is 4 byte aligned
        test    edi, 3
        jne     CopyLeadUp

        ; check if src is 4 byte aligned
        test    esi, 3
        jne     Dword_align_Ok

; A software pipelining vectorized memcpy loop using PALIGN instructions

; (1) copy the first bytes to align dst up to the nearest 16-byte boundary
; 4 byte align -> 12 byte copy, 8 byte align -> 8 byte copy, 12 byte align -> 4 byte copy
PalignHead4:
        bt      edi, 2
        jae     PalignHead8
        mov     eax, dword ptr [esi]
        sub     ecx, 4
        lea     esi, byte ptr [esi+4]
        mov     dword ptr [edi], eax
        lea     edi, byte ptr [edi+4]

PalignHead8:
        bt      edi, 3
        jae     PalignLoop
        movq    xmm1, qword ptr [esi]
        sub     ecx, 8
        lea     esi, byte ptr [esi+8]
        movq    qword ptr [edi], xmm1
        lea     edi, byte ptr [edi+8]

;(2) Use SSE palign loop
PalignLoop:
        test    esi, 7
        je      MovPalign8
        bt      esi, 3
        jae     MovPalign4

PALIGN_memcpy 12
        jmp     PalignTail

PALIGN_memcpy 8
        jmp     PalignTail

PALIGN_memcpy 4

;(3) Copy the tailing bytes.
PalignTail:
        cmp    ecx,10h
        jl     PalignTail4
        movdqu xmm1,xmmword ptr [esi]
        sub    ecx, 10h
        lea    esi, xmmword ptr [esi+10h]
        movdqa xmmword ptr [edi],xmm1
        lea    edi, xmmword ptr [edi+10h]
        jmp    PalignTail

PalignTail4:
        bt      ecx, 2
        jae     PalignTail8
        mov     eax, dword ptr [esi]
        sub     ecx,4
        lea     esi, byte ptr [esi+4]
        mov     dword ptr [edi], eax
        lea     edi, byte ptr [edi+4]

PalignTail8:
        bt      ecx, 3
        jae     PalignTailLE3
        movq    xmm1, qword ptr [esi]
        sub     ecx,8
        lea     esi, byte ptr [esi+8]
        movq    qword ptr [edi], xmm1
        lea     edi, byte ptr [edi+8]

PalignTailLE3:
        mov     eax, dword ptr TrailUpVec[ecx*4]
        jmp     eax

; The algorithm for forward moves is to align the destination to a dword
; boundary and so we can move dwords with an aligned destination.  This
; occurs in 3 steps.
;
;   - move x = ((4 - Dest & 3) & 3) bytes
;   - move y = ((L-x) >> 2) dwords
;   - move (L - x - y*4) bytes
;

Dword_align:
        test    edi,11b         ;U - destination dword aligned?
        jnz     short CopyLeadUp ;V - if we are not dword aligned already, align
Dword_align_Ok:
        shr     ecx,2           ;U - shift down to dword count
        and     edx,11b         ;V - trailing byte count

        cmp     ecx,8           ;U - test if small enough for unwind copy
        jb      short CopyUnwindUp ;V - if so, then jump

        rep     movsd           ;N - move all of our dwords

        jmp     dword ptr TrailUpVec[edx*4] ;N - process trailing bytes

;
; Code to do optimal memory copies for non-dword-aligned destinations.
;

; The following length check is done for two reasons:
;
;    1. to ensure that the actual move length is greater than any possiale
;       alignment move, and
;
;    2. to skip the multiple move logic for small moves where it would
;       be faster to move the bytes with one instruction.
;

        align   @WordSize
CopyLeadUp:

        mov     eax,edi         ;U - get destination offset
        mov     edx,11b         ;V - prepare for mask

        sub     ecx,4           ;U - check for really short string - sub for adjust
        jb      short ByteCopyUp ;V - branch to just copy bytes

        and     eax,11b         ;U - get offset within first dword
        add     ecx,eax         ;V - update size after leading bytes copied

        jmp     dword ptr LeadUpVec[eax*4-4] ;N - process leading bytes

        align   @WordSize
ByteCopyUp:
        jmp     dword ptr TrailUpVec[ecx*4+16] ;N - process just bytes

        align   @WordSize
CopyUnwindUp:
        jmp     dword ptr UnwindUpVec[ecx*4] ;N - unwind dword copy

        align   @WordSize
LeadUpVec       dd      LeadUp1, LeadUp2, LeadUp3

        align   @WordSize
LeadUp1:
        and     edx,ecx         ;U - trailing byte count
        mov     al,[esi]        ;V - get first byte from source

        mov     [edi],al        ;U - write second byte to destination
        mov     al,[esi+1]      ;V - get second byte from source

        mov     [edi+1],al      ;U - write second byte to destination
        mov     al,[esi+2]      ;V - get third byte from source

        shr     ecx,2           ;U - shift down to dword count
        mov     [edi+2],al      ;V - write third byte to destination

        add     esi,3           ;U - advance source pointer
        add     edi,3           ;V - advance destination pointer

        cmp     ecx,8           ;U - test if small enough for unwind copy
        jb      short CopyUnwindUp ;V - if so, then jump

        rep     movsd           ;N - move all of our dwords

        jmp     dword ptr TrailUpVec[edx*4] ;N - process trailing bytes

        align   @WordSize
LeadUp2:
        and     edx,ecx         ;U - trailing byte count
        mov     al,[esi]        ;V - get first byte from source

        mov     [edi],al        ;U - write second byte to destination
        mov     al,[esi+1]      ;V - get second byte from source

        shr     ecx,2           ;U - shift down to dword count
        mov     [edi+1],al      ;V - write second byte to destination

        add     esi,2           ;U - advance source pointer
        add     edi,2           ;V - advance destination pointer

        cmp     ecx,8           ;U - test if small enough for unwind copy
        jb      short CopyUnwindUp ;V - if so, then jump

        rep     movsd           ;N - move all of our dwords

        jmp     dword ptr TrailUpVec[edx*4] ;N - process trailing bytes

        align   @WordSize
LeadUp3:
        and     edx,ecx         ;U - trailing byte count
        mov     al,[esi]        ;V - get first byte from source

        mov     [edi],al        ;U - write second byte to destination
        add     esi,1           ;V - advance source pointer

        shr     ecx,2           ;U - shift down to dword count
        add     edi,1           ;V - advance destination pointer

        cmp     ecx,8           ;U - test if small enough for unwind copy
        jb      short CopyUnwindUp ;V - if so, then jump

        rep     movsd           ;N - move all of our dwords

        jmp     dword ptr TrailUpVec[edx*4] ;N - process trailing bytes

        align   @WordSize
UnwindUpVec     dd      UnwindUp0, UnwindUp1, UnwindUp2, UnwindUp3
                dd      UnwindUp4, UnwindUp5, UnwindUp6, UnwindUp7

UnwindUp7:
        mov     eax,[esi+ecx*4-28] ;U - get dword from source
                                   ;V - spare
        mov     [edi+ecx*4-28],eax ;U - put dword into destination
UnwindUp6:
        mov     eax,[esi+ecx*4-24] ;U(entry)/V(not) - get dword from source
                                   ;V(entry) - spare
        mov     [edi+ecx*4-24],eax ;U - put dword into destination
UnwindUp5:
        mov     eax,[esi+ecx*4-20] ;U(entry)/V(not) - get dword from source
                                   ;V(entry) - spare
        mov     [edi+ecx*4-20],eax ;U - put dword into destination
UnwindUp4:
        mov     eax,[esi+ecx*4-16] ;U(entry)/V(not) - get dword from source
                                   ;V(entry) - spare
        mov     [edi+ecx*4-16],eax ;U - put dword into destination
UnwindUp3:
        mov     eax,[esi+ecx*4-12] ;U(entry)/V(not) - get dword from source
                                   ;V(entry) - spare
        mov     [edi+ecx*4-12],eax ;U - put dword into destination
UnwindUp2:
        mov     eax,[esi+ecx*4-8] ;U(entry)/V(not) - get dword from source
                                  ;V(entry) - spare
        mov     [edi+ecx*4-8],eax ;U - put dword into destination
UnwindUp1:
        mov     eax,[esi+ecx*4-4] ;U(entry)/V(not) - get dword from source
                                  ;V(entry) - spare
        mov     [edi+ecx*4-4],eax ;U - put dword into destination

        lea     eax,[ecx*4]     ;V - compute update for pointer

        add     esi,eax         ;U - update source pointer
        add     edi,eax         ;V - update destination pointer
UnwindUp0:
        jmp     dword ptr TrailUpVec[edx*4] ;N - process trailing bytes

;-----------------------------------------------------------------------------

        align   @WordSize
TrailUpVec      dd      TrailUp0, TrailUp1, TrailUp2, TrailUp3

        align   @WordSize
TrailUp0:
        mov     eax,[esp + 0Ch] ;U - return pointer to destination
        pop     esi             ;V - restore esi
        pop     edi             ;U - restore edi
                                ;V - spare
        M_EXIT

        align   @WordSize
TrailUp1:
        mov     al,[esi]        ;U - get byte from source
                                ;V - spare
        mov     [edi],al        ;U - put byte in destination
        mov     eax,[esp + 0Ch] ;V - return pointer to destination
        pop     esi             ;U - restore esi
        pop     edi             ;V - restore edi
        M_EXIT

        align   @WordSize
TrailUp2:
        mov     al,[esi]        ;U - get first byte from source
                                ;V - spare
        mov     [edi],al        ;U - put first byte into destination
        mov     al,[esi+1]      ;V - get second byte from source
        mov     [edi+1],al      ;U - put second byte into destination
        mov     eax,[esp + 0Ch] ;V - return pointer to destination
        pop     esi             ;U - restore esi
        pop     edi             ;V - restore edi
        M_EXIT

        align   @WordSize
TrailUp3:
        mov     al,[esi]        ;U - get first byte from source
                                ;V - spare
        mov     [edi],al        ;U - put first byte into destination
        mov     al,[esi+1]      ;V - get second byte from source
        mov     [edi+1],al      ;U - put second byte into destination
        mov     al,[esi+2]      ;V - get third byte from source
        mov     [edi+2],al      ;U - put third byte into destination
        mov     eax,[esp + 0Ch] ;V - return pointer to destination
        pop     esi             ;U - restore esi
        pop     edi             ;V - restore edi
        M_EXIT

;-----------------------------------------------------------------------------
;-----------------------------------------------------------------------------
;-----------------------------------------------------------------------------

;
; Copy down to avoid propogation in overlapping buffers.
;
        align   @WordSize
CopyDown:
        lea     esi,[esi+ecx-4] ;U - point to 4 bytes before src buffer end
        lea     edi,[edi+ecx-4] ;V - point to 4 bytes before dest buffer end
;
; See if the destination start is dword aligned
;

        test    edi,11b         ;U - test if dword aligned
        jnz     short CopyLeadDown ;V - if not, jump

        shr     ecx,2           ;U - shift down to dword count
        and     edx,11b         ;V - trailing byte count

        cmp     ecx,8           ;U - test if small enough for unwind copy
        jb      short CopyUnwindDown ;V - if so, then jump

        std                     ;N - set direction flag
        rep     movsd           ;N - move all of our dwords
        cld                     ;N - clear direction flag back

        jmp     dword ptr TrailDownVec[edx*4] ;N - process trailing bytes

        align   @WordSize
CopyUnwindDown:
        neg     ecx             ;U - negate dword count for table merging
                                ;V - spare

        jmp     dword ptr UnwindDownVec[ecx*4+28] ;N - unwind copy

        align   @WordSize
CopyLeadDown:

        mov     eax,edi         ;U - get destination offset
        mov     edx,11b         ;V - prepare for mask

        cmp     ecx,4           ;U - check for really short string
        jb      short ByteCopyDown ;V - branch to just copy bytes

        and     eax,11b         ;U - get offset within first dword
        sub     ecx,eax         ;U - to update size after lead copied

        jmp     dword ptr LeadDownVec[eax*4-4] ;N - process leading bytes

        align   @WordSize
ByteCopyDown:
        jmp     dword ptr TrailDownVec[ecx*4] ;N - process just bytes

        align   @WordSize
LeadDownVec     dd      LeadDown1, LeadDown2, LeadDown3

        align   @WordSize
LeadDown1:
        mov     al,[esi+3]      ;U - load first byte
        and     edx,ecx         ;V - trailing byte count

        mov     [edi+3],al      ;U - write out first byte
        sub     esi,1           ;V - point to last src dword

        shr     ecx,2           ;U - shift down to dword count
        sub     edi,1           ;V - point to last dest dword

        cmp     ecx,8           ;U - test if small enough for unwind copy
        jb      short CopyUnwindDown ;V - if so, then jump

        std                     ;N - set direction flag
        rep     movsd           ;N - move all of our dwords
        cld                     ;N - clear direction flag

        jmp     dword ptr TrailDownVec[edx*4] ;N - process trailing bytes

        align   @WordSize
LeadDown2:
        mov     al,[esi+3]      ;U - load first byte
        and     edx,ecx         ;V - trailing byte count

        mov     [edi+3],al      ;U - write out first byte
        mov     al,[esi+2]      ;V - get second byte from source

        shr     ecx,2           ;U - shift down to dword count
        mov     [edi+2],al      ;V - write second byte to destination

        sub     esi,2           ;U - point to last src dword
        sub     edi,2           ;V - point to last dest dword

        cmp     ecx,8           ;U - test if small enough for unwind copy
        jb      short CopyUnwindDown ;V - if so, then jump

        std                     ;N - set direction flag
        rep     movsd           ;N - move all of our dwords
        cld                     ;N - clear direction flag

        jmp     dword ptr TrailDownVec[edx*4] ;N - process trailing bytes

        align   @WordSize
LeadDown3:
        mov     al,[esi+3]      ;U - load first byte
        and     edx,ecx         ;V - trailing byte count

        mov     [edi+3],al      ;U - write out first byte
        mov     al,[esi+2]      ;V - get second byte from source

        mov     [edi+2],al      ;U - write second byte to destination
        mov     al,[esi+1]      ;V - get third byte from source

        shr     ecx,2           ;U - shift down to dword count
        mov     [edi+1],al      ;V - write third byte to destination

        sub     esi,3           ;U - point to last src dword
        sub     edi,3           ;V - point to last dest dword

        cmp     ecx,8           ;U - test if small enough for unwind copy
        jb      CopyUnwindDown  ;V - if so, then jump

        std                     ;N - set direction flag
        rep     movsd           ;N - move all of our dwords
        cld                     ;N - clear direction flag

        jmp     dword ptr TrailDownVec[edx*4] ;N - process trailing bytes

;------------------------------------------------------------------

        align   @WordSize
UnwindDownVec   dd      UnwindDown7, UnwindDown6, UnwindDown5, UnwindDown4
                dd      UnwindDown3, UnwindDown2, UnwindDown1, UnwindDown0

UnwindDown7:
        mov     eax,[esi+ecx*4+28] ;U - get dword from source
                                   ;V - spare
        mov     [edi+ecx*4+28],eax ;U - put dword into destination
UnwindDown6:
        mov     eax,[esi+ecx*4+24] ;U(entry)/V(not) - get dword from source
                                   ;V(entry) - spare
        mov     [edi+ecx*4+24],eax ;U - put dword into destination
UnwindDown5:
        mov     eax,[esi+ecx*4+20] ;U(entry)/V(not) - get dword from source
                                   ;V(entry) - spare
        mov     [edi+ecx*4+20],eax ;U - put dword into destination
UnwindDown4:
        mov     eax,[esi+ecx*4+16] ;U(entry)/V(not) - get dword from source
                                   ;V(entry) - spare
        mov     [edi+ecx*4+16],eax ;U - put dword into destination
UnwindDown3:
        mov     eax,[esi+ecx*4+12] ;U(entry)/V(not) - get dword from source
                                   ;V(entry) - spare
        mov     [edi+ecx*4+12],eax ;U - put dword into destination
UnwindDown2:
        mov     eax,[esi+ecx*4+8] ;U(entry)/V(not) - get dword from source
                                   ;V(entry) - spare
        mov     [edi+ecx*4+8],eax ;U - put dword into destination
UnwindDown1:
        mov     eax,[esi+ecx*4+4] ;U(entry)/V(not) - get dword from source
                                  ;V(entry) - spare
        mov     [edi+ecx*4+4],eax ;U - put dword into destination

        lea     eax,[ecx*4]     ;V - compute update for pointer

        add     esi,eax         ;U - update source pointer
        add     edi,eax         ;V - update destination pointer
UnwindDown0:
        jmp     dword ptr TrailDownVec[edx*4] ;N - process trailing bytes

;-----------------------------------------------------------------------------

        align   @WordSize
TrailDownVec    dd      TrailDown0, TrailDown1, TrailDown2, TrailDown3

        align   @WordSize
TrailDown0:
        mov     eax,[esp + 0Ch] ;U - return pointer to destination
                                ;V - spare
        pop     esi             ;U - restore esi
        pop     edi             ;V - restore edi
        M_EXIT

        align   @WordSize
TrailDown1:
        mov     al,[esi+3]      ;U - get byte from source
                                ;V - spare
        mov     [edi+3],al      ;U - put byte in destination
        mov     eax,[esp + 0Ch] ;V - return pointer to destination
        pop     esi             ;U - restore esi
        pop     edi             ;V - restore edi
        M_EXIT

        align   @WordSize
TrailDown2:
        mov     al,[esi+3]      ;U - get first byte from source
                                ;V - spare
        mov     [edi+3],al      ;U - put first byte into destination
        mov     al,[esi+2]      ;V - get second byte from source
        mov     [edi+2],al      ;U - put second byte into destination
        mov     eax,[esp + 0Ch] ;V - return pointer to destination
        pop     esi             ;U - restore esi
        pop     edi             ;V - restore edi
        M_EXIT

        align   @WordSize
TrailDown3:
        mov     al,[esi+3]      ;U - get first byte from source
                                ;V - spare
        mov     [edi+3],al      ;U - put first byte into destination
        mov     al,[esi+2]      ;V - get second byte from source
        mov     [edi+2],al      ;U - put second byte into destination
        mov     al,[esi+1]      ;V - get third byte from source
        mov     [edi+1],al      ;U - put third byte into destination
        mov     eax,[esp + 0Ch] ;V - return pointer to destination
        pop     esi             ;U - restore esi
        pop     edi             ;V - restore edi
        M_EXIT


align       16
VEC_memcpy:
        push        edi         ; save dst for returning
        mov         eax, esi
        and         eax, 0Fh
        ; eax = src and dst alignment (src mod 16)
        test        eax, eax
        jne         L_Notaligned

        ; in:
        ;  edi = dst (16 byte aligned)
        ;  esi = src (16 byte aligned)
        ;  ecx = len is >= (128 - head alignment bytes)
        ; do block copy using SSE2 stores
L_Aligned:
        mov         edx, ecx
        and         ecx, 7Fh
        shr         edx, 7
        je          L_1a
        ; ecx = loop count
        ; edx = remaining copy length
align       16
 L_1:
        movdqa      xmm0,xmmword ptr [esi]
        movdqa      xmm1,xmmword ptr [esi + 10h]
        movdqa      xmm2,xmmword ptr [esi + 20h]
        movdqa      xmm3,xmmword ptr [esi + 30h]
        movdqa      xmmword ptr [edi],xmm0
        movdqa      xmmword ptr [edi + 10h],xmm1
        movdqa      xmmword ptr [edi + 20h],xmm2
        movdqa      xmmword ptr [edi + 30h],xmm3
        movdqa      xmm4,xmmword ptr [esi + 40h]
        movdqa      xmm5,xmmword ptr [esi + 50h]
        movdqa      xmm6,xmmword ptr [esi + 60h]
        movdqa      xmm7,xmmword ptr [esi + 70h]
        movdqa      xmmword ptr [edi + 40h],xmm4
        movdqa      xmmword ptr [edi + 50h],xmm5
        movdqa      xmmword ptr [edi + 60h],xmm6
        movdqa      xmmword ptr [edi + 70h],xmm7
        lea         esi,[esi + 80h]
        lea         edi,[edi + 80h]
        dec         edx
        jne         L_1
L_1a:
        test        ecx, ecx
        je          L_Return

        ; ecx = length (< 128 bytes)
        mov         edx, ecx
        shr         edx, 4
        test        edx, edx
        je          L_Trailing
        ; if > 16 bytes do a loop (16 bytes at a time)
        ; edx - loop count
        ; edi = dst
        ; esi = src
align 16
L_2:
        movdqa      xmm0, xmmword ptr [esi]
        movdqa      xmmword ptr [edi], xmm0
        lea         esi, [esi + 10h]
        lea         edi, [edi + 10h]
        dec         edx
        jne         L_2

L_Trailing:

        ; last 1-15 bytes: step back according to dst and src alignment and do a 16-byte copy
        ; esi = src
        ; eax = src alignment  (set at the start of the procedure and preserved up to here)
        ; edi = dst
        and         ecx, 0Fh
        ; ecx = remaining len
        je          L_Return

        ; get dword aligned
        mov     eax, ecx  ; save remaining len and calc number of dwords
        shr     ecx, 2
        je      L_TrailBytes ; if none try bytes
L_TrailDword:
        mov     edx, dword ptr [esi]
        mov     dword ptr [edi], edx
        lea     esi, [esi+4]
        lea     edi, [edi+4]
        dec     ecx
        jne     L_TrailDword
L_TrailBytes:
        mov     ecx, eax
        and     ecx, 03h
        je      L_Return ; if none return
L_TrailNextByte:
        mov     al, byte ptr [esi]
        mov     byte ptr [edi], al
        inc     esi
        inc     edi
        dec     ecx
        jne     L_TrailNextByte
align 16
L_Return:
        ; return dst
        pop     eax      ; Get destination for return
        pop     esi
        pop     edi
        M_EXIT

; dst addr is not 16 byte aligned
align 16
L_Notaligned:

; copy the first the first 1-15 bytes to align both src and dst up to the nearest 16-byte boundary:

; in
; esi = src
; edi = dst
; eax = src and dst alignment
; ecx = length

        mov     edx, 010h
        sub     edx, eax   ; calc num bytes to get it aligned
        sub     ecx, edx   ; calc new length and save it
        push    ecx
        mov     eax, edx   ; save alignment byte count for dwords
        mov     ecx, eax   ; set exc to rep count
        and     ecx, 03h
        je      L_MovDword ; if no bytes go do dwords
L_Byte:
        mov     dl, byte ptr [esi]  ; move the bytes
        mov     byte ptr [edi], dl
        inc     esi       ; inc the adrs
        inc     edi
        dec     ecx       ; dec the counter
        jne     L_Byte
L_MovDword:
        shr     eax, 2     ; get dword count
        je      L_Adjustcnt ; if none go to main loop
L_Dword:
        mov     edx, dword ptr [esi] ; move the dwords
        mov     dword ptr [edi], edx
        lea     esi, [esi+4] ; inc the adrs
        lea     edi, [edi+4]
        dec     eax          ; dec the counter
        jne     L_Dword
L_Adjustcnt:
        pop     ecx       ; retrive the adjusted length
        jmp     L_Aligned


_MEM_   endp
        end


