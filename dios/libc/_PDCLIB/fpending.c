#include <stdio_ext.h>
#include <assert.h>

/* Return the number of pending (aka buffered, unflushed)
   bytes on the stream, FP, that is open for writing.  */
size_t __fpending ( FILE *fp )
{
  assert( fp );
  return fp->bufidx;
}
