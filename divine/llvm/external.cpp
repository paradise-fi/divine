// -*- C++ -*- (c) 2011 Petr Rockai <me@mornfall.net>
// Implementation of "external" functions that the LLVM bytecode may want to call.

#ifdef HAVE_LLVM

#include <divine/llvm/interpreter.h>

using namespace llvm;
using namespace divine::llvm;

typedef std::vector<GenericValue> Args;

typedef GenericValue (*Builtin)(Interpreter *i,
                                const FunctionType *,
                                const Args &);

static GenericValue builtin_exit(Interpreter *, const FunctionType *, const Args &)
{
    // XXX mark the open state as final
    assert_die();
}

static GenericValue builtin_assert(Interpreter *, const FunctionType *, const Args &args)
{
    if (!args[0].IntVal) {
        // XXX we should mark the current state with an "assertion failed" flag
        // and make those goals, so we can use reachability to search for such
        // states and generate corresponding counterexample traces
        std::cerr << "assertion failed" << std::endl;
    }
    return GenericValue();
}

static GenericValue builtin_malloc(Interpreter *interp, const FunctionType *, const Args &args)
{
    // This yields two different successors, one with a NULL return and another
    // where the memory was actually allocated (the alloca/malloc arenas are
    // actually shared, (TODO) at least for now)
    switch (interp->_alternative) {
        case 0: { // normal allocation here
            int size = args[0].IntVal.getZExtValue();
            Arena::Index mem = interp->arena.allocate(size);
            return PTOGV( reinterpret_cast< void * >( intptr_t( mem ) ) );
        }
        case 1:
            return PTOGV( 0 );
    }
    assert_die();
}

static GenericValue builtin_amb(Interpreter *interp, const FunctionType *, const Args &args)
{
    GenericValue GV;
    GV.IntVal = APInt(32, interp->_alternative);
    return GV;
}

static GenericValue builtin_free(Interpreter *interp, const FunctionType *, const Args &args)
{
    Arena::Index idx = intptr_t(GVTOP(args[0]));
    interp->arena.free(idx);
    return GenericValue();
}

static GenericValue builtin_thread_create(Interpreter *, const FunctionType *, const Args &args)
{
    // XXX this should fork off a new thread, using a specified function as an
    // entry point; the thread ceases to exist as soon as the supplied function
    // exits; of course, before this can be implemented, the interpreter and
    // its state vector need to be rigged with thread descriptors and ability
    // to interleave thread execution nondeterministically
    assert_die();
}

static GenericValue builtin_fork(Interpreter *, const FunctionType *, const Args &args)
{
    // XXX this should fork off a new process; again as with threads, this
    // needs substantial rigging in the interpreter code and data structures;
    // process interleaving may be much coarser than with threads, since there
    // is no shared memory between processes (message passing may need to be
    // implemented too, to make this useful at all, though)
    assert_die();
}

// From LLVM.
// int sprintf(char *, const char *, ...)
static GenericValue builtin_sprintf(Interpreter *interp, const FunctionType *FT,
                                    const std::vector<GenericValue> &Args)
{
  char *OutputBuffer = (char *)GVTOP(Args[0]);
  const char *FmtStr = (const char *)GVTOP(Args[1]);
  unsigned ArgNo = 2;

  // printf should return # chars printed.  This is completely incorrect, but
  // close enough for now.
  GenericValue GV;
  GV.IntVal = APInt(32, strlen(FmtStr));
  while (1) {
    switch (*FmtStr) {
    case 0: return GV;             // Null terminator...
    default:                       // Normal nonspecial character
      sprintf(OutputBuffer++, "%c", *FmtStr++);
      break;
    case '\\': {                   // Handle escape codes
      sprintf(OutputBuffer, "%c%c", *FmtStr, *(FmtStr+1));
      FmtStr += 2; OutputBuffer += 2;
      break;
    }
    case '%': {                    // Handle format specifiers
      char FmtBuf[100] = "", Buffer[1000] = "";
      char *FB = FmtBuf;
      *FB++ = *FmtStr++;
      char Last = *FB++ = *FmtStr++;
      unsigned HowLong = 0;
      while (Last != 'c' && Last != 'd' && Last != 'i' && Last != 'u' &&
             Last != 'o' && Last != 'x' && Last != 'X' && Last != 'e' &&
             Last != 'E' && Last != 'g' && Last != 'G' && Last != 'f' &&
             Last != 'p' && Last != 's' && Last != '%') {
        if (Last == 'l' || Last == 'L') HowLong++;  // Keep track of l's
        Last = *FB++ = *FmtStr++;
      }
      *FB = 0;

      switch (Last) {
      case '%':
        memcpy(Buffer, "%", 2); break;
      case 'c':
        sprintf(Buffer, FmtBuf, uint32_t(Args[ArgNo++].IntVal.getZExtValue()));
        break;
      case 'd': case 'i':
      case 'u': case 'o':
      case 'x': case 'X':
        if (HowLong >= 1) {
          if (HowLong == 1 &&
              interp->getTargetData()->getPointerSizeInBits() == 64 &&
              sizeof(long) < sizeof(int64_t)) {
            // Make sure we use %lld with a 64 bit argument because we might be
            // compiling LLI on a 32 bit compiler.
            unsigned Size = strlen(FmtBuf);
            FmtBuf[Size] = FmtBuf[Size-1];
            FmtBuf[Size+1] = 0;
            FmtBuf[Size-1] = 'l';
          }
          sprintf(Buffer, FmtBuf, Args[ArgNo++].IntVal.getZExtValue());
        } else
          sprintf(Buffer, FmtBuf,uint32_t(Args[ArgNo++].IntVal.getZExtValue()));
        break;
      case 'e': case 'E': case 'g': case 'G': case 'f':
        sprintf(Buffer, FmtBuf, Args[ArgNo++].DoubleVal); break;
      case 'p':
        sprintf(Buffer, FmtBuf, (void*)GVTOP(Args[ArgNo++])); break;
      case 's':
        sprintf(Buffer, FmtBuf, (char*)GVTOP(Args[ArgNo++])); break;
      default:
        errs() << "<unknown printf code '" << *FmtStr << "'!>";
        ArgNo++; break;
      }
      size_t Len = strlen(Buffer);
      memcpy(OutputBuffer, Buffer, Len + 1);
      OutputBuffer += Len;
      }
      break;
    }
  }
  return GV;
}

// A limited version of printf, might be useful for last-resort model
// debugging. Traces are shown whenever the model checker visits the given
// state.
static GenericValue builtin_trace(Interpreter *interp, const FunctionType *FT,
                                  const std::vector<GenericValue> &args)
{
    char buffer[10000];
    Args newargs;
    newargs.push_back(PTOGV((void*)&buffer[0]));
    std::copy( args.begin(), args.end(), std::back_inserter( newargs ) );
    builtin_sprintf( interp, FT, newargs );
    std::cerr << "TRACE: " << buffer << std::endl;
    return GenericValue();
}

static struct {
    const char *name;
    Builtin fun;
} builtins[] = {
    { "__divine_builtin_amb", builtin_amb },
    { "__divine_builtin_exit", builtin_exit },
    { "__divine_builtin_trace", builtin_trace },
    { "__divine_builtin_assert", builtin_assert },
    { "__divine_builtin_malloc", builtin_malloc },
    { "__divine_builtin_free", builtin_free },
    { "__divine_builtin_thread_create", builtin_thread_create },
    { "__divine_builtin_fork", builtin_fork },
    { 0, 0 }
};

GenericValue Interpreter::callExternalFunction(
    Function *F, const std::vector<GenericValue> &ArgVals)
{
    /* std::string mangled = "__divine_builtin_";
    const FunctionType *FT = F->getFunctionType();
    for (unsigned i = 0, e = FT->getNumContainedTypes(); i != e; ++i)
        mangled += getTypeID(FT->getContainedType(i));
    mangled + "_" + F->getNameStr(); */
    std::string plain = "__divine_builtin_" + F->getNameStr();

    for ( int i = 0; builtins[i].name; ++i ) {
        if ( plain == builtins[i].name ) {
            return builtins[i].fun(this, F->getFunctionType(), ArgVals);
        }
    }

    std::cerr << "WARNING: failed to resolve symbol " << plain << std::endl;

    return GenericValue();
}


bool Interpreter::alternatives( int alt )
{
    Function *F;
    Instruction &I = nextInstruction();

    /* non-call/invoke insns are deterministic */
    if (!isa<CallInst>(I) && !isa<InvokeInst>(I))
        return alt < 1;
    else { // extract the function
        CallSite cs(&I);
        F = cs.getCalledFunction();
    }

    assert( F );
    if (!F->isDeclaration())
        return alt < 1; // not a builtin, deterministic

    std::string plain = "__divine_builtin_" + F->getNameStr();
    for ( int i = 0; builtins[i].name; ++i ) {
        if ( plain == builtins[i].name ) {
            if (builtins[i].fun == builtin_malloc)
                return alt < 2; /* malloc has 2 different returns */
            if (builtins[i].fun == builtin_amb)
                return alt < 2; /* amb has 2 different returns (0 and 1) */

            // everything else is deterministic as well
            return alt < 1;
        }
    }

    std::cerr << "WARNING: failed to resolve symbol " << plain << std::endl;
    return alt < 1;
}

#endif
