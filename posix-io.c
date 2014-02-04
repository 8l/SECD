#include "secd.h"
#include "secd_io.h"
#include "memory.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#define io_assert(cond, ...) \
    if (!(cond)) { \
        errorf(__VA_ARGS__); \
        return -1; \
    }

cell_t *secd_stdin(secd_t *secd) {
    return new_fileport(secd, stdin, "r");
}

cell_t *secd_stdout(secd_t *secd) {
    return new_fileport(secd, stdout, "w");
}

cell_t *secd_fopen(secd_t *secd, const char *fname, const char *mode) {
    FILE *f = fopen(fname, mode);
    if (!f)
        return new_error(secd, "secd_fopen('%s'): %s\n", fname, strerror(errno));

    cell_t *cmdport = new_fileport(secd, f, mode);
    assert_cellf(cmdport, "secd_fopen: failed to create port for '%s'\n", fname);
    return cmdport;
}

long secd_portsize(secd_t *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_portsize: not a port\n");

    if (port->as.port.file) {
        FILE *f = port->as.port.as.file;
        long curpos = ftell(f);

        if (!fseek(f, 0, SEEK_END))
            return -1; /* file is not seekable */

        long endpos = ftell(f);
        fseek(f, curpos, SEEK_SET);
        return endpos;
    } else {
        cell_t *str = port->as.port.as.str;
        return mem_size(secd, str);
    }
}

int secd_pclose(secd_t *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_pclose: not a port\n");
    io_assert(port->as.port.as.file, "secd_pclose: already closed\n");

    int ret = 0;
    if (port->as.port.file) {
        ret = fclose(port->as.port.as.file);
        port->as.port.as.file = NULL;
    } else {
        drop_cell(secd, port->as.port.as.str);
        port->as.port.as.str = SECD_NIL;
    }
    port->as.port.input = false;
    port->as.port.output = false;
    return ret;
}

/*
 * Port-reading
 */
int secd_getc(secd_t *secd, cell_t *port) {
    io_assert(cell_type(port) == CELL_PORT, "secd_getc: not a port\n");
    io_assert(is_input(port), "secd_getc: not an input port\n");

    if (port->as.port.file) {
        int c = fgetc(port->as.port.as.file);
        if (c != EOF)
            return c;
        return SECD_EOF;
    } else {
        cell_t *str = port->as.port.as.str;
        size_t size = mem_size(secd, str);
        if (str->as.str.offset >= (int)size)
            return EOF;

        char c = strmem(str)[str->as.str.offset];
        ++str->as.str.offset;
        return (int)c;
    }
}

size_t secd_fread(secd_t *secd, cell_t *port, char *s, int size) {
    io_assert(cell_type(port) == CELL_PORT, "secd_fread: not a port\n");
    io_assert(is_input(port), "secd_fread: not an input port\n");

    if (port->as.port.file) {
        FILE *f = port->as.port.as.file;
        return fread(s, size, 1, f);;
    } else {
        cell_t *str = port->as.port.as.str;
        size_t srcsize = mem_size(secd, str);
        if (srcsize < (size_t)size) size = srcsize;

        memcpy(s, strmem(str), size);
        return size;
    }
}

/*
 * Port-printing
 */
int secd_vprintf(secd_t *secd, cell_t *port, const char *format, va_list ap) {
    io_assert(cell_type(port) == CELL_PORT, "vpprintf: not a port\n");
    io_assert(is_output(port), "vpprintf: not an output port\n");
    int ret;

    if (port->as.port.file) {
        ret = vfprintf(port->as.port.as.file, format, ap);
    } else {
        cell_t *str = port->as.port.as.str;
        char *mem = strmem(str);
        size_t offset = str->as.str.offset;
        size_t size = mem_size(secd, str) - offset;
        ret = vsnprintf(mem, size, format, ap);
        if (ret == (int)size) {
            errorf("vpprintf: string is too small");
            errorf("vpprintf: TODO: resize string");
            ret = -1;
        }
    }
    return ret;
}

int secd_printf(secd_t *secd, cell_t *port, const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    int ret = secd_vprintf(secd, port, format, ap);
    va_end(ap);

    return ret;
}

void sexp_print_port(secd_t *secd, const cell_t *port) {
    if (SECD_NIL == port->as.port.as.str) {
        printf("#<closed>");
        return;
    }
    bool in = port->as.port.input;
    bool out = port->as.port.output;
    printf("#<%s%s%s: ", (in ? "input" : ""), (out ? "output" : ""), (in && out ? "/" : ""));

    if (port->as.port.file) {
        printf("file %d", fileno(port->as.port.as.file));
    } else {
        printf("string %ld", cell_index(secd, port->as.port.as.str));
    }
    printf(">");
}

