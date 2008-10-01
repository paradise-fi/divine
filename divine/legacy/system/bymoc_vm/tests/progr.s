!module "main"

	GLOBSZ	4
!strinf begin scope_init 0
	LDC	0
	STVA	G	4	0
!strinf end scope_init 0
	RUN	0	0	P_A
	POP	r0
	RUN	0	0	P_B
	POP	r0
	STEP	1	T	0
!strinf begin proc A
P_A:
!srcloc 5 3
!flags progress
S1_A_progress:
!srcloc 5 13
	LDVA	G	4	0
	LDC	0
	NEQ
	NEXZ
	STEP	1	T	0
!strinf end proc A
!strinf begin proc B
P_B:
!srcloc 10 3
	LDS	np
	LDC	0
	EQ
	NEXZ
	STEP	1	N	0
!srcloc 11 3
	LDC	1
	TRUNC	-31
	STVA	G	4	0
	STEP	1	N	0
!srcloc 12 3
	LDS	np
	LDC	1
	EQ
	NEXZ
	STEP	1	T	0
!strinf end proc B


