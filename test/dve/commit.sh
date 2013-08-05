. lib

dve_shape deadlock "1->2 2->3 3->4 4->5 5->2" <<EOF
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

dve_shape deadlock "1->2 2->3 3->4" <<EOF
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
