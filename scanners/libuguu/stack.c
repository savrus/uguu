/* stack.c - stack
 *
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include "stack.h"
#include "log.h"

void stack_push(struct stack **top, struct stack *new)
{
    LOG_ASSERT(top != NULL, "Bad arguments\n");
    
    new->p = *top;
    *top = new;
}

struct stack * stack_pop(struct stack **top)
{
    struct stack *s;
    LOG_ASSERT(top != NULL, "Bad arguments\n");
    LOG_ASSERT(*top != NULL, "Stack underflow\n");

    s = *top;
    *top = (*top)->p;
    return s;
}

void stack_pop_free(struct stack **top, void (*fr) (struct stack *s))
{
    struct stack *s;
    LOG_ASSERT(top != NULL, "Bad arguments\n");
    LOG_ASSERT(*top != NULL, "Stack underflow\n");

    s = stack_pop(top);
    fr(s);
}

void stack_free(struct stack **top, void (*fr) (struct stack *s))
{
    LOG_ASSERT(top != NULL, "Bad arguments\n");
    
    while (*top != NULL)
        stack_pop_free(top, fr);
}

