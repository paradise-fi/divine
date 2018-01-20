/* TAGS: min c */
union U {
    struct { short a, b; } x;
    int y;
};

int main() {
    union U u;
    u.x.a = 1;
    u.x.b = 2;
    if ( u.y )
        return 0;
    else
        return 1;
}
