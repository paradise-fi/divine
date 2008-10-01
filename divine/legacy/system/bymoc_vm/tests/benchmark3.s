; ByMoC ex - Bytecode based Model Checker example assembly code
; version 0.1 date 2005-04-08
; Copyright (C) 2005: Stefan Schuermans <stes@i2.informatik.rwth-aachen.de>
;                     Lehrstuhl fuer Informatik II, RWTH Aachen
; Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html

; chan c = [0] of {int};

init:		GLOBSZ	1
 		LDC	-31
 		CHNEW	0	1
		STVA	G	1u      0
		RUN	4	0	send
		POP	r0
		RUN	4	0	recv
		POP	r0
		STEP	1	T	0

; active proctype send( ) {
;   int i = 0;
;   do
;     :: c!i; i++
;   od
; }

send:		LDC	0
		STVA	L	4       0
se_1:		LDVA	G	1u      0
		TOP	r0
		CHFREE
		NEXZ
		PUSH	r0
		CHADD
		PUSH	r0
		LDVA	L	4       0
		CHSETO	0
		STEP	2	N	0
		LDVA	L	4       0
		LDC	1
		ADD
		STVA	L	4       0
		STEP	2	N	0
		JMP	se_1
        	STEP	1	T	0

; active proctype recv( ) {
;   int i;
;   do
;     :: c?i
;   od
; }

recv:   	LDC	0
		STVA	L	4       0
re_1:		LDVA	G	1u      0
		TOP	r0
		CHLEN
		NEXZ
		PUSH	r0
		PUSH	r0
		CHGETO	0
		STVA	L	4       0
		CHDEL
		STEP	2	N	0
		JMP	re_1
		STEP	1	T	0

