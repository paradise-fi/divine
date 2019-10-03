/* TAGS: min c++ */
/* EXPECT: --result error --symbol _Unwind_RaiseException */

__attribute__((noinline)) void bar() { throw 0; }

__attribute__((nothrow, noinline)) void foo() { bar(); }

int main() {
    try {
        foo();
    } catch ( ... ) { }
}
