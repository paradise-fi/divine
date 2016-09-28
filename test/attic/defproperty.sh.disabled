. lib
. flavour vanilla

# test that divine has default property for each major generator

test "$ALG_REACHABILITY" = ON || skip

if [ "$GEN_LLVM" = "ON" ]; then
    run llvm_assemble data/global-ok.ll verify
    check report Finished Yes
fi

if [ "$GEN_DVE" = "ON" ]; then
    run verify data/empty.dve
    check report Finished Yes
fi

if [ "$GEN_TIMED" = "ON" ]; then
    run verify data/zeno.xml
    check report Finished Yes
fi
