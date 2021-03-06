/* stack.h - definitions and interfaces of stack
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#ifndef STACK_H
#define STACK_H

#include <stddef.h>

struct stack {
    struct stack *p;
};

/* Return data of type data_struct associated with a stack object pointed
 * by stack_ptr. data_struct must have a field of type struct stack with a
 * name referenced by data_stack */
#define stack_data(stack_ptr, data_struct, data_stack) \
    ((data_struct *) ((unsigned long)(stack_ptr) - offsetof(data_struct, data_stack)))

/* initialize stack */
void stack_init(struct stack **top);

/* push new element into stack */
void stack_push(struct stack **top, struct stack *new);

/* pop element from stack */
struct stack * stack_pop(struct stack **top);

/* pop element from stack and free it using fr() */
void stack_pop_free(struct stack **top, void (*fr) (struct stack *s));

/* free entire stack, each element is freed using fr() */
void stack_rfree(struct stack **top, void (*fr) (struct stack *s));

#endif /* STACK_H */

