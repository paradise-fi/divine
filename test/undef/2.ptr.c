#include <divine.h>

int main() {
    int *ptrundef;
    int x = *ptrundef; /* ERROR */
    return x;
}
