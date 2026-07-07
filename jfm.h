#ifndef JFM_H
#define JFM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <locale.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xft/Xft.h>

#define nil NULL
typedef unsigned char uchar;

typedef struct Point Point;
typedef struct Rectangle Rectangle;
struct Point { int x, y; };
struct Rectangle { Point min, max; };

#define Pt(x,y) ((Point){x,y})
#define Rect(x1,y1,x2,y2) ((Rectangle){(Point){x1,y1},(Point){x2,y2}})
#define Rpt(p,q) ((Rectangle){p,q})
#define Dx(r) ((r).max.x - (r).min.x)
#define Dy(r) ((r).max.y - (r).min.y)
#define addpt(p,q) ((Point){(p).x+(q).x,(p).y+(q).y})
#define subpt(p,q) ((Point){(p).x-(q).x,(p).y-(q).y})
#define insetrect(r,n) ((Rectangle){addpt((r).min,Pt(n,n)),subpt((r).max,Pt(n,n))})
#define ptinrect(p,r) ((p).x>=(r).min.x && (p).x<(r).max.x && (p).y>=(r).min.y && (p).y<(r).max.y)
#define eqrect(a,b) ((a).min.x==(b).min.x && (a).min.y==(b).min.y && (a).max.x==(b).max.x && (a).max.y==(b).max.y)

enum {
	Toolpadding = 4,
	Padding = 1,
	Scrollwidth = 14,
	Slowscroll = 10,
};

enum {
	Mrename,
	Mdelete,
};

typedef struct Dir Dir;
struct Dir {
	char *name;
	long long length;
	time_t mtime;
	int isdir;
};

#define max(a,b) ((a)>(b)?(a):(b))
char *smprint(const char *fmt, ...);

extern Display *dpy;
extern int scr;
extern Window win;
extern GC gc;
extern XftFont *font;
extern char *fontname;
extern XftDraw *xftdraw;
extern Visual *visual;
extern Colormap cmap;
extern int depth;

extern Rectangle screenr;
extern Rectangle toolr;
extern Rectangle homer;
extern Rectangle upr;
extern Rectangle cdr;
extern Rectangle newdirr;
extern Rectangle newfiler;
extern Rectangle viewr;
extern Rectangle scrollr;
extern Rectangle scrposr;
extern Rectangle pathr;

extern XftColor c_toolbg, c_toolfg, c_viewbg, c_viewfg, c_selbg, c_selfg;
extern XftColor c_scrollbg, c_scrollfg, c_high, c_alertbg, c_alertfg;
extern XftColor c_sel2bg, c_sel2fg;

extern Dir *dirs;
extern int ndirs;
extern char path[4096];
extern char *home;
extern int sizew;
extern int lineh;
extern int nlines;
extern int offset;
extern int scrolling;
extern int oldbuttons;
extern int lastn;
extern int selected;

void redraw(void);
void showerrstr(const char *msg);
void loaddirs(void);
void up(void);
void cd(char *dir);
void mk_dir(char *name);
void touch(char *name);
void rm(Dir d);
void mv(char *from, char *to);
void movefile(char *from, char *dest);
int plumbfile(char *dir, char *name);
void flash(int n);
void alert(const char *msg, const char *err);
int confirm(const char *msg);
int enter(const char *prompt, char *buf, int nbuf);
void evtresize(int w, int h);
void evtmouse(XEvent *e);
void evtkey(XEvent *e);
int indexat(Point p);
Point cept(const char *text);
void drawdir(int n, int selected);
Point drawtext(Point p, XftColor col, char *t, int maxx);
Rectangle drawbutton(Point *p, XftColor col, char *label);
void xfillrect(Rectangle r, XftColor c);
void xdrawrect(Rectangle r, XftColor c);
void xdrawline(Point p1, Point p2, XftColor c);
void xdrawstring(Point p, XftColor c, const char *s);
int xstringwidth(const char *s);
char *cleanname(char *s);
char *abspath(char *wd, char *p);
char *expandtilde(char *s);
void initcolors(void);
void loadfont(const char *name);
void cyclefont(void);
int scrollclamp(int off);
void scrollup(int off);
void scrolldown(int off);
void readhome(void);
void usage(void);
char *mdate(Dir d);
int menuhit(int x, int y, char **items, int nitems);
int doexec(const char *cmd);
char *quotestrdup(const char *s);

#endif
