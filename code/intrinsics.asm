public _handmade_intrinsic_sin
.data
temp dword ?
.code
_handmade_intrinsic_sin PROC

	push rbp
	push rbx
	
	movss real4 ptr[temp], xmm0
	fld dword ptr[temp]
	fsin
	fstp dword ptr[temp]
	movss xmm0, real4 ptr[temp]

	pop rbx
	pop rbp
	ret

_handmade_intrinsic_sin ENDP
end
