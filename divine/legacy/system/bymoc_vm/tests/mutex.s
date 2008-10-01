!module "main"

	GLOBSZ	2
!strinf begin scope_init 0
	LDC	-31
	CHNEW	1	1
	STVA	G	1u	1
	LDC	1
	CHNEW	1	1
	STVA	G	1u	0
!strinf end scope_init 0
	RUN	4	0	P_receiver
	POP	r0
	LDC	3
	POP	r1
L0:
	RUN	0	0	P_sender
	POP	r0
	LOOP	r1	L0
	STEP	1	T	0
!strinf begin proc sender
P_sender:
!srcloc 6 3
!strinf begin do
L1:
!strinf begin option
!srcloc 7 8
	LDVA	G	1u	0
	TOP	r0
	CHFREE
	NEXZ
	PUSH	r0
	CHADD
	PUSH	r0
	LDC	1
	CHSETO	0
	STEP	2	N	0
!srcloc 8 8
	LDVA	G	1u	1
	TOP	r0
	CHFREE
	NEXZ
	PUSH	r0
	CHADD
	PUSH	r0
	LDS	pid
	CHSETO	0
	STEP	2	N	0
!srcloc 9 8
	LDVA	G	1u	0
	TOP	r0
	CHLEN
	NEXZ
	PUSH	r0
	PUSH	r0
	PUSH	r0
	CHGETO	0
	LDC	1
	EQ
	NEXZ
	POP	r0
	CHDEL
	STEP	2	N	0
	JMP	L1
!strinf end option
L2:
	STEP	2	T	0
!strinf end do
!strinf end proc sender
!strinf begin proc receiver
P_receiver:
!strinf begin scope_init 5
	LDC	0
	STVA	L	4	0
!strinf end scope_init 5
!srcloc 16 3
!strinf begin do
L3:
!strinf begin option
!srcloc 17 8
	LDVA	G	1u	0
	TOP	r0
	CHFREE
	NEXZ
	PUSH	r0
	CHADD
	PUSH	r0
	LDC	1
	CHSETO	0
	STEP	2	N	0
!srcloc 18 8
	LDVA	G	1u	1
	TOP	r0
	CHLEN
	NEXZ
	PUSH	r0
	PUSH	r0
	CHGETO	0
	TRUNC	-31
	STVA	L	4	0
	CHDEL
	STEP	2	N	0
!srcloc 19 8
	LDVA	G	1u	0
	TOP	r0
	CHLEN
	NEXZ
	PUSH	r0
	PUSH	r0
	PUSH	r0
	CHGETO	0
	LDC	1
	EQ
	NEXZ
	POP	r0
	CHDEL
	STEP	2	N	0
	JMP	L3
!strinf end option
L4:
	STEP	2	T	0
!strinf end do
!strinf end proc receiver


