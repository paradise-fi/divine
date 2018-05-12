/* TAGS: c sym */
/* VERIFY_OPTS: --symbolic --sequential -o nofail:malloc */
// from FreePastry source, file Id.java
  /**
   * Internal method for mapping byte[] -> int[]
   *
   * @param material The input byte[]
   * @return THe int[]
   */
/*
  protected static int[] trans(byte[] material) {
    int[] array = new int[nlen];

    for (int j = 0; (j < IdBitLength / 8) && (j < material.length); j++) {
      int k = material[j] & 0xff;
      array[j / 4] |= k << ((j % 4) * 8);
    }

    return array;
  }
*/
extern void __VERIFIER_error(void);
extern void __VERIFIER_assume(int);
void __VERIFIER_assert(int cond) {
  if (!(cond)) {
    ERROR: __VERIFIER_error();
  }
  return;
}
int __VERIFIER_nondet_int();
#define LARGE_INT 1000000

int main() {
  int idBitLength, material_length, nlen;
  int j, k;

  nlen = __VERIFIER_nondet_int();
  idBitLength = __VERIFIER_nondet_int();
  material_length = __VERIFIER_nondet_int();
    if (!( nlen  ==  idBitLength / 32 )) return 0;

  for (j = 0; (j < idBitLength / 8) && (j < material_length); j++) {
    __VERIFIER_assert( 0 <= j);
    __VERIFIER_assert( j < material_length );
    __VERIFIER_assert( 0 <= j/4 );
    __VERIFIER_assert( j/4 < nlen); /* ERROR */
  }

  return 0;
}
