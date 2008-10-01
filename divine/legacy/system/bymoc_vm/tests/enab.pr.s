!module "main"

	GLOBSZ	5
!strinf begin scope_init 0
	LDC	-31
	CHNEW	3	1
	STVA	G	1u	4
	LDC	0
	STVA	G	4	0
!strinf end scope_init 0
	LRUN	0	0	P_p
	POP	r0
	STEP	1	N	0
	LJMP	P_init
!strinf begin proc p
P_p:
!srcloc 6 3
	LDVA	G	1u	4
	TOP	r0
	CHFREE
	NEXZ
	PUSH	r0
	CHADD
	PUSH	r0
	LDC	1
	CHSETO	0
	STEP	1	N	0
!srcloc 7 3
	LDVA	G	4	0
	LDC	0
	NEQ
	NEXZ
	STEP	1	N	0
!srcloc 8 3
	LDVA	G	1u	4
	TOP	r0
	CHFREE
	NEXZ
	PUSH	r0
	CHADD
	PUSH	r0
	LDC	3
	CHSETO	0
	STEP	1	T	0
!strinf end proc p
!strinf begin proc init
P_init:
!srcloc 13 3
	LDC	2
	ENAB
	BNOT
	NEXZ
	STEP	1	N	0
!srcloc 14 3
	LDVA	G	1u	4
	TOP	r0
	CHFREE
	NEXZ
	PUSH	r0
	CHADD
	PUSH	r0
	LDC	2
	CHSETO	0
	STEP	1	N	0
!srcloc 15 3
	LDC	1
	TRUNC	-31
	STVA	G	4	0
	STEP	1	T	0
!strinf end proc init


