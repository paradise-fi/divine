#ifndef _GLIBCXX_ATOMIC_WORD_H
#define _GLIBCXX_ATOMIC_WORD_H  1

typedef int _Atomic_word;

#define _GLIBCXX_READ_MEM_BARRIER __asm __volatile ("mb":::"memory")
#define _GLIBCXX_WRITE_MEM_BARRIER __asm __volatile ("wmb":::"memory")

#endif
