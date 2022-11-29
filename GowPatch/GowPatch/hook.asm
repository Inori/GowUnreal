
pushaq MACRO
	push rax
	push rbx
	push rcx
	push rdx
	push rbp
	push rsi
	push rdi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
ENDM

popaq MACRO
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdi
	pop rsi
	pop rbp
	pop rdx
	pop rcx
	pop rbx
	pop rax
ENDM


EXTERN NewParseAnime :PROC

.CODE


ParseAnimeWrapper PROC
sub rsp, 20h

mov r9, rbp
call NewParseAnime

add rsp, 20h
ret
ParseAnimeWrapper ENDP


END