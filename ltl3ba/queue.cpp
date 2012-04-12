/***** ltl3ba : queue.c *****/

/* Written by Tomas Babiak, FI MU, Brno, Czech Republic                   */
/* Copyright (c) 2012  Tomas Babiak                                       */
/*                                                                        */

#include "ltl3ba.h"

extern FILE *tl_out;

int is_empty(Queue *q) {
  return (q->size == 0);
}

int is_full(Queue *q) {
  return (q->size == q->max);
}

Queue* create_queue(int max_size) {
  Queue *q;
  
  q = (Queue *) tl_emalloc(sizeof(struct Queue));
  
  if (q != NULL) {
    q->max = max_size;
    q->size = 0;
    q->front = 0;
    q->data = (int*) tl_emalloc(max_size * sizeof(int));
    if (q->data == NULL) {
      tfree(q);
      return NULL;
    }
  }
  return q;
}

void free_queue(Queue *q) {
  if(!q) return;
  if(q->data)
    tfree(q->data);
  tfree(q);
}

int push(Queue *q, int elem) {
  if (is_full(q)) return 0;
  q->data[(q->front+q->size)%q->max] = elem;
  q->size++;
  return 1;
}

int pop(Queue *q) {
  if (is_empty(q))
    return -1;

  int elem = q->data[q->front];
  q->front = (q->front+1)%q->max;
  q->size--;
  return elem;
}

void print_queue(Queue *q) {
  int i, start = 1;
  fprintf(tl_out, "[");
  for (i=0; i<q->size; i++) {
    if (!start)
      fprintf(tl_out, ", ");
    fprintf(tl_out, "%d", q->data[(q->front+i)%q->max]);
  }
  fprintf(tl_out, "]");
}
