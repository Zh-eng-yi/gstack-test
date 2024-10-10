#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <mprompt.h>
#include <stdatomic.h>

#define N 1000       // max active async workers
#define M 1000000    // total number of requests

static void* await_result(mp_resume_t* r, void* arg) {
  (void)(arg);
  return r;  // instead of resuming ourselves, we return the resumption as a "suspended async computation" (A)
}

static void* async_worker(mp_prompt_t* parent, void* arg) {
  (void)(arg);
  // start a fresh worker
  // ... do some work
  intptr_t partial_result = 0;
  // and await some request; we do this by yielding up to our prompt and running `await_result` (in the parent context!)
  mp_yield( parent, &await_result, NULL );
  // when we are resumed at some point, we do some more work 
  // ... do more work
  partial_result++;
  // and return with the result (B)
  return (void*)(partial_result);
}

static void async_workers(void) {
  mp_resume_t** workers = (mp_resume_t**)calloc(N,sizeof(mp_resume_t*));  // allocate array of N resumptions
  intptr_t count = 0;
  for( int i = 0; i < M+N; i++) {  // perform M connections
    int j = i % N;               // pick an active worker
    // if the worker is actively waiting (suspended), resume it
    if (workers[j] != NULL) {  
      count += (intptr_t)mp_resume(workers[j], NULL);  // (B)
      workers[j] = NULL;
    }
    // and start a fresh worker and wait for its first yield (suspension). 
    // the worker returns its own resumption as a result.
    if (i < M) {
      workers[j] = (mp_resume_t*)mp_prompt( &async_worker, NULL );  // (A)
    }
  }
  printf("ran %zd workers\n", count);
}

#define my_atomic(name) atomic_##name
#define my_atomic_load(p) my_atomic(load)(p)
#define my_atomic_load_ptr(tp,p) my_atomic_load(p)
typedef struct my_type_s {
  bool a;
  int b;
} my_type_t;

#include <string.h>

int main() {
  // async_workers();
  _Atomic(my_type_t *)my_ptr = malloc(sizeof(my_type_t));
  my_ptr->a = false;
  my_ptr->b = 520;
  my_type_t *second_ptr = my_atomic_load_ptr(my_type_t, &my_ptr);
  printf("read second ptr: %s\n", second_ptr->a ? "true" : "false");
  printf("read second ptr: %d\n", second_ptr->b);
  free(my_ptr);
  return 0;
}
