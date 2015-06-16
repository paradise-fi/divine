Register Interference Graph
===========================

LART can compute register interference graphs and store them in LLVM
metadata. The encoding is based on !lart.id metadata nodes; each instruction
gets a set of all interfering values attached through !lart.interference. These
sets include 'void' values, the final 'use' instructions that kill the register
as well as the definition itself.

The algorithm works by propagating references to all 'use' instructions
backwards through the CFG, effectively marking up the region of liveness of the
register. The propagation stops when either the corresponding 'def' or another
instruction that is already in the interference set for 'def' is reached.
