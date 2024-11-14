#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "mprompt.h"
 #include <sys/types.h>
#include "internal/gstack.h"

int main() {
  zz_init();
  mp_gstack_t *g = zz_gstack;

  printf("stack_size: %zd\n", g->stack_size);

  uint8_t *base = g->stack + g->stack_size;

  printf("checking committed size...\n");
  printf("initial_commit: %zd\n", g->initial_commit);
  printf("committed: %zd\n", g->committed);

  printf("accessing the base...\n");
  printf("%04x\n", base[-1]);

  printf("accessing the last committed...\n");
  printf("%04x\n", base[-4096]);

  printf("checking committed size...\n");
  printf("committed: %zd\n", g->committed);

  printf("accessing the first byte that's not committed...\n");
  printf("%04x\n", base[-4097]);

  printf("checking committed size...\n");
  printf("committed: %zd\n", g->committed);

  printf("g address: %p\n", g);
  printf("stack address: %p\n", g->stack);
  printf("diff: %td\n", (void *) g->stack - (void *) g);

  mp_gstack_free(g, false);
  printf("this will give seg fault\n");
  printf("stack_size: %zd\n", g->stack_size);

  // printf("accessing the bottom...\n");
  // printf("%04x\n", g->stack[0]);

  // printf("checking committed size...\n");
  // printf("committed: %zd\n", g->committed);

  // printf("this should give seg fault...\n");
  // printf("%04x\n", g->stack[-1]);

  // uint8_t       extra[1];           // extra allocated (holds the mp_prompt_t structure)
  // printf("size: %zd\n", sizeof(int*));

  // int array[5];
  // printf("size: %ld\n", sizeof(array));

}
