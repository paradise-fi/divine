/* TAGS: c threads */

/*
    Copyright (C) Dmitry Vyukov. All rights reserved.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#define NUM_THREADS 2

typedef struct SafeStackItem
{
    _Atomic int Value;
    _Atomic int Next;
} SafeStackItem;

typedef struct SafeStack
{
    SafeStackItem array[3];
    _Atomic int head;
    _Atomic int count;
} SafeStack;

pthread_t threads[NUM_THREADS];
SafeStack stack;

void Init(int pushCount)
{
    int i;
    __c11_atomic_store(&stack.count, pushCount, __ATOMIC_SEQ_CST);
    __c11_atomic_store(&stack.head, 0, __ATOMIC_SEQ_CST);
    for (i = 0; i < pushCount - 1; i++)
    {
        __c11_atomic_store(&stack.array[i].Next, i + 1, __ATOMIC_SEQ_CST);
    }
    __c11_atomic_store(&stack.array[pushCount - 1].Next, -1, __ATOMIC_SEQ_CST);
}

int Pop(void)
{
    while (__c11_atomic_load(&stack.count, __ATOMIC_SEQ_CST) > 1)
    {
        int head1 = __c11_atomic_load(&stack.head, __ATOMIC_SEQ_CST);
        int next1 = __c11_atomic_exchange(&stack.array[head1].Next, -1, __ATOMIC_SEQ_CST);

        if (next1 >= 0)
        {
            int head2 = head1;
            if (__c11_atomic_compare_exchange_strong(&stack.head, &head2, next1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
            {
                __c11_atomic_fetch_sub(&stack.count, 1, __ATOMIC_SEQ_CST);
                return head1;
            }
            else
            {
                __c11_atomic_exchange(&stack.array[head1].Next, next1, __ATOMIC_SEQ_CST);
            }
        }
    }

    return -1;
}

void Push(int index)
{
    int head1 = __c11_atomic_load(&stack.head, __ATOMIC_SEQ_CST);
    do
    {
        __c11_atomic_store(&stack.array[index].Next, head1, __ATOMIC_SEQ_CST);

    } while (!(__c11_atomic_compare_exchange_strong(&stack.head, &head1, index, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)));
    __c11_atomic_fetch_add(&stack.count, 1, __ATOMIC_SEQ_CST);
}


void* thread(void* arg)
{
    size_t i;
    int idx = (int)(size_t)arg;
    for (i = 0; i < 2; i++)
    {
        int elem;
        for (;;)
        {
            elem = Pop();
            if (elem >= 0)
                break;
        }

        stack.array[elem].Value = idx;
        assert(stack.array[elem].Value == idx);

        Push(elem);
    }
    return NULL;
}

int main(void)
{
    int i;
    Init(NUM_THREADS);
    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_create(&threads[i], NULL, thread, (void*) i);
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}

