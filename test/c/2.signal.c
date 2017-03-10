#include <signal.h>

typedef void (*func)(int);

void f(int i){}

int main()
{
    int sig = 15;
    func ret = signal(sig, f);
    return 0;
}
