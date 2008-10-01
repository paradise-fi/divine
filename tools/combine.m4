divert(-1)
changecom(`//')
changeword(`[_a-zA-Z][_a-zA-Z0-9]*[!?]?') dnl to kvuli tomu, abych mohl delat buffer channels a nahrazovt K! na K_in!

dnl snad poradnej forloop:
define(forloop, `ifelse(
  eval($2 > $3),  1,
    `',
  eval($2 <= $3), 1, 
    `pushdef(`$1', `$2')$4`'popdef(`$1')')`'dnl
ifelse(
  eval($2 < $3), 1,
    `forloop(`$1', incr($2), `$3', `$4')')')

dnl basic for loop; zacykli se kdyz $3 < $2 OPRAVIT
dnl define(`forloop', `pushdef(`$1', `$2')_forloop(`$1', `$2', `$3', `$4')popdef(`$1')')
dnl define(`forloop', `ifelse(eval(`$3'>=`$2'), 1, pushdef(`$1', `$2')_forloop(`$1', `$2', `$3', `$4')popdef(`$1'))')
dnl  define(`_forloop',
dnl       `$4`'ifelse($1, `$3', ,
dnl			 `define(`$1', incr($1))_forloop(`$1', `$2', `$3', `$4')')')
dnl specializovany loop, ktery pouziva ruzne oddelovace mezi a na konci (to se casto hodi), 5 arg. je oddelovac mezi, na konci se nepouzije
define(`myloop', `forloop(`$1', `$2', decr(`$3'), `$4 $5')pushdef(`$1', `$3')$4 popdef(`$1')')
dnl specializovany forloop od 0 do N-1
define(`forallx', `forloop(x, 0, decr(N), `$1')')

dnl bool veci
define(false, 0)
define(true, 1)
define(bool, byte) dnl lip to momentalne neumime

dnl pomocne veci na datove struktury
define(stack, `byte $1[$2];
byte $1_act = 0;
define($1_size, $2)') 

define(queue, `byte $1[$2];
byte $1_act = 0;
define($1_size, $2)')
dnl oboje
define(full, `$1_act == $1_size') 
define(empty, `$1_act == 0') 
dnl stack oper.
define(push, `$1[$1_act] = $2, $1_act = $1_act +1') 
define(pop, `$1[$1_act-1] = 0, $1_act = $1_act -1') dnl 0 tam davam kvuli efektivnosti
define(top, `$1[$1_act-1]') 
dnl fronta oper.
define(front, `$1[0]') 
define(pop_front, `forloop(i, 0, decr(decr($1_size)), `$1[i] = $1[incr(i)], ') $1[decr($1_size)] = 0, $1_act = $1_act -1') 

dnl channels:
define(buffer_channel, `channel $1_in, $1_out;
define(`$1!', `$1_in!')dnl
define(`$1?', `$1_out?')dnl
divert(1)
process channel_$1 {
queue(buf, $2)
state q;
init q;
trans
 q -> q { guard not(full(buf)); sync $1_in?buf[buf_act]; effect buf_act = buf_act+1; },
 q -> q { guard not(empty(buf)); sync $1_out!buf[0]; effect pop_front(buf);};
}
divert
')
dnl async channel (vlastne buf. channel vel. 1 ale to mi nejak nejde
define(async_channel,  `channel $1_in, $1_out;
define(`$1!', `$1_in!')
define(`$1?', `$1_out?')
divert(1)
process channel_$1 {
byte v;
state ready,tr;
init ready;
trans
 ready -> tr { sync $1_in?v; },
 tr -> ready { sync $1_out!v;};
}
divert
')
dnl loosy channel:
define(loosy_channel,  `channel $1_in, $1_out;
define(`$1!', `$1_in!')
define(`$1?', `$1_out?')
divert(1)
process channel_$1 {
byte v;
state ready,tr;
init ready;
trans
 ready -> tr { sync $1_in?v; },
 tr -> ready { }, // loose msg
 tr -> ready { sync $1_out!v;};
}
divert
')
dnl loosy channel:
define(bounded_loosy_channel,  `channel $1_in, $1_out;
define(`$1!', `$1_in!')
define(`$1?', `$1_out?')
divert(1)
process channel_$1 {
byte v, lost=0;
state ready,tr;
init ready;
trans
 ready -> tr { sync $1_in?v; },
 tr -> ready { guard lost < $2; effect lost = lost + 1; }, // loose msg
 tr -> ready { sync $1_out!v; effect lost = 0; };
}
divert
')


define(system,`undivert(1)`system'') dnl to proto, aby se mi pred definici systemu vypsali vsechny pomocne procesy

define(default, `ifdef(`$1',`',define($1,$2))')

divert