!module "main"

	GLOBSZ	4
!strinf begin scope_init 0
	LDC	0
	STVA	G	4	0
!strinf end scope_init 0
	LRUN	4	0	P_p
	POP	r0
	STEP	1	N	0
	LJMP	P_init
!strinf begin proc p
P_p:
!strinf begin scope_init 2
	LDC	0
	STVA	L	4	0
!strinf end scope_init 2
!srcloc 6 3
	LDC	0
	TRUNC	-31
	STVA	L	4	0
	STEP	1	N	0
!srcloc 7 3
S_p_l:
!srcloc 7 6
	LDVA	G	4	0
	LDC	0
	NEQ
	NEXZ
	STEP	1	T	0
!strinf end proc p
!strinf begin proc init
P_init:
!srcloc 12 3
	LDC	2
	PCVAL
	LDC	0
	GT
	NEXZ
	STEP	1	N	0
!srcloc 13 3
	LDC	2
	PCVAL
	LDA	S_p_l
	EQ
	NEXZ
	STEP	1	N	0
!srcloc 14 3
	LDC	2
	LDC	0
	LVAR	4
	LDC	0
	EQ
	NEXZ
	STEP	1	N	0
!srcloc 15 3
	LDC	1
	TRUNC	-31
	STVA	G	4	0
	STEP	1	T	0
!strinf end proc init


