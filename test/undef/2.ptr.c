#include <sys/divm.h>

int main() {
    int *ptrundef;
    int x = *ptrundef; /* ERROR */
    return x;
}
