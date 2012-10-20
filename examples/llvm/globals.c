#include "divine-llvm.h"

int integer = -0x12;
unsigned unsignedInt = 125U;

float fp = 2.456;
double dfp = .25e4;

char character = 'W';

int array[] = {1,2,3,4,5};

int *pointer = &array;

int sum(int a, int b) {
    return a+b;
}
int (*fncPointer) (int,int) = &sum;

char* stringPtr = "This string is stored in Interpret::constGlobalmem, "
                  "and pointer pointing to it is allocated in Interpret::globalmem";
char string[] = "This string is stored in Interpret::globalmem.";


typedef enum {one = 1, two, tree} Enumeration;

Enumeration enumA = one;
Enumeration enumB = 2;

typedef struct {
    int number;
    char character;
    bool boolean;
} Structure;

Structure structA = {20, 'S', false};
Structure structB = { .character = 'U', .boolean = true};


int main() {

  // integer
    assert(integer == -18);
    assert(unsignedInt == 125);

  // floating point
    assert(1 < fp < 3);
    assert(dfp == 2500);

  // character
    assert(character == 'W');

  // array & pointer
    for (unsigned i = 0; i<5; ++i) {
        assert(array[i] == i+1);
        assert(*(pointer + i) == i+1);
    }

  // function pointer
    assert(fncPointer(2,3) == 5);

  // string
    trace(string);
    trace(stringPtr);
    trace("This string is stored in Interpret::constGlobalmem.");

  // enum
    assert(enumA == 1);
    assert(enumB == two);

  // structure
    assert(structA.number == 20);
    assert(structB.character == 'U');
    assert(structB.boolean);
    // ...

  return 0;
}
