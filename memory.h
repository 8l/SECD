#ifndef __SECD_MEM_H__
#define __SECD_MEM_H__

#include "conf.h"
#include "secd.h"

/*
 *   Allocation
 */

typedef  struct secd_memory  secd_memory_t;

cell_t *new_cons(secd_t *secd, cell_t *car, cell_t *cdr);
cell_t *new_frame(secd_t *secd, cell_t *syms, cell_t *vals);
cell_t *new_number(secd_t *secd, int num);
cell_t *new_symbol(secd_t *secd, const char *sym);
cell_t *new_op(secd_t *secd, opindex_t opind);
cell_t *new_clone(secd_t *secd, const cell_t *from);
cell_t *new_error(secd_t *secd, const char *fmt, ...);

cell_t *free_cell(secd_t *, cell_t *c);

cell_t *push_stack(secd_t *secd, cell_t *newc);
cell_t *pop_stack(secd_t *secd);

cell_t *set_control(secd_t *secd, cell_t *opcons);
cell_t *pop_control(secd_t *secd);

cell_t *push_dump(secd_t *secd, cell_t *cell);
cell_t *pop_dump(secd_t *secd);

/*
 * Reference-counting
 */

inline static cell_t *share_cell(secd_t *secd, cell_t *c) {
    if (not_nil(c)) {
        ++c->nref;
        memtracef("share[%ld] %ld\n", cell_index(c), c->nref);
    } else {
        memdebugf("share[NIL]\n");
    }
    return c;
}

inline static cell_t *drop_cell(secd_t *secd, cell_t *c) {
    if (is_nil(c)) {
        memdebugf("drop [NIL]\n");
        return NULL;
    }
    if (c->nref <= 0) {
        assert(c->nref > 0, "drop_cell[%ld]: negative", cell_index(secd, c));
    }

    -- c->nref;
    memtracef("drop [%ld] %ld\n", cell_index(c), c->nref);
    if (c->nref) return c;
    return free_cell(secd, c);
}

/*
 *
 */

void init_mem(secd_t *secd, cell_t *heap, size_t size);

#endif // __SECD_MEM_H__
