set -xe

if test -n "$NIX_BUILD"; then
    fail="touch $out/nix-support/failed"
    mkdir -p $out/nix-support
else
    fail="exit 1"
fi

"$@" || $fail

if test -n "$NIX_BUILD" && test -e results; then
    mkdir -p $out/test-results
    cp -R results/*.txt $out/test-results/
    cat results/list >> $out/test-results/list
    hbp=$out/nix-support/hydra-build-products
    grep $out/test-results $hbp || echo "report tests $out/test-results" >> $hbp
fi
