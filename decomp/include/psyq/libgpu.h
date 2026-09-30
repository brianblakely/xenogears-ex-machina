#ifndef PSYQ_LIBGPU_H
#define PSYQ_LIBGPU_H

#include "psyq/types.h"

/* PsyQ libgpu. */
typedef struct {
    short x, y;
    short w, h;
} RECT;

typedef struct {
    u_long tag;
    u_long code[15];
} DR_ENV;

typedef struct {
    RECT clip;
    short ofs[2];
    RECT tw;
    u_short tpage;
    u_char dtd;
    u_char dfe;
    u_char isbg;
    u_char r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    u_char isinter;
    u_char isrgb24;
    u_char pad0, pad1;
} DISPENV;

typedef struct {
    u_long mode;
    RECT *crect;
    u_long *caddr;
    RECT *prect;
    u_long *paddr;
} TIM_IMAGE;

typedef struct {
    unsigned addr : 24;
    unsigned len : 8;
    u_char r0, g0, b0, code;
} P_TAG;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    short w, h;
} TILE;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short w, h;
} SPRT;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
} POLY_F3;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char u0, v0;
    u_short clut;
    short x1, y1;
    u_char u1, v1;
    u_short tpage;
    short x2, y2;
    u_char u2, v2;
    u_short pad1;
    short x3, y3;
    u_char u3, v3;
    u_short pad2;
} POLY_FT4;

typedef struct {
    u_long tag;
    u_long code[2];
} DR_MODE;

#define setlen(p, _len) (((P_TAG *)(p))->len = (u_char)(_len))
#define setaddr(p, _addr) (((P_TAG *)(p))->addr = (u_long)(_addr))
#define setcode(p, _code) (((P_TAG *)(p))->code = (u_char)(_code))
#define getaddr(p) (u_long)(((P_TAG *)(p))->addr)
#define setSprt(p) setlen(p, 4), setcode(p, 0x64)
#define addPrim(ot, p) setaddr(p, getaddr(ot)), setaddr(ot, p)

int ResetGraph(int mode);
int SetGraphDebug(int level);
int DrawSync(int mode);
int DrawSyncCallback(void (*func)());
void SetDispMask(int mask);
int ClearImage(RECT *rect, u_char r, u_char g, u_char b);
int LoadImage(RECT *rect, u_long *p);
int StoreImage(RECT *rect, u_long *p);
int MoveImage(RECT *rect, int x, int y);
u_long *ClearOTagR(u_long *ot, int n);
void DrawOTag(u_long *p);
void DrawPrim(void *p);
void AddPrim(void *ot, void *p);
void AddPrims(void *ot, void *p0, void *p1);
void TermPrim(void *p);
void SetPolyF3(POLY_F3 *p);
void SetPolyFT4(POLY_FT4 *p);
void SetTile(TILE *p);
void SetSprt(SPRT *p);
void SetSemiTrans(void *p, int abe);
void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
u_short GetClut(int x, int y);
u_short GetTPage(int tp, int abr, int x, int y);
DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
int OpenTIM(u_long *addr);
TIM_IMAGE *ReadTIM(TIM_IMAGE *timimg);

#endif
