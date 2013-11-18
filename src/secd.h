#ifndef __SECD_H__
#define __SECD_H__

#include <stdint.h>

/*
 * fixed-size cells
 */
typedef  struct cell  cell_t;

/*
 * indeces for cells
 */
typedef  struct ptr   ptr_t;

/*
 * the SECD machine
 */
typedef  struct secd            secd_t;
typedef  struct secdstat        secdstat_t;
typedef  struct chunk_manager   chunkman_t;

struct secd {
    ptr_t stack, env, ctrl, dump;
    void* chunk[CHUNK_COUNT];

    chunkman_t *used;
    chunkman_t *free;
    cell_t *freecell;  // list;

    secdstat_t stat;
};


typedef enum {
    CELLSZ_FIX = 0,
    CELLSZ_VAR = 1
} cellsize_e;

typedef enum {
    /* a stub for uninitialized cells */
    CELL_UNKN = 0,

    /* fixed-size self-contained */
    CELL_INT,
    CELL_FLOAT,
    CELL_BOOL,
    CELL_CHAR,

    /* fixed-size linked */
    CELL_REF,       /* single pointer to other cell */
    CELL_CONS,      /* two pointers to other cells */
    CELL_SYM,       /* pointer to a string */

    /* varsized */
    CELL_CACHE,     /* have a pointer to cell_t[] */
    CELL_VECT,      /* have a pointer to contiguous cell_t[] */
    CELL_STR,       /* have a pointer to contiguous wchar_t[] */
    CELL_BVECT,     /* have a pointer to contiguous uint8_t[] */
} celltype_e;

/*
 * Cell 
 */
struct cell {
    struct {
        cellsize_e sz:1;
        celltype_e ty:7;
        int nref:24;
    } tag;

    union {
       fixcell_t fix;
       varcell_t var;
    } as;
};

#endif //__SECD_H__
