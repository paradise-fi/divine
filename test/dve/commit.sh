. lib
. flavour vanilla

dve_shape deadlock "1->2 1->3 2->4 3->5 3->1 4->6 5->7 6->8 7->9 8->2 8->10 9->10 10->5 10->8" <<EOF
process Pcom
{
    int x=0;
    state a,b,c;
    init a;
    commit b;
    trans
        a->b {guard true; effect x=0;},
        b->b {guard x < 2; effect x = x + 1;},
        b->a {guard x == 2;}
    ;
}

process P1
{
    state a,b;
    init a;
    trans 
        a->b {guard true;},
        b->a {guard true;}
    ;
}

system async;
EOF

dve_shape deadlock "1->2 1->3 2->4 3->5 3->1 4->6 5->7 7->8" <<EOF
process Pcom
{
    int x=0;
    state a,b,c;
    init a;
    commit b;
    trans
        a->b {guard true; effect x=0;},
        b->b {guard x < 2; effect x = x + 1;}
    ;
}

process P1
{
    state a,b;
    init a;
    trans 
        a->b {guard true;},
        b->a {guard true;}
    ;
}

system async;
EOF
