/* TAGS: sym c */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */


#include <assert.h>


// test correct unstash after casted sym constructor

int main() {
	int x = __sym_val_i32();
	int y = __sym_val_i32();
	int z = x + y;
}
