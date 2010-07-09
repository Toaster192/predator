#include "../sl.h"
#include <stdlib.h>

struct item {
    struct item *next;
};

struct master_item {
    struct item             *slave;
    struct master_item      *next;
};

struct item* alloc_or_die(void)
{
    struct item *pi = malloc(sizeof(*pi));
    if (pi)
        return pi;
    else
        abort();
}

struct master_item* alloc_or_die_master(void)
{
    struct master_item *pm = malloc(sizeof(*pm));
    if (pm)
        return pm;
    else
        abort();
}

struct item* create_sll_item(struct item *next) {
    struct item *pi = alloc_or_die();
    pi->next = next;
    return pi;
}

struct item* create_sll(void)
{
    struct item *sll = create_sll_item(NULL);
    sll = create_sll_item(sll);
    sll = create_sll_item(sll);

    // NOTE: running this on bare metal may cause the machine to swap a bit
    int i;
    for (i = 1; i; ++i)
        sll = create_sll_item(sll);

    // the return will trigger further abstraction (stack frame destruction)
    return sll;
}

struct item* create_slseg(void)
{
    struct item *list = create_sll();
    struct item *sls = list->next;
    free(list);
    return sls;
}

struct master_item* create_master_item(struct master_item *next) {
    struct master_item *pm = alloc_or_die_master();
    pm->slave = create_slseg();
    pm->next  = next;
    return pm;
}

struct master_item* create_shape(void)
{
    struct master_item *item = create_master_item(NULL);
    item = create_master_item(item);
    item = create_master_item(item);

    // NOTE: running this on bare metal may cause the machine to swap a bit
    int i;
    for (i = 1; i; ++i)
        item = create_master_item(item);

    // the return will trigger further abstraction (stack frame destruction)
    ___SL_PLOT_FNC(create_shape);
    return item;
}

struct master_item* create_sane_shape(void)
{
    struct master_item *list = create_shape();
    struct master_item *shape = list->next;
    free(list);
    ___SL_PLOT_FNC(create_sane_shape);
    return shape;
}

int main()
{
    struct master_item *shape = create_sane_shape();

    // trigger a memory leak
    free(shape);

    ___SL_PLOT_FNC(main);
    return 0;
}
