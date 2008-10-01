; ByMoC ex - Bytecode based Model Checker example assembly code
; version 0.1 date 2005-04-08
; Copyright (C) 2005: Stefan Schuermans <stes@i2.informatik.rwth-aachen.de>
;                     Lehrstuhl fuer Informatik II, RWTH Aachen
; Copyleft: GNU public license - http://www.gnu.org/copyleft/gpl.html

; chan c = [1] of {int};

init:		GLOBSZ	1
 		LDC	-31
 		CHNEW	1	1
		STVA	G	1u      0
		RUN	4	0	send
		POP	r0
		RUN	4	0	recv
		POP	r0
		STEP	1	T	0

; active proctype send( ) {
;   int i = 0;
;   do
;     :: c!i;
;        if
;          :: i = i + 1
;          :: i = i + 2
;          :: i = i + 3
;          :: i = i + 4
;          :: i = i + 5
;          :: i = i + 6
;          :: i = i + 7
;          :: i = i + 8
;          :: i = i + 9
;          :: i = i + 10
;        fi
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
                NDET    se_2
                NDET    se_3
                NDET    se_4
                NDET    se_5
                NDET    se_6
                NDET    se_7
                NDET    se_8
                NDET    se_9
                NDET    se_10
		LDVA	L	4       0
		LDC	1
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_2:		LDVA	L	4       0
		LDC	2
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_3:		LDVA	L	4       0
		LDC	3
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_4:		LDVA	L	4       0
		LDC	4
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_5:		LDVA	L	4       0
		LDC	5
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_6:		LDVA	L	4       0
		LDC	6
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_7:		LDVA	L	4       0
		LDC	7
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_8:		LDVA	L	4       0
		LDC	8
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_9:		LDVA	L	4       0
		LDC	9
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_10:		LDVA	L	4       0
		LDC	10
		ADD
		STVA	L	4       0
		STEP	2	N	0
                JMP     se_11
se_11:		JMP	se_1
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

