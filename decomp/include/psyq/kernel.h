#ifndef PSYQ_KERNEL_H
#define PSYQ_KERNEL_H

/* PsyQ kernel.h: the executable file header (PS-X EXE). */
struct EXEC {
    unsigned long pc0;    /* entry point */
    unsigned long gp0;    /* initial $gp */
    unsigned long t_addr; /* load address */
    unsigned long t_size; /* bytes loaded from file offset 0x800 */
    unsigned long d_addr;
    unsigned long d_size;
    unsigned long b_addr; /* cleared by the loader */
    unsigned long b_size;
    unsigned long s_addr; /* initial $sp/$fp when nonzero */
    unsigned long s_size;
    unsigned long sp, fp, gp, ret, base; /* saved by Exec() */
};

struct XF_HDR {
    char key[8]; /* "PS-X EXE" */
    unsigned long text;
    unsigned long data;
    struct EXEC exec;
    char title[60]; /* region marker */
};

#endif
