/* VERIFY_OPTS: -C,-D"RETURN(x)=(x-2)" */

int main() {
    return RETURN(2);
}
