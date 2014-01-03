#include "secd.h"
#include "memory.h"
#include "secdops.h"

/*
 *  SECD built-ins
 */

cell_t *secd_cons(secd_t *secd) {
    ctrldebugf("CONS\n");
    cell_t *a = pop_stack(secd);

    cell_t *b = pop_stack(secd);

    cell_t *cons = new_cons(secd, a, b);
    drop_cell(a); drop_cell(b);

    return push_stack(secd, cons);
}

cell_t *secd_car(secd_t *secd) {
    ctrldebugf("CAR\n");
    cell_t *cons = pop_stack(secd);
    assert(cons, "secd_car: pop_stack() failed");
    assert(not_nil(cons), "secd_car: cons is NIL");
    assert(is_cons(cons), "secd_car: cons expected");

    cell_t *car = push_stack(secd, get_car(cons));
    drop_cell(cons);
    return car;
}

cell_t *secd_cdr(secd_t *secd) {
    ctrldebugf("CDR\n");
    cell_t *cons = pop_stack(secd);
    assert(cons, "secd_cdr: pop_stack() failed");
    assert(not_nil(cons), "secd_cdr: cons is NIL");
    assert(is_cons(cons), "secd_cdr: cons expected");

    cell_t *cdr = push_stack(secd, get_cdr(cons));
    drop_cell(cons);
    return cdr;
}

cell_t *secd_ldc(secd_t *secd) {
    ctrldebugf("LDC ");

    cell_t *arg = pop_control(secd);
    assert(arg, "secd_ldc: pop_control failed");
    if (CTRLDEBUG) printc(arg);

    push_stack(secd, arg);
    drop_cell(arg);
    return arg;
}

cell_t *secd_ld(secd_t *secd) {
    ctrldebugf("LD ");

    cell_t *arg = pop_control(secd);
    assert(arg, "secd_ld: stack empty");
    assert(atom_type(arg) == ATOM_SYM,
           "secd_ld: not a symbol [%ld]", cell_index(arg));
    if (CTRLDEBUG) printc(arg);

    const char *sym = symname(arg);
    cell_t *val = lookup_env(secd, sym);
    drop_cell(arg);
    assert(val, "lookup failed for %s", sym);
    return push_stack(secd, val);
}

static inline cell_t *to_bool(secd_t *secd, bool cond) {
    return ((cond)? lookup_env(secd, "#t") : secd->nil);
}

bool atom_eq(const cell_t *a1, const cell_t *a2) {
    enum atom_type atype1 = atom_type(a1);
    if (a1 == a2)
        return true;
    if (atype1 != atom_type(a2))
        return false;
    switch (atype1) {
      case ATOM_INT: return (a1->as.atom.as.num == a2->as.atom.as.num);
      case ATOM_SYM: return (!strcasecmp(symname(a1), symname(a2)));
      case ATOM_FUNC: return (a1->as.atom.as.op.fun == a2->as.atom.as.op.fun);
      default: errorf("atom_eq([%ld], [%ld]): don't know how to handle type %d\n",
                       cell_index(a1), cell_index(a2), atype1);
    }
    return false;
}

bool list_eq(const cell_t *xs, const cell_t *ys) {
    asserti(is_cons(xs), "list_eq: [%ld] is not a cons", cell_index(xs));
    if (xs == ys)   return true;
    while (not_nil(xs)) {
        if (!is_cons(xs)) return atom_eq(xs, ys);
        if (is_nil(ys)) return false;
        if (!is_cons(ys)) return false;
        const cell_t *x = get_car(xs);
        const cell_t *y = get_car(ys);
        if (not_nil(x)) {
            if (is_nil(y)) return false;
            if (is_cons(x)) {
                if (!list_eq(x, y)) return false;
            } else {
                if (!atom_eq(x, y)) return false;
            }
        } else {
            if (not_nil(y)) return false;
        }

        xs = list_next(xs);
        ys = list_next(ys);
    }
    return is_nil(ys);
}

cell_t *secd_atom(secd_t *secd) {
    ctrldebugf("ATOM\n");
    cell_t *val = pop_stack(secd);
    assert(val, "secd_atom: pop_stack() failed");
    assert(not_nil(val), "secd_atom: empty stack");

    cell_t *result = to_bool(secd, (val ? !is_cons(val) : true));
    drop_cell(val);
    return push_stack(secd, result);
}

cell_t *secd_eq(secd_t *secd) {
    ctrldebugf("EQ\n");
    cell_t *a = pop_stack(secd);
    assert(a, "secd_eq: pop_stack(a) failed");

    cell_t *b = pop_stack(secd);
    assert(b, "secd_eq: pop_stack(b) failed");

    bool eq = (is_cons(a) ? list_eq(a, b) : atom_eq(a, b));

    cell_t *val = to_bool(secd, eq);
    drop_cell(a); drop_cell(b);
    return push_stack(secd, val);
}

static cell_t *arithm_op(secd_t *secd, int op(int, int)) {
    cell_t *a = pop_stack(secd);
    assert(a, "secd_add: pop_stack(a) failed")
    assert(atom_type(a) == ATOM_INT, "secd_add: a is not int");

    cell_t *b = pop_stack(secd);
    assert(b, "secd_add: pop_stack(b) failed");
    assert(atom_type(b) == ATOM_INT, "secd_add: b is not int");

    int res = op(a->as.atom.as.num, b->as.atom.as.num);
    drop_cell(a); drop_cell(b);
    return push_stack(secd, new_number(secd, res));
}

inline static int iplus(int x, int y) {
    return x + y;
}
inline static int iminus(int x, int y) {
    return x - y;
}
inline static int imult(int x, int y) {
    return x * y;
}
inline static int idiv(int x, int y) {
    return x / y;
}
inline static int irem(int x, int y) {
    return x % y;
}

cell_t *secd_add(secd_t *secd) {
    ctrldebugf("ADD\n");
    return arithm_op(secd, iplus);
}
cell_t *secd_sub(secd_t *secd) {
    ctrldebugf("SUB\n");
    return arithm_op(secd, iminus);
}
cell_t *secd_mul(secd_t *secd) {
    ctrldebugf("MUL\n");
    return arithm_op(secd, imult);
}
cell_t *secd_div(secd_t *secd) {
    ctrldebugf("SUB\n");
    return arithm_op(secd, idiv);
}
cell_t *secd_rem(secd_t *secd) {
    ctrldebugf("SUB\n");
    return arithm_op(secd, irem);
}

cell_t *secd_leq(secd_t *secd) {
    ctrldebugf("LEQ\n");

    cell_t *opnd1 = pop_stack(secd);
    cell_t *opnd2 = pop_stack(secd);

    assert(atom_type(opnd1) == ATOM_INT, "secd_leq: int expected as opnd1");
    assert(atom_type(opnd2) == ATOM_INT, "secd_leq: int expected as opnd2");

    cell_t *result = to_bool(secd, opnd1->as.atom.as.num <= opnd2->as.atom.as.num);
    drop_cell(opnd1); drop_cell(opnd2);
    return push_stack(secd, result);
}

cell_t *secd_sel(secd_t *secd) {
    ctrldebugf("SEL ");

    cell_t *condcell = pop_stack(secd);
    if (CTRLDEBUG) printc(condcell);

    bool cond = not_nil(condcell) ? true : false;
    drop_cell(condcell);

    cell_t *thenb = pop_control(secd);
    cell_t *elseb = pop_control(secd);
    assert(is_cons(thenb) && is_cons(elseb), "secd_sel: both branches must be conses");

    cell_t *joinb = secd->control;
    secd->control = share_cell(cond ? thenb : elseb);

    push_dump(secd, joinb);

    drop_cell(thenb); drop_cell(elseb); drop_cell(joinb);
    return secd->control;
}

cell_t *secd_join(secd_t *secd) {
    ctrldebugf("JOIN\n");

    cell_t *joinb = pop_dump(secd);
    assert(joinb, "secd_join: pop_dump() failed");

    secd->control = joinb; //share_cell(joinb); drop_cell(joinb);
    return secd->control;
}



cell_t *secd_ldf(secd_t *secd) {
    ctrldebugf("LDF\n");

    cell_t *func = pop_control(secd);
    assert(func, "secd_ldf: failed to get the control path");

    cell_t *body = list_head(list_next(func));
    if (! is_control_compiled(body)) {
        cell_t *compiled = compile_control_path(secd, body);
        assert(compiled, "secd_ldf: failed to compile possible callee");
        drop_cell(body);
        func->as.cons.cdr->as.cons.car = share_cell(compiled);
    }

    cell_t *closure = new_cons(secd, func, secd->env);
    drop_cell(func);
    return push_stack(secd, closure);
}

#if TAILRECURSION
static cell_t * new_dump_if_tailrec(cell_t *control, cell_t *dump);
#endif

static cell_t *extract_argvals(secd_t *secd) {
    if (atom_type(list_head(secd->control)) != ATOM_INT) {
        return pop_stack(secd); // don't forget to drop
    }

    cell_t *argvals = secd->nil;
    cell_t *argvcursor = secd->nil;
    cell_t *new_stack = secd->stack;

    cell_t *ntop = pop_control(secd);
    int n = numval(ntop);

    while (n-- > 0) {
        argvcursor = new_stack;
        new_stack = list_next(new_stack);
    }
    if (not_nil(argvcursor)) {
        argvals = secd->stack;
        argvcursor->as.cons.cdr = secd->nil;
    }
    secd->stack = new_stack; // no share_cell

    drop_cell(ntop);
    // has at least 1 "ref", don't forget to drop
    return argvals;
}

cell_t *secd_ap(secd_t *secd) {
    ctrldebugf("AP\n");

    cell_t *closure = pop_stack(secd);
    assert(is_cons(closure), "secd_ap: closure is not a cons");

    cell_t *argvals = extract_argvals(secd);
    assert(argvals, "secd_ap: no arguments on stack");
    assert(is_cons(argvals), "secd_ap: a list expected for arguments");

    cell_t *func = get_car(closure);
    cell_t *newenv = get_cdr(closure);

    if (atom_type(func) == ATOM_FUNC) {
        secd_nativefunc_t native = (secd_nativefunc_t)func->as.atom.as.op.fun;
        cell_t *result = native(secd, argvals);
        assert(result, "secd_ap: a built-in routine failed");
        push_stack(secd, result);

        drop_cell(closure); drop_cell(argvals);
        return result;
    }
    assert(is_cons(func), "secd_ap: not a cons at func definition");

    cell_t *argnames = get_car(func);
    cell_t *control = list_head(list_next(func));
    assert(is_cons(control), "secd_ap: control path is not a list");

    if (! is_control_compiled( control )) {
        // control has not been compiled yet
        cell_t *compiled = compile_control_path(secd, control);
        assert(compiled, "secd_ap: failed to compile callee");
        //drop_cell(control); // no need: will be dropped with func
        control = compiled;
    }

#if TAILRECURSION
    cell_t *new_dump = new_dump_if_tailrec(secd->control, secd->dump);
    if (new_dump) {
        cell_t *dump = secd->dump;
        secd->dump = share_cell(new_dump);
        drop_cell(dump);  // dump may be new_dump, so don't drop before share
    } else {
        push_dump(secd, secd->control);
        push_dump(secd, secd->env);
        push_dump(secd, secd->stack);
    }
#else
    push_dump(secd, secd->control);
    push_dump(secd, secd->env);
    push_dump(secd, secd->stack);
#endif

    drop_cell(secd->stack);
    secd->stack = secd->nil;

    cell_t *frame = new_frame(secd, argnames, argvals);
    cell_t *oldenv = secd->env;
    drop_cell(oldenv);
    secd->env = share_cell(new_cons(secd, frame, newenv));

    drop_cell(secd->control);
    secd->control = share_cell(control);

    drop_cell(closure); drop_cell(argvals);
    return control;
}

cell_t *secd_rtn(secd_t *secd) {
    ctrldebugf("RTN\n");

    assert(is_nil(secd->control), "secd_rtn: commands after RTN");

    assert(not_nil(secd->stack), "secd_rtn: stack is empty");
    cell_t *top = pop_stack(secd);
    assert(is_nil(secd->stack), "secd_rtn: stack holds more than 1 value");

    cell_t *prevstack = pop_dump(secd);
    cell_t *prevenv = pop_dump(secd);
    cell_t *prevcontrol = pop_dump(secd);

    drop_cell(secd->env);

    secd->stack = share_cell(new_cons(secd, top, prevstack));
    drop_cell(top); drop_cell(prevstack);

    secd->env = prevenv; // share_cell(prevenv); drop_cell(prevenv);
    secd->control = prevcontrol; // share_cell(prevcontrol); drop_cell(prevcontrol);

    return top;
}


cell_t *secd_dum(secd_t *secd) {
    ctrldebugf("DUM\n");

    cell_t *oldenv = secd->env;
    cell_t *newenv = new_cons(secd, secd->nil, oldenv);

    secd->env = share_cell(newenv);
    drop_cell(oldenv);

    return newenv;
}

cell_t *secd_rap(secd_t *secd) {
    ctrldebugf("RAP\n");

    cell_t *closure = pop_stack(secd);
    cell_t *argvals = pop_stack(secd);

    cell_t *newenv = get_cdr(closure);
    cell_t *func = get_car(closure);
    cell_t *argnames = get_car(func);
    cell_t *control = get_car(list_next(func));

    if (! is_control_compiled( control )) {
        // control has not been compiled yet
        cell_t *compiled = compile_control_path(secd, control);
        assert(compiled, "secd_rap: failed to compile callee");
        control = compiled;
    }

    push_dump(secd, secd->control);
    push_dump(secd, get_cdr(secd->env));
    push_dump(secd, secd->stack);

    cell_t *frame = new_frame(secd, argnames, argvals);
#if CTRLDEBUG
    printf("new frame: \n"); print_cell(frame);
    printf(" argnames: \n"); printc(argnames);
    printf(" argvals : \n"); printc(argvals);
#endif
    newenv->as.cons.car = share_cell(frame);

    drop_cell(secd->stack);
    secd->stack = secd->nil;

    cell_t *oldenv = secd->env;
    secd->env = share_cell(newenv);

    drop_cell(secd->control);
    secd->control = share_cell(control);

    drop_cell(oldenv);
    drop_cell(closure); drop_cell(argvals);
    return secd->control;
}


cell_t *secd_read(secd_t *secd) {
    ctrldebugf("READ\n");

    cell_t *inp = sexp_parse(secd, NULL);
    assert(inp, "secd_read: failed to read");

    push_stack(secd, inp);
    return inp;
}

cell_t *secd_print(secd_t *secd) {
    ctrldebugf("PRINT\n");

    cell_t *top = get_car(secd->stack);
    assert(top, "secd_print: no stack");

    sexp_print(top);
    printf("\n");
    return top;
}

const cell_t cons_func  = INIT_FUNC(secd_cons);
const cell_t car_func   = INIT_FUNC(secd_car);
const cell_t cdr_func   = INIT_FUNC(secd_cdr);
const cell_t add_func   = INIT_FUNC(secd_add);
const cell_t sub_func   = INIT_FUNC(secd_sub);
const cell_t mul_func   = INIT_FUNC(secd_mul);
const cell_t div_func   = INIT_FUNC(secd_div);
const cell_t rem_func   = INIT_FUNC(secd_rem);
const cell_t leq_func   = INIT_FUNC(secd_leq);
const cell_t ldc_func   = INIT_FUNC(secd_ldc);
const cell_t ld_func    = INIT_FUNC(secd_ld);
const cell_t eq_func    = INIT_FUNC(secd_eq);
const cell_t atom_func  = INIT_FUNC(secd_atom);
const cell_t sel_func   = INIT_FUNC(secd_sel);
const cell_t join_func  = INIT_FUNC(secd_join);
const cell_t ldf_func   = INIT_FUNC(secd_ldf);
const cell_t ap_func    = INIT_FUNC(secd_ap);
const cell_t rtn_func   = INIT_FUNC(secd_rtn);
const cell_t dum_func   = INIT_FUNC(secd_dum);
const cell_t rap_func   = INIT_FUNC(secd_rap);
const cell_t read_func  = INIT_FUNC(secd_read);
const cell_t print_func = INIT_FUNC(secd_print);
const cell_t stop_func  = INIT_FUNC(NULL);

const cell_t ap_sym     = INIT_SYM("AP");
const cell_t add_sym    = INIT_SYM("ADD");
const cell_t atom_sym   = INIT_SYM("ATOM");
const cell_t car_sym    = INIT_SYM("CAR");
const cell_t cdr_sym    = INIT_SYM("CDR");
const cell_t cons_sym   = INIT_SYM("CONS");
const cell_t div_sym    = INIT_SYM("DIV");
const cell_t dum_sym    = INIT_SYM("DUM");
const cell_t eq_sym     = INIT_SYM("EQ");
const cell_t join_sym   = INIT_SYM("JOIN");
const cell_t ld_sym     = INIT_SYM("LD");
const cell_t ldc_sym    = INIT_SYM("LDC");
const cell_t ldf_sym    = INIT_SYM("LDF");
const cell_t leq_sym    = INIT_SYM("LEQ");
const cell_t mul_sym    = INIT_SYM("MUL");
const cell_t print_sym  = INIT_SYM("PRINT");
const cell_t rap_sym    = INIT_SYM("RAP");
const cell_t read_sym   = INIT_SYM("READ");
const cell_t rem_sym    = INIT_SYM("REM");
const cell_t rtn_sym    = INIT_SYM("RTN");
const cell_t sel_sym    = INIT_SYM("SEL");
const cell_t stop_sym   = INIT_SYM("STOP");
const cell_t sub_sym    = INIT_SYM("SUB");

const cell_t t_sym      = INIT_SYM("#t");
const cell_t nil_sym    = INIT_SYM("NIL");

const opcode_t opcode_table[] = {
    // opcodes: for information, not to be called
    // keep symbols sorted properly
    [SECD_ADD]  = { &add_sym,     &add_func,  0},
    [SECD_AP]   = { &ap_sym,      &ap_func,   0},
    [SECD_ATOM] = { &atom_sym,    &atom_func, 0},
    [SECD_CAR]  = { &car_sym,     &car_func,  0},
    [SECD_CDR]  = { &cdr_sym,     &cdr_func,  0},
    [SECD_CONS] = { &cons_sym,    &cons_func, 0},
    [SECD_DIV]  = { &div_sym,     &div_func,  0},
    [SECD_DUM]  = { &dum_sym,     &dum_func,  0},
    [SECD_EQ]   = { &eq_sym,      &eq_func,   0},
    [SECD_JOIN] = { &join_sym,    &join_func, 0},
    [SECD_LD]   = { &ld_sym,      &ld_func,   1},
    [SECD_LDC]  = { &ldc_sym,     &ldc_func,  1},
    [SECD_LDF]  = { &ldf_sym,     &ldf_func,  1},
    [SECD_LEQ]  = { &leq_sym,     &leq_func,  0},
    [SECD_MUL]  = { &mul_sym,     &mul_func,  0},
    [SECD_PRN]  = { &print_sym,   &print_func,0},
    [SECD_RAP]  = { &rap_sym,     &rap_func,  0},
    [SECD_READ] = { &read_sym,    &read_func, 0},
    [SECD_REM]  = { &rem_sym,     &rem_func,  0},
    [SECD_RTN]  = { &rtn_sym,     &rtn_func,  0},
    [SECD_SEL]  = { &sel_sym,     &sel_func,  2},
    [SECD_STOP] = { &stop_sym,    &stop_func, 0},
    [SECD_SUB]  = { &sub_sym,     &sub_func,  0},

    [SECD_LAST] = { NULL,         NULL,       0}
};

index_t optable_len = 0;


index_t search_opcode_table(cell_t *sym) {
    if (optable_len == 0)
        while (opcode_table[optable_len].sym) ++optable_len;

    index_t a = 0;
    index_t b = optable_len;
    while (opcode_table[b].sym) ++b;
    while (a != b) {
        index_t c = (a + b) / 2;
        int ord = str_cmp( symname(sym), symname(opcode_table[c].sym));
        if (ord == 0) return c;
        if (ord < 0) b = c;
        else a = c;
    }
    return -1;
}

