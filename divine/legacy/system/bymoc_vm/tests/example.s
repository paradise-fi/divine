; ByMoC ex - Bytecode based Model Checker example assembly code
; version 0.9.3 date 2005-06-23
; Copyright (C) 2005: Stefan Schuermans <stes@i2.informatik.rwth-aachen.de>
;                     Lehrstuhl fuer Informatik II, RWTH Aachen
; Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html

!module "main"

; chan c = [1] of {int};

init:		GLOBSZ	1
 		LDC	-31
 		CHNEW	1	1
		STVA	G	1u	0
		RUN	0	0	send
		POP	r0
		RUN	4	0	recv
		POP	r0
		STEP	1	N	0

; some more code - not generated from promela source

		LDS	timeout
		NEXZ
		PRINTS	1
		PRINTS	2
		STEP	1	T	0

; active proctype send( ) {
;   atomic {
;     c!3;
;     c!5
;   }
; }

send:		LDVA	G	1u	0
		TOP	r0
		CHFREE
		NEXZ
		PUSH	r0
		CHADD
		PUSH	r0
		LDC	3
		CHSETO	0
		STEP	1	A	0
		LDVA	G	1u	0
		TOP	r0
		CHFREE
		NEXZ
		PUSH	r0
		CHADD
		PUSH	r0
		LDC	5
		CHSETO	0
		STEP	1	T	0

; active proctype recv( ) {
;   int i = 2;
;   do
;     :: i < 5; c?i
;     :: else; break
;   od
; }

recv:   	LDC	2
		STVA	L	4	0
re_1:		NDET	re_2
		LDVA	L	4	0
		LDC	5
		LT
		NEXZ
		STEP	2	N	0
		LDVA	G	1u	0
		TOP	r0
		CHLEN
		NEXZ
		PUSH	r0
		PUSH	r0
		CHGETO	0
		STVA	L	4	0
		CHDEL
		STEP	2	N	0
		JMP	re_1
re_2:		STEP	1	N	0
		STEP	1	T	0

!string 1 "this example "
!string 2 "is now finshed"

