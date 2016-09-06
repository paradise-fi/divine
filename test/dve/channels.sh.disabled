. lib
. flavour vanilla


for bufsize in 0 1 3; do
    test $bufsize = 0 && size="2 3";
    test $bufsize = 1 && size="3 5";
    test $bufsize = 3 && size="3 5";

    dve_statespace deadlock $size 0 0 <<EOF
    channel {int,int} test[$bufsize];

    process P1
    {
      int x=1,y=2;
      state a,b;
      init a;
      trans a->b {guard true; sync test!{x,y};},
    	    b->b {guard true;};
    }

    process P2
    {
      int z=0,w=0;
      state a,b;
      init a;
      trans a->b {guard true; sync test?{w,z};},
            b->b {guard true;};
    }

    system async;
EOF

    # local 1
    dve_statespace deadlock $size 0 0 <<EOF
    process P1
    {
      channel {int,int} test[$bufsize];
      int x=1,y=2;
      state a,b;
      init a;
      trans a->b {guard true; sync test!{x,y};},
            b->b {guard true;};
    }

    process P2
    {
      int z=0,w=0;
      state a,b;
      init a;
      trans a->b {guard true; sync P1->test?{w,z};},
            b->b {guard true;};
    }

    system async;
EOF

    # local 2
    dve_statespace deadlock $size 0 0 <<EOF
    process P1
    {
      int x=1,y=2;
      state a,b;
      init a;
      trans a->b {guard true; sync P2->test!{x,y};},
            b->b {guard true;};
    }

    process P2
    {
      channel {int,int} test[$bufsize];
      int z=0,w=0;
      state a,b;
      init a;
      trans a->b {guard true; sync test?{w,z};},
            b->b {guard true;};
    }

    system async;
EOF
done
