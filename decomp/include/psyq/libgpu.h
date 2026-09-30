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
    short x1, y1;
    short x2, y2;
    short x3, y3;
} POLY_F4;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    u_char r1, g1, b1, pad1;
    short x1, y1;
    u_char r2, g2, b2, pad2;
    short x2, y2;
    u_char r3, g3, b3, pad3;
    short x3, y3;
} POLY_G4;

typedef struct {
    u_long tag;
    u_char r0, g0, b0, code;
    short x0, y0;
    short x1, y1;
    short x2, y2;
    u_long pad;
} LINE_F3;

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

typedef struct {
    u_long tag;
    u_long code[2];
} DR_TWIN;

typedef struct {
    u_long tag;
    u_long code[1];
} DR_TPAGE;

typedef struct {
    u_long tag;
    u_long code[5];
} DR_MOVE;

#define setlen(p, _len) (((P_TAG *)(p))->len = (u_char)(_len))
#define setaddr(p, _addr) (((P_TAG *)(p))->addr = (u_long)(_addr))
#define setcode(p, _code) (((P_TAG *)(p))->code = (u_char)(_code))
#define getaddr(p) (u_long)(((P_TAG *)(p))->addr)
#define getcode(p) (u_char)(((P_TAG *)(p))->code)
#define setRGB0(p, _r0, _g0, _b0) ((p)->r0 = _r0, (p)->g0 = _g0, (p)->b0 = _b0)
#define setRGB1(p, _r1, _g1, _b1) ((p)->r1 = _r1, (p)->g1 = _g1, (p)->b1 = _b1)
#define setRGB2(p, _r2, _g2, _b2) ((p)->r2 = _r2, (p)->g2 = _g2, (p)->b2 = _b2)
#define setRGB3(p, _r3, _g3, _b3) ((p)->r3 = _r3, (p)->g3 = _g3, (p)->b3 = _b3)
#define setWH(p, _w, _h) (p)->w = _w, (p)->h = _h
#define setUV4(p, _u0, _v0, _u1, _v1, _u2, _v2, _u3, _v3) \
    (p)->u0 = _u0, (p)->v0 = _v0, (p)->u1 = _u1, (p)->v1 = _v1, \
    (p)->u2 = _u2, (p)->v2 = _v2, (p)->u3 = _u3, (p)->v3 = _v3
#define setXY4(p, _x0, _y0, _x1, _y1, _x2, _y2, _x3, _y3) \
    (p)->x0 = _x0, (p)->y0 = _y0, (p)->x1 = _x1, (p)->y1 = _y1, \
    (p)->x2 = _x2, (p)->y2 = _y2, (p)->x3 = _x3, (p)->y3 = _y3
#define setSemiTrans(p, abe) \
    ((abe) ? setcode(p, getcode(p) | 0x02) : setcode(p, getcode(p) & ~0x02))
#define setShadeTex(p, tge) \
    ((tge) ? setcode(p, getcode(p) | 0x01) : setcode(p, getcode(p) & ~0x01))
#define setPolyFT4(p) setlen(p, 9), setcode(p, 0x2c)
#define setPolyG3(p) setlen(p, 6), setcode(p, 0x30)
#define setPolyG4(p) setlen(p, 8), setcode(p, 0x38)
#define setTile(p) setlen(p, 3), setcode(p, 0x60)
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
void SetPolyF4(POLY_F4 *p);
void SetPolyG4(POLY_G4 *p);
void SetPolyFT4(POLY_FT4 *p);
void SetLineF3(LINE_F3 *p);
void SetTile(TILE *p);
void SetSprt(SPRT *p);
void SetSemiTrans(void *p, int abe);
void SetShadeTex(void *p, int tge);
void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
void SetTexWindow(DR_TWIN *p, RECT *tw);
void SetDrawTPage(DR_TPAGE *p, int dfe, int dtd, int tpage);
void SetDrawMove(DR_MOVE *p, RECT *rect, int x, int y);
u_short GetClut(int x, int y);
u_short GetTPage(int tp, int abr, int x, int y);
DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);
int OpenTIM(u_long *addr);
TIM_IMAGE *ReadTIM(TIM_IMAGE *timimg);

#endif
