/* TAGS: min c const */
const int x = 42;

int main() {
    *((int*)&x) = 4; /* ERROR */
}
