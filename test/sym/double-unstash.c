/* TAGS: sym c */
/* VERIFY_OPTS: --symbolic */
/* CC_OPTS: */


#include <assert.h>


// test correct unstash after casted sym constructor

int main() {
	int x = __lamp_any_i32();
	int y = __lamp_any_i32();
	int z = x + y;
}
