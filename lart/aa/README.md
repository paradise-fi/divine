Alias Analysis in LART
======================

The alias analyses implemented in LART are fairly expensive, and usually can't
provide on-the-fly answers. Instead, they compute a whole-module analysis that
gives points-to information for all "top level" values (mainly LLVM registers)
in the program, and for any "abstract memory locations" these values could be
pointing to.

As a rule, all alias analyses implemented in LART are "may" analyses, and
represent their result using points-to sets, where a particular pointer may
take any of the values in its points-to set. If this set is singleton, the
information is fully precise. Most importantly, the analyses guarantee that if
a particular location is *not* in a points-to set, the pointer must not take
that value.

Abstract Memory
---------------

In a running program, each memory (heap) allocation will result in a concrete
memory location being created in the program's address space and used. For the
purpose of alias analysis, we need to abstract those concrete memory locations
in some fashion, as it is impossible to statically compute the set of concrete
locations. The common abstraction used for this is assigning a single abstract
location to every callsite of a memory allocation routine.

In LART, the creation of abstract memory locations is a choice to be made by a
particular analysis. Initially, we will use the common approach of using one
abstract location for each static call that allocates memory.

Abstract memory locations are represented as unique integers.

Context and Flow Sensitivity
----------------------------

An alias analysis can be global, computing a single conservative "may point to"
solution for the entire program, meaning that for a particular pointer, it will
never "fall out" of its points-to set for the entire lifetime of the program.
In many cases, this analysis will be overly pessimistic. To that end, there are
two common ways in which to refine this global view. One computes distinct
solutions for each call-site (this is called context sensitivity), and another
for each control-flow location (this is called flow sensitivity). These two are
somewhat orthogonal: an analysis can be neither, one of them or both.

Metadata Format
---------------

As outlined above, the analyses produce a significant quantity of data and
could require fairly long time to do so. Moreover, it is desirable that this
data be readily available to external tools, even though LART itself is not
going to be part of LLVM in foreseeable future. As such, providing analysis
results in a well-defined format embedded inside LLVM bitcode as metadata which
standard LLVM libraries and tools can read seems to be a good compromise.

This makes the analysis results persistent, and easily re-usable without a
requirement for run-time linking or exporting stable APIs from LART.

The structure of the persistent metadata needs to be such that it can easily
represent results of multiple different alias analyses, with various degrees of
context and flow sensitivity. In particular, this means that the metadata needs
to be able to attach points-to sets to particular contexts, whether that
context is derived from flow sensitivity or from callsite/callgraph
sensitivity.

## Representing Points-To Sets

While top-level variables in LLVM are in an SSA form (meaning that they never
change their value during their lifetime), this is not true of allocated
memory, whether with alloca instructions or with malloc (and it wouldn't be
necessary to perform further alias analysis if it was). As such, the top-level
points-to sets (attached to toplevel pointers) never change during their
lifetime -- however, the points-to sets available indirectly through them do.
To take advantage of this fact, it is desirable to decouple transitive
points-to relations, so that top-level points-to sets can be attached to
definitions of toplevel values (i.e. to the instruction that defines them). As
such, they are entirely immutable. The points-to sets can only point at
abstract memory locations, toplevel variables in LLVM never have their
addresses taken and as such no pointers can point at them.

Apart from toplevel points-to sets, the metadata needs to also represent sets
that are attached to abstract memory locations. These sets will be (depending
on the type of the analysis) different at different point in the program, and
hence it needs to be possible to keep multiple such sets for each abstract
memory location, distinguished by context.

The representation of the context needs to be such as to be able to efficiently
decide if a particular callstack falls under the given context. For
flow-sensitive queries, only the topmost callframe is relevant, while for
context-sensitive queries, all but the topmost callframe can be potentially
relevant.

The contexts form a semi-lattice, and so do the points-to sets associated to
those contexts. Meets of contexts correspond to joins of points-to sets and
vice-versa. Intuitively, the broader the context the bigger (more
over-approximated) the points-to sets it entails. The idea here is that for a
particular context, we can obtain over-approximate points-to set as a union of
points-to sets in all contexts that are more specific. Conversely, for a more
specific context, if exact information for that context is not available, a
points-to set for any enclosing context is a sound over-approximation of the
desired answer. This gives us a good way to represent the context -> points-to
set mappings for abstract memory locations: each abstract memory location has a
context tree attached to it.

There are two types of context-tree nodes, internal and leaf nodes. An internal
node only has a single instruction pointer in it, always pointing to a callsite
(there is an additional virtual callsite that represents the entire program,
i.e. the root of the static call graph). Leaf nodes additionally contain an
actual points-to set. The points-to set for an internal node can be computed as
an union of all its children's points-to sets, and as such is not stored
explicitly. The depth of a context tree is entirely the discretion of the
analysis in question, and queries are satisfied by giving the most-specific
points-to set available in the context tree. (In the degenerate case of a
context-insensitive analysis, the context tree is singleton.)

To represent flow sensitivity in the data, context-trees are rather cumbersome
and inefficient, as they would need to allow representing instruction spans,
and for context-insensitive, flow-sensitive analysis, addition of many
redundant internal nodes to the context tree. Instead, a different
representation is used to represent flow sensitivity, orthogonal to context
sensitivity. Recall that instructions have a static points-to set attached to
them, representing the *result* of that instruction. Additionally, we can
attach a map from abstract memory locations to context trees to each
instruction, containing all the AMLs that any of the static points-to set in
any of its *arguments* refer to. The context trees in this map then represent
the points-to sets for those memory locations at the particular spot in the
control flow graph. Again, for context-insensitive, flow-sensitive analyses,
these context trees will be singleton. Conversely, for flow-insensitive,
context-sensitive analyses, they will just point to the global context trees
for the relevant memory locations (over-approximating the flow sensitivity
away).

## Mapping Points-To Set Representation to LLVM Metadata

To summarise the discussion above, we should set out the various types that
come into play in representing points-to sets.

abstract memory location
:   a unique representation for (a set of) memory locations
points-to set
:   a set of abstract memory locations
context tree
:   each node points to a particular callsite, representing the static
    callgraph of the program; leaf nodes additionaly contain a points-to set;
    representing context trees is particularly tricky, because it is not
    possible to directly store references to instructions in global metadata;
    instead, context tree nodes are referenced from callsites, and value
    use-def chains can be used to look up the callsite for a particular context
    tree node
AML map
:   a function from abstract memory locations to context trees
instruction
:   each instruction has a single points-to set attached, representing the
    points-to set of the (toplevel) result of this instruction, and a single
    AML map, which has entries for each element in all static points-to sets
    for all operands of the instruction, and transitively for all AMLs that
    arise in points-to sets of any already included AMLs
global points-to data
:   a single top-level AML map with entries for all AMLs that exist in the
    program; can be automatically summarised using results of a flow-sensitive
    analysis by unioning AML maps attached to all instructions, or can be
    obtained directly as a result of a flow-insensitive analysis

LLVM metadata is structured as a graph with labelled edges and nodes being
tuples of primitive values. The basic idea of the format is to represent the
above data types as LLVM metadata nodes. Abstract memory locations are simply
represented by nodes carrying a numeric ID. Points-to sets are represented by a
single node, with outgoing edges for each element of that set. Context tree are
mapped naturally to LLVM metadata trees, AML maps are represented as a list of
tuples (AML pointer, context tree pointer). Instructions get two named metadata
slots, `!aa_def` and `!aa_use`, first representing the result points-to set and
the other an AML map.

## Context Trees

As outlined earlier, context trees are particularly challenging to embed in
LLVM metadata. The trees are formed with no references to the actual callsites
they represent; instead, each callsite gets a unique ID and this ID is stored
in the context tree. To make lookups easy, each callsite is annotated with a
list of context tree nodes that reference its ID. Then, to obtain a pointer to
the particular callsite instruction from a context tree node, we look at the
node's use list -- the only instruction in the use list is the callsite.

## An Annotated IR Example

XXX

Future Extensions
-----------------

This spec is a work in progress. We expect to extend the metadata format to
cover type information, especially for agregate types, and field sensitivity in
the points-to sets.
