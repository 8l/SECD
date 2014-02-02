#include "memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/*
 *      A short description of SECD memory layout
 *                                          last updated: 12 Jan 2014
 *
 *  TODO
 */

/* internal declarations */
void free_array(secd_t *secd, cell_t *this);


inline static size_t bytes_to_cell(size_t bytes) {
    size_t ncell = bytes / sizeof(cell_t);
    if (bytes % sizeof(cell_t))
        ++ncell;
    return ncell;
}

/* http://en.wikipedia.org/wiki/Jenkins_hash_function */
static hash_t jenkins_hash(const char *key, size_t len) {
    uint32_t hash, i;
    for(hash = i = 0; i < len; ++i)
    {
        hash += key[i];
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

hash_t memhash(const char *key, size_t len) {
    return jenkins_hash(key, len);
}

/*
 *  Cell memory management
 */

cell_t *init_with_copy(secd_t *secd,
                       cell_t *__restrict cell,
                       const cell_t *__restrict with)
{
    *cell = *with;

    cell->nref = 0;
    switch (cell_type(with)) {
      case CELL_CONS: case CELL_FRAME:
        share_cell(secd, with->as.cons.car);
        share_cell(secd, with->as.cons.cdr);
        break;
      case CELL_REF:
        share_cell(secd, with->as.ref);
        break;
      case CELL_ARRAY:
        share_cell(secd, arr_meta(with->as.arr.data));
        break;
      case CELL_STR:
        share_cell(secd, arr_meta((cell_t *)strmem((cell_t *)with)));
        break;
      case CELL_ERROR:
      case CELL_ATOM:
      case CELL_UNDEF:
        break;
      case CELL_ARRMETA: case CELL_FREE:
        return new_error(secd, "trying to initialize with CELL_ARRMETA");
    }
    return cell;
}

void free_atom(cell_t *cell) {
    switch (cell->as.atom.type) {
      case ATOM_SYM:
        if (cell->as.atom.as.sym.size != DONT_FREE_THIS)
            free((char *)cell->as.atom.as.sym.data); break;
      default: return;
    }
}

cell_t *drop_dependencies(secd_t *secd, cell_t *c) {
    enum cell_type t = cell_type(c);
    switch (t) {
      case CELL_ATOM:
        free_atom(c);
        break;
      case CELL_FRAME:
      case CELL_CONS:
        if (not_nil(c)) {
            drop_cell(secd, get_car(c));
            drop_cell(secd, get_cdr(c));
        }
        break;
      case CELL_STR:
      case CELL_ARRAY: {
        cell_t *arr = arr_ref(c, 0);
        cell_t *meta = arr_meta(arr);
        -- meta->nref;
        if (0 == meta->nref) {
            if (meta->as.mcons.cells) {
                size_t size = arrmeta_size(secd, meta);
                size_t i;

                /* free the items */
                for (i = 0; i < size; ++i) {
                    /* don't free uninitialized */
                    if (cell_type(arr + i) != CELL_UNDEF)
                        drop_dependencies(secd, arr + i);
                }
            }

            free_array(secd, arr);
        }
        } break;
      case CELL_REF:
        drop_cell(secd, c->as.ref);
        break;
      case CELL_ERROR:
      case CELL_UNDEF:
        return c;
      default:
        return new_error(secd, "drop_dependencies: unknown cell_type 0x%x", t);
    }
    return c;
}


cell_t *pop_free(secd_t *secd) {
    cell_t *cell;
    if (not_nil(secd->free)) {
        /* take a cell from the list */
        cell = secd->free;
        secd->free = get_cdr(secd->free);
        if (secd->free)
            secd->free->as.cons.car = SECD_NIL;
        memdebugf("NEW [%ld]\n", cell_index(secd, cell));
        -- secd->free_cells;
    } else {
        assert(secd->free_cells == 0,
               "pop_free: free=NIL when nfree=%zd\n", secd->free_cells);
        /* move fixedptr */
        if (secd->fixedptr >= secd->arrayptr)
            return &secd_out_of_memory;

        cell = secd->fixedptr;
        ++ secd->fixedptr;
        memdebugf("NEW [%ld] ++\n", cell_index(secd, cell));
    }

    cell->type = CELL_UNDEF;
    cell->nref = 0;
    return cell;
}

void push_free(secd_t *secd, cell_t *c) {
    assertv(c, "push_free(NULL)");
    assertv(c->nref == 0,
            "push_free: [%ld]->nref is %ld\n", cell_index(secd, c), (long)c->nref);
    assertv(c < secd->fixedptr, "push_free: Trying to free array cell");

    if (c + 1 < secd->fixedptr) {
        /* just add the cell to the list secd->free */
        c->type = CELL_FREE;
        c->as.cons.car = SECD_NIL;
        c->as.cons.cdr = secd->free;

        if (not_nil(secd->free))
            secd->free->as.cons.car = c;
        secd->free = c;

        ++secd->free_cells;
        memdebugf("FREE[%ld], %zd free\n",
                cell_index(secd, c), secd->free_cells);
    } else {
        memdebugf("FREE[%ld] --\n", cell_index(secd, c));
        --c;

        while (c->type == CELL_FREE) {
            /* it is a cell adjacent to the free space */
            if (c != secd->free) {
                cell_t *prev = c->as.cons.car;
                cell_t *next = c->as.cons.cdr;
                if (not_nil(prev)) {
                    prev->as.cons.cdr = next;
                }
                if (not_nil(next)) {
                    next->as.cons.car = prev;
                }
            } else {
                cell_t *next = c->as.cons.cdr;
                if (not_nil(next))
                    next->as.cons.car = SECD_NIL;

                secd->free = next;
            }
            memdebugf("FREE[%ld] --\n", cell_index(secd, c));
            --c;
            --secd->free_cells;
        }

        secd->fixedptr = c + 1;
    }
}

static inline cell_t*
init_cons(secd_t *secd, cell_t *cell, cell_t *car, cell_t *cdr) {
    cell->type = CELL_CONS;
    cell->as.cons.car = share_cell(secd, car);
    cell->as.cons.cdr = share_cell(secd, cdr);
    return cell;
}

static cell_t *init_meta(secd_t __unused *secd, cell_t *cell, cell_t *prev, cell_t *next) {
    cell->type = CELL_ARRMETA;
    cell->as.mcons.prev = prev;
    cell->as.mcons.next = next;
    cell->as.mcons.cells = false;
    return cell;
}

/*
 *      Array memory management
 */

/* checks if the array described by the metadata cons is free */
static inline bool is_array_free(secd_t *secd, cell_t *metacons) {
    if (metacons == secd->arrlist) return false;
    return metacons->nref == 0;
}
static inline void mark_free(cell_t *metacons) { metacons->nref = 0; }

cell_t *alloc_array(secd_t *secd, size_t size) {
    /* look through the list of arrays */
    cell_t *cur = secd->arrlist;
    while (not_nil(mcons_next(cur))) {
        if (is_array_free(secd, cur)) {
            size_t cursize = arrmeta_size(secd, cur);
            if (cursize >= size) {
                /* allocate this gap */
                if (cursize > size + 1) {
                    cell_t *newmeta = cur + size + 1;
                    init_meta(secd, newmeta, mcons_prev(cur), cur);
                    mark_free(newmeta);
                }
                return cur + 1;
            }
        }
        cur = mcons_next(cur);
    }

    /* no chunks of sufficient size found, move secd->arrayptr */
    if (secd->arrayptr - secd->fixedptr <= (int)size)
        return &secd_out_of_memory;

    /* create new metadata cons at arrayptr - size - 1 */
    cell_t *oldmeta = secd->arrayptr;

    cell_t *meta = oldmeta - size - 1;
    init_meta(secd, meta, oldmeta, SECD_NIL);

    oldmeta->as.mcons.next = meta;

    secd->arrayptr = meta;
    return meta + 1;
}

void free_array(secd_t *secd, cell_t *this) {
    assertv(this <= secd->arrlist, "free_array: tried to free arrlist");
    assertv(secd->arrayptr < this, "free_array: not an array");

    cell_t *meta = arr_meta(this);
    cell_t *prev = mcons_prev(meta);

    if (meta != secd->arrayptr) {
        if (is_array_free(secd, prev)) {
            /* merge with the previous array */
            cell_t *pprev = prev->as.mcons.prev;
            pprev->as.mcons.next = meta;
            meta->as.mcons.prev = pprev;
        }

        cell_t *next = mcons_next(meta);
        if (is_array_free(secd, next)) {
            /* merge with the next array */
            cell_t *newprev = meta->as.mcons.prev;
            next->as.mcons.prev = newprev;
            newprev->as.mcons.next = next;
        }
        mark_free(meta);
    } else {
        /* move arrayptr into the array area */
        prev->as.mcons.next = SECD_NIL;
        secd->arrayptr = prev;

        if (is_array_free(secd, prev)) {
            /* at most one array after 'arr' may be free */
            cell_t *pprev = prev->as.mcons.prev;
            pprev->as.mcons.next = SECD_NIL;
            secd->arrayptr = pprev;
        }
    }
}

void print_array_layout(secd_t *secd) {
    errorf(";; Array heap layout:\n");
    errorf(";;  arrayptr = %ld\n", cell_index(secd, secd->arrayptr));
    errorf(";;  arrlist  = %ld\n", cell_index(secd, secd->arrlist));
    errorf(";; Array list is:\n");
    cell_t *cur = secd->arrlist;
    while (not_nil(mcons_next(cur))) {
        cur = mcons_next(cur);
        errorf(";;  %ld\t%ld (size=%zd,\t%s)\n", cell_index(secd, cur), 
                cell_index(secd, mcons_prev(cur)), arrmeta_size(secd, cur), 
                (is_array_free(secd, cur)? "free" : "used"));
    }
}

cell_t *fill_array(secd_t *secd, cell_t *arr, cell_t *with) {
    cell_t *data = arr->as.arr.data;
    size_t len = arr_size(secd, arr);
    size_t i;

    for (i = 0; i < len; ++i)
        init_with_copy(secd, data + i, with);

    return arr;
}

/*
 *      "Constructors"
 */
cell_t *new_cons(secd_t *secd, cell_t *car, cell_t *cdr) {
    return init_cons(secd, pop_free(secd), car, cdr);
}

cell_t *new_frame(secd_t *secd, cell_t *syms, cell_t *vals) {
    cell_t *cons = new_cons(secd, syms, vals);
    cons->type = CELL_FRAME;
    return cons;
}

cell_t *new_number(secd_t *secd, int num) {
    cell_t *cell = pop_free(secd);
    cell->type = CELL_ATOM;
    cell->as.atom.type = ATOM_INT;
    cell->as.atom.as.num = num;
    return cell;
}

cell_t *new_symbol(secd_t *secd, const char *sym) {
    cell_t *cell = pop_free(secd);
    cell->type = CELL_ATOM;
    cell->as.atom.type = ATOM_SYM;
    cell->as.atom.as.sym.size = strlen(sym);
    cell->as.atom.as.sym.data = strdup(sym);
    return cell;
}

typedef union {
    char *as_cstr;
    cell_t *as_cell;
} arrref_t;

static cell_t *init_strref(secd_t *secd, cell_t *cell, arrref_t mem, size_t size) {
    cell->type = CELL_STR;

    share_cell(secd, arr_meta(mem.as_cell));
    cell->as.str.data = mem.as_cstr;
    cell->as.str.hash = memhash(mem.as_cstr, size);
    return cell;
}

cell_t *new_strref(secd_t *secd, arrref_t mem, size_t size) {
    cell_t *ref = pop_free(secd);
    assert_cell(ref, "new_strref: allocation failed");
    return init_strref(secd, ref, mem, size);
}

cell_t *new_string_of_size(secd_t *secd, size_t size) {
    arrref_t mem;
    mem.as_cell = alloc_array(secd, bytes_to_cell(size));
    assert_cell(mem.as_cell, "new_string_of_size: alloc failed");

    return new_strref(secd, mem, size);
}

cell_t *new_string(secd_t *secd, const char *str) {
    size_t size = strlen(str) + 1;
    cell_t *cell = new_string_of_size(secd, size);

    strcpy(strmem(cell), str);
    return cell;
}

cell_t *new_op(secd_t *secd, opindex_t opind) {
    cell_t *cell = pop_free(secd);
    cell->type = CELL_ATOM;
    cell->as.atom.type = ATOM_OP;
    cell->as.atom.as.op = opind;
    return cell;
}

cell_t *new_const_clone(secd_t *secd, const cell_t *from) {
    if (is_nil(from)) return NULL;

    cell_t *clone = pop_free(secd);
    return init_with_copy(secd, clone, from);
}

cell_t *new_clone(secd_t *secd, cell_t *from) {
    cell_t *clone = pop_free(secd);
    assert_cell(clone, "new_clone: allocation failed");

    return init_with_copy(secd, clone, from);
}

static cell_t *init_error(cell_t *cell, const char *buf) {
    cell->type = CELL_ERROR;
    cell->as.err.len = strlen(buf);
    cell->as.err.msg = strdup(buf);
    return cell;
}

cell_t *new_errorv(secd_t *secd, const char *fmt, va_list va) {
#define MAX_ERROR_SIZE  512
    char buf[MAX_ERROR_SIZE];
    vsnprintf(buf, MAX_ERROR_SIZE, fmt, va);
    return init_error(pop_free(secd), buf);
}

cell_t *new_error(secd_t *secd, const char *fmt, ...) {
    va_list va;
    va_start(va, fmt);
    cell_t *cell = new_errorv(secd, fmt, va);
    va_end(va);
    return cell;
}

cell_t *new_error_with(
        secd_t *secd, __unused cell_t *preverr, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    cell_t *err = new_errorv(secd, fmt, va);
    va_end(va);
    return err;
}

cell_t *new_array(secd_t *secd, size_t size) {
    /* try to allocate memory */
    cell_t *mem = alloc_array(secd, size);
    assert_cell(mem, "new_array: memory allocation failed");

    cell_t *arr = pop_free(secd);
    arr->type = CELL_ARRAY;
    arr->as.arr.data = mem;
    arr_meta(mem)->nref = 1;
    arr_meta(mem)->as.mcons.cells = true;
    return arr;
}


cell_t *free_cell(secd_t *secd, cell_t *c) {
    push_free(secd, drop_dependencies(secd, c));
    return SECD_NIL;
}

inline static cell_t *push(secd_t *secd, cell_t **to, cell_t *what) {
    cell_t *newtop = new_cons(secd, what, *to);
    drop_cell(secd, *to);
    return (*to = share_cell(secd, newtop));
}

inline static cell_t *pop(secd_t *secd, cell_t **from) {
    cell_t *top = *from;
    assert(not_nil(top), "pop: stack is empty");
    assert(is_cons(top), "pop: not a cons");

    cell_t *val = share_cell(secd, get_car(top));
    *from = share_cell(secd, get_cdr(top));
    drop_cell(secd, top);
    return val;
}

cell_t *push_stack(secd_t *secd, cell_t *newc) {
    cell_t *top = push(secd, &secd->stack, newc);
    memdebugf("PUSH S[%ld (%ld, %ld)]\n", cell_index(secd, top),
                        cell_index(secd, get_car(top)), cell_index(secd, get_cdr(top)));
    return top;
}

cell_t *pop_stack(secd_t *secd) {
    cell_t *cell = pop(secd, &secd->stack);
    memdebugf("POP S[%ld]\n", cell_index(secd, cell));
    return cell; // don't forget to drop_call(result)
}

cell_t *set_control(secd_t *secd, cell_t *opcons) {
    assert(is_cons(opcons),
           "set_control: failed, not a cons at [%ld]\n", cell_index(secd, opcons));
    if (! is_control_compiled(secd, opcons)) {
        opcons = compile_control_path(secd, opcons);
        assert_cell(opcons, "set_control: failed to compile control path");
    }
    return (secd->control = share_cell(secd, opcons));
}

cell_t *pop_control(secd_t *secd) {
    return pop(secd, &secd->control);
}

cell_t *push_dump(secd_t *secd, cell_t *cell) {
    cell_t *top = push(secd, &secd->dump, cell);
    memdebugf("PUSH D[%ld] (%ld, %ld)\n", cell_index(secd, top),
            cell_index(secd, get_car(top)),
            cell_index(secd, get_cdr(top)));
    ++secd->used_dump;
    return top;
}

cell_t *pop_dump(secd_t *secd) {
    cell_t *cell = pop(secd, &secd->dump);
    memdebugf("POP D[%ld]\n", cell_index(secd, cell));
    --secd->used_dump;
    return cell;
}

/*
 *     List/vector/string utilities
 */

size_t list_length(secd_t *secd, cell_t *lst) {
    size_t res = 0;
    while (not_nil(lst)) {
        if (! is_cons(lst))
            break;
        lst = list_next(secd, lst);
        ++res;
    }
    return res;
}

cell_t *vector_from_list(secd_t *secd, cell_t *lst) {
    size_t i;
    size_t len = list_length(secd, lst);
    cell_t *arr = new_array(secd, len);
    assert_cell(arr, "vector_from_list: allocation failed");

    for (i = 0; i < len; ++i) {
        if (is_nil(lst)) break;
        init_with_copy(secd, arr_ref(arr, i), get_car(lst));
        lst = list_next(secd, lst);
    }
    return arr;
}


void init_mem(secd_t *secd, cell_t *heap, size_t size) {
    secd->begin = heap;
    secd->end = heap + size;

    secd->fixedptr = secd->begin;
    secd->arrayptr = secd->end - 1;

    secd->arrlist = secd->arrayptr;
    init_meta(secd, secd->arrlist, SECD_NIL, SECD_NIL);
    secd->arrlist->nref = 0;

    secd->used_stack = 0;
    secd->used_dump = 0;
    secd->used_control = 0;
    secd->free_cells = 0;
}
