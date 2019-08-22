## RST

RST stands for Runtime Support for Transformations and serves for symbolic verification
of programs. The symbolic part is achieved by abstracting values ("lifting" them into
abstract domains) and propagating the abstractions where the values are further used.

Files relevant to RST can be found in:
 - `dios/rst/*`         - .cpp
 - `dios/include/rst/*` - headers
 - `bricks/brick-smt`   - SMT-LIB operations
 - common{.hpp}         - utilities: tainting, peek/poke, setting flags

### Domains

With abstractions, we need a concrete domain and an abstract domain, to abstract the
value into (a typical example would be integers and abstraction to even/odd or
positive/negative/zero).

`domains.h` holds functions available to the user to define an abstract value.
An example test case with the use of a symbolic 32-bit integer is:

    #include <rst/domains.h>
    #include <assert.h>

    int main()
    {
        int i = __sym_val_i32();
        assert( i != 0 );  // error, `i` can hold any int value
    }

### Lifting

Lifting is the process of abstracting a value from the concrete to the abstract domain.
We normally lift a single value of specific type, e.g. lift one 8-bit integer to the 
abstract (term etc.) domain.
The opposite of lift() is lower(), which converts an abstract value to a concrete
value (set of concrete values). Lowering is, obviously, not a symmetric operation.
Lowering after lifting is not necessarily even of the same type (a float can be
represented as a pair of integers, for instance).

The abstract domain has to form a lattice and has to have a top, which only means that
we need an abstraction that includes all values from the concrete domain (== `lift_any`).

There can be multiple abstract domains for a given concrete domain. In rst, they are
distinguished by ID (in `base.hpp`).
The domains present in DIVINE are:

- **Star domain** = unit{.hpp,.cpp} = a single unit/value, everything is abstracted
to a Star
- **Tristate** = special LART domain, for deciding conditional branching
    - has values True, False, Unknown ("Maybe")
    - every domain needs to define the `to_tristate()` method, to decide which path
    to take in case an abstract value of the class is encountered in an if condition
- **Term domain** = terms are formed from constants, variables and operations on them;
    a constant or a variable is a term by itself, an operation needs well-formed
    terms as its operands to be a well-formed term. The Term abstract domain makes
    use of SMT-LIB to check whether a term is satisfiable. The available operations
    are defined in `bricks/brick-smt`. Internally, a term is encoded as a sequence
    of bytes, in a structure called RPM (also in `brick-smt`). RPM stands for
    Reverse Polish Notation. RPNView is then used to transform/decode the bytecode
    into a string that can be input to SMT-LIB.    
- (**Mstring** = string abstraction, string segments and their properties)

A domain defines:
    - __lift_one_TYPE
    - __lift_any
    - (lower) - can only lower() Tristate to bool
    - to_tristate + assume
    - constructors for a user
    - operations on the domain - with abstract operands

### Tainting

When LART computes a set of symbolic values, the variables of a program are annotated
as *must be concrete* or *may be abstract*, since partitioning into *must be concrete*
and *must be abstract* is not possible.

For variables in the *may be abstract* set, it is then decided dynamically whether
a value is symbolic with a procedure known as *tainting*. The property of being
tainted is propagated to other variables that are assigned a tainted value.
Tainting is done at runtime and divm is responsible for the propagation of the
property (it is stored in the metadata layer of an object using `__vm_peek` and
`__vm_poke` - see divine/vm/divm.h).

Taint is propagated through operations: if a tainted value appears as an operand,
the result of the operation is also tainted.

For instance, we have x = a + b; in the program. The variables are first marked
statically at compilation, say 'a' is definitely concrete and 'b' and 'x' are marked
as maybe abstract.
Then divm decides at runtime whether to taint the maybe-abstract values or whether
they are, in fact, concrete (untainted). If 'b' is tainted, then the taint will be
propagated to 'x', as a result of the + operation that had either operand tainted.
If 'b' was not tainted along the way, then it is considered as concrete and adding
two concrete values will produce a concrete value and 'x' will not be marked as tainted.

### Stashing

__lart_stash + __lart_unstash
- in lart{.h,.cpp}, shoved into task.__rst_stash
- done before and after function calls to preserve abstraction of values,
LART can only do computations on concrete values, the values are "lowered"
but remain tainted

### Freezing & Thawing

As the size of a value has to be consistent throughout execution, the data of abstracted
(= symbolic) values has to be stored aside. The abstract value associated with a concrete
value can be accessed through its (unique) address. *Freezing* ("store") takes an address
and the abstract value of the variable that lives on the address and stores the data away.
*Thawing* ("load") takes an address and retrieves the abstract data back.

For example, we have a symbolic integer `x` that holds an arbitrary value. When computing
with `x`, the abstract representation is used. However, when saving `x` back, we have to
remember the constraints that now apply to it, such as it is greater than 0. We *freeze*
the variable `x` and pass its address and the associated abstraction data. The data is
stored away. When we then want to fetch the abstraction data again, we call *thaw* with
the address of `x`.

