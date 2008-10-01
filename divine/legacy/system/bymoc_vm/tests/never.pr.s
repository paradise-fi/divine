!module "main"

	GLOBSZ	4
!strinf begin scope_init 0
	LDC	0
	STVA	G	4	0
!strinf end scope_init 0
	LRUN	0	0	P_never
	MONITOR
	STEP	1	N	0
	LJMP	P_init
!strinf begin proc init
P_init:
!srcloc 9 3
!strinf begin do
L0:
	NDET	L1
	NDET	L2
	NDET	L3
!strinf begin option
!srcloc 10 8
	LDC	0
	TRUNC	-31
	STVA	G	4	0
	STEP	2	N	0
	JMP	L0
!strinf end option
!strinf begin option
L1:
!srcloc 11 8
	LDC	1
	TRUNC	-31
	STVA	G	4	0
	STEP	2	N	0
	JMP	L0
!strinf end option
!strinf begin option
L2:
!srcloc 12 8
	LDC	2
	TRUNC	-31
	STVA	G	4	0
	STEP	2	N	0
	JMP	L0
!strinf end option
!strinf begin option
L3:
!srcloc 13 8
	LDC	3
	TRUNC	-31
	STVA	G	4	0
	STEP	2	N	0
	JMP	L0
!strinf end option
L4:
	STEP	2	T	0
!strinf end do
!strinf end proc init
!strinf begin proc never
P_never:
!srcloc 19 3
!strinf begin do
L5:
	NDET	L6
	NDET	L7
!strinf begin option
!srcloc 20 8
	LDVA	G	4	0
	LDC	1
	EQ
	NEXZ
	STEP	2	N	0
	JMP	L5
!strinf end option
!strinf begin option
L6:
!srcloc 21 8
	LDVA	G	4	0
	LDC	2
	EQ
	NEXZ
	STEP	2	N	0
!srcloc 21 16
!flags accept
S_never_accept:
!srcloc 21 24
	LDVA	G	4	0
	LDC	2
	NEQ
	NEXZ
	STEP	2	N	0
	JMP	L5
!strinf end option
!strinf begin option
L7:
!srcloc 22 8
	LDVA	G	4	0
	LDC	3
	EQ
	NEXZ
	STEP	2	N	0
!srcloc 22 16
	JMP	L8
	JMP	L5
!strinf end option
L8:
	STEP	2	N	0
!strinf end do
L9:
	STEP	1	N	0
!flags accept
	JMP	L9
!strinf end proc never


