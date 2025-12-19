! setup.s

.globl begtext, begdata, begbss, endtext, enddata, endbss
.text
begtext:
.data
begdata:
.bss
begbss:
.text

SETUPLEN = 2				! nr of setup-sectors
BOOTSEG  = 0x07c0			! original address of boot-sector
INITSEG  = 0x9000			! we move boot here - out of the way
SETUPSEG = 0x9020			! setup starts here

entry _start
_start:

! Print "we are in the seup"

	mov	ax,#SETUPSEG
	mov	es,ax

	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	
	mov	cx,#25
	mov	bx,#0x0007		! page 0, attribute 7 (normal)
	mov	bp,#msgSETUP
	mov	ax,#0x1301		! write string, move cursor
	int	0x10

! Read cursor

	mov	ax,#INITSEG
	mov	ds,ax
	mov	ah,#0x03
	xor	bh,bh
	int	0x10
	mov	[0],dx

! Read memory

	mov	ah,#0x88
	int	0x15
	mov	[2],ax

! Read others

	mov	ax,#0x0000
	mov	ds,ax
	lds	si,[4*0x41]
	mov	ax,#INITSEG
	mov	es,ax
	mov	di,#0x0004
	mov	cx,#0x10
	rep !重复16次
	movsb

! Init stack

	mov	ax,#SETUPSEG
	mov	es,ax  		

	mov ax,#INITSEG
	mov ss,ax
	sub sp,sp

! Print CURSOR

	call 	print_before
	mov		bp,#msgCURSOR
	call	print_after
	mov 	bp,#0x00
	call 	print_hex

! Print MEMORY

	call 	print_before
	mov		bp,#msgMEMORY
	call	print_after
	mov 	bp,#0x02
	call 	print_hex

! Print CYLS

	call 	print_before
	mov		bp,#msgCYLS
	call	print_after
	mov 	bp,#0x04
	call 	print_hex

! Print HEADS

	call 	print_before
	mov		bp,#msgHEADS
	call	print_after
	mov 	bp,#0x06
	call 	print_hex

! Print SECTOR

	call 	print_before
	mov		bp,#msgSECTOR
	call	print_after
	mov 	bp,#0x12
	call 	print_hex

inf_loop:
	jmp inf_loop

! 以16进制方式打印栈顶的16位数
print_hex:
    mov	cx,#4 		! 4个十六进制数字
    mov	dx,(bp) 	! 将(bp)所指的值放入dx中，如果bp是指向栈顶的话
print_digit:
    rol	dx,#4		! 循环以使低4比特用上 !! 取dx的高4比特移到低4比特处。
    mov	ax,#0xe0f 	! ah = 请求的功能值，al = 半字节(4个比特)掩码。
    and	al,dl 		! 取dl的低4比特值。
    add	al,#0x30 	! 给al数字加上十六进制0x30
    cmp	al,#0x3a
    jl	outp  		!是一个不大于十的数字
    add	al,#0x07  	!是a～f，要多加7
outp: 
    int	0x10
    loop	print_digit
! 打印回车换行
print_nl:
    mov	ax,#0xe0d 	! CR
    int	0x10
    mov	al,#0xa 	! LF
    int	0x10
    ret

print_before:
	mov	ah,#0x03		! read cursor pos
	xor	bh,bh
	int	0x10
	
	mov	cx,#16
	mov	bx,#0x0007		! page 0, attribute 7 (normal)
	ret

print_after:
	mov	ax,#0x1301		! write string, move cursor
	int	0x10
	ret

msgSETUP:
	.byte 13,10
	.ascii "Now we are in SETUP"
	.byte 13,10,13,10

msgCURSOR:
	.ascii "*      Cursor  :"
msgMEMORY:
	.ascii "*      Memory  :"
msgCYLS:
	.ascii "*      Cyls    :"
msgHEADS:
	.ascii "*      Heads   :"
msgSECTOR:
	.ascii "*      Sectors :"

.text
endtext:
.data
enddata:
.bss
endbss:
