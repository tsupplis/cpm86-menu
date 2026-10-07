;
;  Licensed under the MIT license. See LICENSE file in the project root for details.
;
codeseg	segment	para public 'code'
dataseg segment para public 'data'
dataseg ends

	assume	cs:codeseg, ds:dataseg
    extrn memset_:near
    bss scb_pb_:word,257
;

	    public	setcurdrv_  
setcurdrv_	proc	near
        push	bp
        mov 	bp,sp
        push    es
        push	di
        push	si
        push    bx
        push    cx
        push    dx
        mov 	dx,word ptr 4[bp]
        mov     cx,0Eh
        int     0E0h
        pop     dx
        pop     cx
        pop     bx
        pop	    si
        pop 	di
        pop 	es
        pop 	bp
        ret
setcurdrv_  	endp

	    public	getccpdrv_  
getccpdrv_	proc	near
        push	bp
        mov 	bp,sp
        push    es
        push	di
        push	si
        push    bx
        push    cx
        push    dx
        mov     cx,0Ch
        int     0E0h
        cmp     ax,22h
        jz      ccpdrv_cp11sv
        cmp     ax,1430h
        jae     ccpdrv_ccp3xsd
        cmp     ax,1041h
        jz      ccpdrv_4xsv
        cmp     ax,1050h
        jz      ccpdrv_4xsv
        jmp     ccpdrv_none
ccpdrv_cp11sv:
        mov     cx,31h
        int     0E0h
        add     bx, 17h
        xor     ah, ah
        mov     al, es:[bx] 
        jmp     ccpdrv_end 
ccpdrv_ccp3xsd:
        mov     cx, 9Ah
        int     0E0h
        xor     ah,ah
        add     bx, 4Bh
        mov     al, es:[bx] 
        jmp     ccpdrv_end
ccpdrv_4xsv:
        mov     al,86h
        mov     byte ptr scb_pb_,al
        mov     al,0h
        mov     byte ptr scb_pb_+1,al
        mov     dx, offset scb_pb_    
        mov     cx,31h
        int     0E0h
        xor     ah,ah
        mov     al,byte ptr scb_pb_+2
        cmp     bx,0FFFh
        jnz     ccpdrv_end
ccpdrv_none:
        mov     ax,0FFFh
ccpdrv_end:
        pop     dx
        pop     cx
        pop     bx
        pop	    si
        pop 	di
        pop 	es
        pop 	bp
        ret
getccpdrv_  	endp

	    public	getcurdrv_  
getcurdrv_	proc	near
        push	bp
        mov 	bp,sp
        push    es
        push	di
        push	si
        push    bx
        push    cx
        push    dx
        mov     cx,19h
        int     0E0h
        pop     dx
        pop     cx
        pop     bx
        pop	    si
        pop 	di
        pop 	es
        pop 	bp
        ret
getcurdrv_  	endp

	    public	delay_
delay_	proc	near
        push	bp
        mov 	bp,sp
        push    es
        push	di
        push	si
        push    bx
        push    cx
        push    dx
        mov     cx,0Ch
        int     0E0h
        sub     al,22H
        jz      delay_bios
        mov     bl, 3
        mov     al, byte ptr 4[bp]
        mul     bl
        mov     dx,ax
        ;mov     dx, word ptr 4[bp]
        mov     cx,8Dh
        int     0E0h
        jmp     delay_end
delay_bios:
        mov     ah, 00h
        int     01Ah
        add     dx, word ptr 4[bp]
        mov     bx, dx
delay_lp:
        int     01Ah
        cmp     dx, bx
        jl      delay_lp
delay_end:
        pop     dx
        pop     cx
        pop     bx
        pop	    si
        pop 	di
        pop 	es
        pop 	bp
        xor     ax,ax
        ret
delay_	endp

	    public	bdosx_
bdosx_	proc	near
        push	bp
        mov 	bp,sp
        push	es
        push	di
        push	si
        push    bx
        push    cx
        mov 	cx,word ptr 4[bp]
        mov 	dx,word ptr 6[bp]
        int 	0E0h
        mov     cx,bx
        mov 	bx,word ptr 10[bp]
        mov     word ptr [bx],cx
        mov     cx,es
        mov 	bx,word ptr 8[bp]
        mov     word ptr [bx],cx
        pop     cx
        pop     bx
        pop	    si
        pop 	di
        pop 	es
        pop 	bp
        and     ax,255
        ret
bdosx_	endp

	        public	ostype_
ostype_ 	proc	near
            push	bp
            mov 	bp,sp
            push    ds
            push	es
            push	di
            push	si
            push    bx
            push    cx
            mov     cx,0Ch
            int     0E0h
            mov     al,ah
            xor     ah,ah
            cmp     al,0
            jnz     ostypeend
            mov     al,10h
ostypeend:
            pop     cx
            pop     bx
            pop	    si
            pop 	di
            pop     es
            pop     ds
            pop     bp
            ret
ostype_     endp

	        public	osver_
osver_  	proc	near
            push	bp
            mov 	bp,sp
            push    ds
            push	es
            push	di
            push	si
            push    bx
            push    cx
            mov     cx,0Ch
            int     0E0h
            xor     ah,ah
            pop     cx
            pop     bx
            pop	    si
            pop 	di
            pop     es
            pop     ds
            pop     bp
            ret
osver_      endp

	        public	bioskey_
bioskey_	proc	near
            push	bp
            mov 	bp,sp
            push    ds
            mov     ah, 0
            int     16h
            pop     ds
            pop     bp
            ret
bioskey_    endp

	    public	pckey_
pckey_	proc	near
        push	bp
        mov 	bp,sp
        push    ds
        push    bx
        cli
        xor     ax,ax
        in      al, 60h
        in      al, 60h
        push    ax
        in      al, 61h
        mov     bl, al
        or      al, 80h
        out     61h, al
        mov     al, bl
        out     61h, al
        mov     al, 20h
        out     20h,al
        pop     ax
        sti
        pop     bx
        pop     ds
        pop     bp
        ret
pckey_  endp

	    public	xstatline_
xstatline_	proc	near
        push	bp
        mov 	bp,sp
        push    es
        push    ds
        push    si
        push    ax
        push    bx
        push    cx
        mov     cl,0Ch
        int     0E0h
        push    ax
        mov     cl,9Ah
        int     0E0h
        mov	    cx, word ptr 4[bp]
        xor     ch,ch
        pop     ax
        cmp     ax, 1041h
        jz      xst_dosplus
        cmp     ax, 1432h
        jb      xst_exit
        mov     si, word ptr [68h] ;[4Eh] for Dos Plus
        jmp     xst_call
xst_dosplus:
        mov     si, word ptr [4Eh] 
xst_call:
        mov     dx,0
        push    es
        mov     es, 10h[si] 
        pop     ds
        mov     ax,20H 
        call    dword ptr[28h]
xst_exit:
        pop     cx
        pop     bx
        pop     ax
        pop     si
        pop     ds
        pop     es
        pop     bp
        ret
xstatline_  endp

;
; p_chain: BDOS fn 47 (0x2F) -- chain to program.
; Writes the command string into the CP/M command tail at 0080h, then
; calls BDOS fn 47 with the chain flag DL=0xFF (adopt current drive/user).
; Does not return on success.
;
; void p_chain(char *cmd)   -- cmd is a near pointer in DS
;
	public	p_chain_
p_chain_	proc	near
	push	bp
	mov	bp,sp
	push	si
	push	di
	push	es
	; set ES:DI to 0000:0080h (command tail in low memory)
	xor	ax,ax
	mov	es,ax
	mov	di,80h
	; copy cmd (DS:SI) into ES:[81h..], building length in CX
	mov	si,word ptr 4[bp]
	xor	cx,cx
p_chain_copy:
	mov	al,byte ptr [si]
	or	al,al
	jz	p_chain_done
	mov	byte ptr es:[di+1],al	; write at 81h+cx
	inc	si
	inc	cx
	inc	di
	cmp	cx,7fh			; max 127 chars
	jl	p_chain_copy
p_chain_done:
	mov	byte ptr es:[80h],cl	; length byte at 0080h
	pop	es
	pop	di
	pop	si
	pop	bp
	mov	cx,02Fh			; P_CHAIN = BDOS fn 47
	mov	dl,0ffh			; chain flag: adopt current drive/user
	xor	dh,dh
	int	0e0h			; does not return on success
	ret				; return on failure
p_chain_	endp

;
; sub_exit: set MDSUBE flag at absolute 0x0805 (CCP data segment),
; then perform BDOS fn 0 (system reset) so CCP picks up $$$     .SUB.
;
; Mirrors scd.a86 from the SUBMIT command:
;   mdsube equ 0805h  -- absolute address in CCP data segment
;   mov es:byte ptr mdsube, 0FFh
;   mov cl, 0 / int 224
;
	public	sub_exit_
sub_exit_	proc	near
	push	bp
	mov	bp,sp
	push	es
	xor	ax,ax
	mov	es,ax			; ES = segment 0 (absolute addressing)
	mov	byte ptr es:[0805h],0ffh ; MDSUBE at absolute 0x0805
	pop	es
	pop	bp
	xor	cx,cx			; BDOS fn 0 (system reset)
	xor	dx,dx
	int	0e0h
sub_exit_	endp

codeseg	ends

	    end
