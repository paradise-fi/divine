#ifndef _DEB_HH_
 #define _DEB_HH_


 #ifdef LOCAL_DEB
  #ifndef DO_DEB
   #define DO_DEB
  #endif
 #endif
 
 #ifdef DO_DEB
  /* general debugging macro - produces the standard messages about the
   * steps of computation - it should not produce the large amounts
   * of messages (in one function call) */
  #define DEB(x) x
  
  /* debugging of a long evaluation - e. g. of expression.
   * It can produce large amount of messages about variable values */
  #define DEBEVAL(x) x
  
  /* debugging of a function - it should produce the report about
   * the start of the function (with the parameters) and about the end
   * of the function */
  #define DEBFUNC(x) x
  
  /* In `x' we check whether some contidion holds. We can also exit the
   * program, if the condition doesn't hold */
  #define DEBCHECK(x) x
  
  /* Auxiliary debugging message. The message that is only temporary
   * and it doesn't have to write any meaningful message. It can
   * for example serve of determine, whether the particular part
   * of a code has been executed or not. */
  #define DEBAUX(x) x
 #else
  #define DEB(x) 
  #define DEBEVAL(x) 
  #define DEBFUNC(x) 
  #define DEBCHECK(x)
  #define DEBAUX(x) 
 #endif
#endif

