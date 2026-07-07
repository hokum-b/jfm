#include "jfm.h"

Display *dpy;
int scr;
Window win;
GC gc;
XftFont *font;

XftDraw *xftdraw;
Visual *visual;
Colormap cmap;
int depth;

Rectangle screenr;
Rectangle toolr;
Rectangle homer;
Rectangle upr;
Rectangle cdr;
Rectangle newdirr;
Rectangle newfiler;
Rectangle viewr;
Rectangle scrollr;
Rectangle scrposr;
Rectangle pathr;

XftColor c_toolbg, c_toolfg, c_viewbg, c_viewfg, c_selbg, c_selfg;
XftColor c_scrollbg, c_scrollfg, c_high, c_alertbg, c_alertfg;
XftColor c_sel2bg, c_sel2fg;

Dir *dirs;
int ndirs;
char path[4096];
char *home;
int sizew;
int lineh;
int nlines;
int offset;
int scrolling;
int oldbuttons;
int lastn;
int selected = -1;

char *
expandtilde(char *s)
{
	if (s[0] != '~')
		return strdup(s);
	if (s[1] == '/' || s[1] == '\0') {
		char *expanded = smprint("%s%s", home, s + 1);
		return expanded;
	}
	return strdup(s);
}

char *
cleanname(char *s)
{
	char *p, *q, *dotdot;
	int rooted;

	rooted = s[0] == '/';
	p = q = dotdot = s + rooted;

	while (*p) {
		while (*p == '/')
			p++;

		if (p[0] == '.' && (p[1] == '/' || p[1] == 0)) {
			p += 1 + (p[1] == '/');
			continue;
		}

		if (p[0] == '.' && p[1] == '.' && (p[2] == '/' || p[2] == 0)) {
			p += 2 + (p[2] == '/');
			if (q > dotdot) {
				q--;
				while (q > dotdot && *(q - 1) != '/')
					q--;
			} else if (!rooted) {
				if (q != s)
					*q++ = '/';
				*q++ = '.';
				*q++ = '.';
				dotdot = q;
			}
			continue;
		}

		if (q != dotdot && *(q - 1) != '/')
			*q++ = '/';
		while (*p && *p != '/')
			*q++ = *p++;
	}

	if (q == s)
		*q++ = '.';
	if (q > dotdot && *(q - 1) == '/')
		q--;
	*q = 0;

	return s;
}

void
xfillrect(Rectangle r, XftColor c)
{
	XSetForeground(dpy, gc, c.pixel);
	XFillRectangle(dpy, win, gc, r.min.x, r.min.y, Dx(r), Dy(r));
}

void
xdrawrect(Rectangle r, XftColor c)
{
	XSetForeground(dpy, gc, c.pixel);
	XDrawRectangle(dpy, win, gc, r.min.x, r.min.y, Dx(r) - 1, Dy(r) - 1);
}

void
xdrawline(Point p1, Point p2, XftColor c)
{
	XSetForeground(dpy, gc, c.pixel);
	XDrawLine(dpy, win, gc, p1.x, p1.y, p2.x, p2.y);
}

void
xdrawstring(Point p, XftColor c, const char *s)
{
	XftDrawStringUtf8(xftdraw, &c, font, p.x, p.y, (FcChar8 *)s, strlen(s));
}

int
xstringwidth(const char *s)
{
	XGlyphInfo ext;
	XftTextExtentsUtf8(dpy, font, (FcChar8 *)s, strlen(s), &ext);
	return ext.width;
}

Point
drawtext(Point p, XftColor col, char *t, int maxx)
{
	char *s = t;
	if (*s && (p.x + xstringwidth(s)) > maxx) {
		int ew = xstringwidth("…");
		xdrawstring(p, col, "…");
		p.x += ew;
		while (*s && (p.x + xstringwidth(s)) > maxx) {
			int n = 1;
			unsigned char c = *s;
			if ((c & 0x80) == 0) n = 1;
			else if ((c & 0xe0) == 0xc0) n = 2;
			else if ((c & 0xf0) == 0xe0) n = 3;
			else if ((c & 0xf8) == 0xf0) n = 4;
			s += n;
		}
		xdrawstring(p, col, s);
		p.x += xstringwidth(s);
	} else {
		xdrawstring(p, col, s);
		p.x += xstringwidth(s);
	}
	return p;
}

Rectangle
drawbutton(Point *p, XftColor col, char *label)
{
	p->x += Toolpadding;
	int w = xstringwidth(label);
	int h = font->height;
	Rectangle r = Rect(p->x, p->y, p->x + w, p->y + h);
	p->y += (Dy(r) - font->height) / 2 + font->ascent;
	xdrawstring(*p, col, label);
	p->y -= (Dy(r) - font->height) / 2 + font->ascent;
	p->x += w + Toolpadding;
	return r;
}

char *
abspath(char *wd, char *p)
{
	if (p[0] == '/') {
		char *s = strdup(p);
		return cleanname(s);
	} else {
		int n = strlen(wd) + strlen(p) + 2;
		char *s = malloc(n);
		snprintf(s, n, "%s/%s", wd, p);
		return cleanname(s);
	}
}

int
dircmp(const void *a, const void *b)
{
	const Dir *da = a, *db = b;
	if (da->isdir == db->isdir)
		return strcmp(da->name, db->name);
	return da->isdir ? -1 : 1;
}

void
loaddirs(void)
{
	DIR *d = opendir(path);
	if (!d) {
		showerrstr("Unable to load directory");
		return;
	}
	if (dirs) {
		for (int i = 0; i < ndirs; i++)
			free(dirs[i].name);
		free(dirs);
	}
	struct dirent *de;
	int count = 0;
	while ((de = readdir(d)))
		count++;
	rewinddir(d);
	dirs = calloc(count, sizeof(Dir));
	ndirs = 0;
	while ((de = readdir(d))) {
		if (strcmp(de->d_name, ".") == 0)
			continue;
		char fullpath[4096];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", path, de->d_name);
		struct stat st;
		if (stat(fullpath, &st) < 0)
			continue;
		dirs[ndirs].name = strdup(de->d_name);
		dirs[ndirs].length = st.st_size;
		dirs[ndirs].mtime = st.st_mtime;
		dirs[ndirs].isdir = S_ISDIR(st.st_mode);
		ndirs++;
	}
	closedir(d);
	qsort(dirs, ndirs, sizeof(Dir), dircmp);
	offset = 0;
	selected = -1;
	long long m = 1;
	for (int i = 0; i < ndirs; i++)
		if (dirs[i].length > m)
			m = dirs[i].length;
	sizew = m == 0 ? 3 : 1 + 1 + (int)log10((double)m);
}

void
up(void)
{
	char *tmp = abspath(path, "..");
	strncpy(path, tmp, sizeof(path) - 1);
	path[sizeof(path) - 1] = 0;
	free(tmp);
	loaddirs();
}

void
cd(char *dir)
{
	char newpath[4096] = {0};
	if (dir == nil)
		snprintf(newpath, sizeof(newpath), "%s", home);
	else if (dir[0] == '~') {
		char *expanded = expandtilde(dir);
		snprintf(newpath, sizeof(newpath), "%s", expanded);
		free(expanded);
	} else if (dir[0] == '/')
		snprintf(newpath, sizeof(newpath), "%s", dir);
	else {
		int plen = strlen(path);
		if (plen > 0 && path[plen - 1] == '/')
			snprintf(newpath, sizeof(newpath), "%s%s", path, dir);
		else
			snprintf(newpath, sizeof(newpath), "%s/%s", path, dir);
	}
	if (access(newpath, F_OK) < 0) {
		showerrstr("Directory does not exist");
		return;
	}
	char *tmp = abspath(path, newpath);
	strncpy(path, tmp, sizeof(path) - 1);
	path[sizeof(path) - 1] = 0;
	free(tmp);
	loaddirs();
}

void
mk_dir(char *name)
{
	char *p = smprint("%s/%s", path, name);
	if (access(p, F_OK) >= 0) {
		showerrstr("Directory already exists");
		free(p);
		return;
	}
	if (mkdir(p, 0755) < 0)
		showerrstr("Unable to create directory");
	else
		loaddirs();
	free(p);
}

void
touch(char *name)
{
	char *p = smprint("%s/%s", path, name);
	if (access(p, F_OK) >= 0) {
		showerrstr("File already exists");
		free(p);
		return;
	}
	int fd = open(p, O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (fd < 0)
		showerrstr("Unable to create file");
	else {
		close(fd);
		loaddirs();
	}
	free(p);
}

int
doexec(const char *cmd)
{
	int rc = system(cmd);
	if (rc < 0)
		return -1;
	if (WIFEXITED(rc) && WEXITSTATUS(rc) == 0)
		return 0;
	return -1;
}

char *
quotestrdup(const char *s)
{
	int n = 2;
	for (const char *p = s; *p; p++) {
		if (*p == '\'')
			n += 4;
		else
			n++;
	}
	char *q = malloc(n + 1);
	char *d = q;
	*d++ = '\'';
	for (const char *p = s; *p; p++) {
		if (*p == '\'') {
			*d++ = '\'';
			*d++ = '\\';
			*d++ = '\'';
			*d++ = '\'';
		} else {
			*d++ = *p;
		}
	}
	*d++ = '\'';
	*d = 0;
	return q;
}

void
rm(Dir d)
{
	char buf[1024] = {0};
	if (d.isdir)
		snprintf(buf, sizeof(buf), "Delete directory '%s' and its subdirectories ?", d.name);
	else
		snprintf(buf, sizeof(buf), "Delete file '%s' ?", d.name);
	if (!confirm(buf))
		return;
	char *p = smprint("%s/%s", path, d.name);
	char *qp = quotestrdup(p);
	char *cmd = smprint("rm -r %s >/dev/null 2>&1", qp);
	if (doexec(cmd) < 0)
		alert("Cannot remove file/directory", "Check permissions or try again.");
	else
		loaddirs();
	free(cmd);
	free(qp);
	free(p);
}

void
mv(char *from, char *to)
{
	char *fp = smprint("%s/%s", path, from);
	char *tp = smprint("%s/%s", path, to);
	char *qfp = quotestrdup(fp);
	char *qtp = quotestrdup(tp);
	char *cmd = smprint("mv %s %s >/dev/null 2>&1", qfp, qtp);
	if (doexec(cmd) < 0)
		alert("Cannot rename", "Check that the destination is valid and you have permission.");
	else
		loaddirs();
	free(cmd);
	free(qtp);
	free(qfp);
	free(tp);
	free(fp);
}

void
movefile(char *from, char *dest)
{
	char *qfrom = quotestrdup(from);
	char *qdest = quotestrdup(dest);
	char *cmd = smprint("mv %s %s >/dev/null 2>&1", qfrom, qdest);
	if (doexec(cmd) < 0)
		alert("Move failed", "Check that the destination directory exists and is writable.");
	else
		loaddirs();
	free(cmd);
	free(qdest);
	free(qfrom);
}

int
plumbfile(char *dir, char *name)
{
	char *f = smprint("%s/%s", dir, name);
	if (access(f, F_OK) != 0) {
		alert("File does not exist anymore", nil);
		free(f);
		loaddirs();
		redraw();
		return 0;
	}
	pid_t pid = fork();
	if (pid == 0) {
		execlp("xdg-open", "xdg-open", f, (char *)nil);
		_exit(1);
	}
	free(f);
	return 1;
}

void
alert(const char *msg, const char *err)
{
	int h = Padding + font->height + Padding;
	if (err)
		h += font->height;
	int mw = xstringwidth(msg);
	int ew = err ? xstringwidth(err) : 0;
	int w = Padding + max(mw, ew) + Padding;
	Point o = addpt(screenr.min, Pt((Dx(screenr) - w) / 2, (Dy(screenr) - h) / 2));
	Rectangle r = Rect(o.x, o.y, o.x + w, o.y + h);

	int done = 0;
	while (!done) {
		xfillrect(r, c_alertbg);
		XSetForeground(dpy, gc, c_alertfg.pixel);
		XSetLineAttributes(dpy, gc, 2, LineSolid, CapButt, JoinMiter);
		XDrawRectangle(dpy, win, gc, r.min.x, r.min.y, Dx(r) - 1, Dy(r) - 1);
		XSetLineAttributes(dpy, gc, 1, LineSolid, CapButt, JoinMiter);

		Point p = addpt(o, Pt(Padding, Padding + font->ascent));
		xdrawstring(p, c_alertfg, msg);
		if (err) {
			p.y += font->height;
			xdrawstring(p, c_alertfg, err);
		}
		XFlush(dpy);

		XEvent e;
		XNextEvent(dpy, &e);
		if (e.type == KeyPress) {
			KeySym k;
			XLookupString(&e.xkey, nil, 0, &k, nil);
			if (k == XK_Return || k == XK_Escape)
				done = 1;
		} else if (e.type == ButtonPress) {
			if (ptinrect(Pt(e.xbutton.x, e.xbutton.y), r))
				done = 1;
		} else if (e.type == Expose) {
		}
	}
	redraw();
}

int
confirm(const char *msg)
{
	int h = Padding + font->height + Padding;
	int w = Padding + xstringwidth(msg) + xstringwidth(" Yes / No") + Padding;
	Point o = addpt(screenr.min, Pt((Dx(screenr) - w) / 2, (Dy(screenr) - h) / 2));
	Rectangle r = Rect(o.x, o.y, o.x + w, o.y + h);

	int done = 0, rc = 0;
	while (!done) {
		xfillrect(r, c_viewbg);
		xdrawrect(r, c_high);

		Point p = addpt(o, Pt(Padding, Padding + font->ascent));
		p = drawtext(p, c_viewfg, (char *)msg, r.max.x - Padding);
		p = drawtext(p, c_high, " Y", r.max.x - Padding);
		p = drawtext(p, c_viewfg, "es /", r.max.x - Padding);
		p = drawtext(p, c_high, " N", r.max.x - Padding);
		drawtext(p, c_viewfg, "o", r.max.x - Padding);
		XFlush(dpy);

		XEvent e;
		XNextEvent(dpy, &e);
		if (e.type == KeyPress) {
			KeySym k;
			XLookupString(&e.xkey, nil, 0, &k, nil);
			if (k == XK_Escape || k == XK_n || k == XK_N) {
				done = 1;
				rc = 0;
			} else if (k == XK_y || k == XK_Y || k == XK_Return) {
				done = 1;
				rc = 1;
			}
		} else if (e.type == ButtonPress) {
			if (ptinrect(Pt(e.xbutton.x, e.xbutton.y), r)) {
				done = 1;
				rc = 0;
			}
		}
	}
	redraw();
	return rc;
}

int
enter(const char *prompt, char *buf, int nbuf)
{
	int pw = xstringwidth(prompt);
	int w = Padding + pw + 300 + Padding;
	int h = Padding + font->height + Padding + font->height + Padding + 4;
	Point o = addpt(screenr.min, Pt((Dx(screenr) - w) / 2, (Dy(screenr) - h) / 2));
	Rectangle r = Rect(o.x, o.y, o.x + w, o.y + h);

	int len = strlen(buf);
	int done = 0, rc = 0;
	while (!done) {
		xfillrect(r, c_viewbg);
		xdrawrect(r, c_high);

		Point p = addpt(o, Pt(Padding, Padding + font->ascent));
		xdrawstring(p, c_viewfg, prompt);
		p.y += font->height + Padding;
		xdrawstring(p, c_viewfg, buf);
		int cw = xstringwidth(buf);
		XSetForeground(dpy, gc, c_viewfg.pixel);
		XDrawLine(dpy, win, gc, p.x + cw, p.y - font->ascent, p.x + cw, p.y + font->descent);
		XFlush(dpy);

		XEvent e;
		XNextEvent(dpy, &e);
		if (e.type == KeyPress) {
			KeySym k;
			char str[32];
			int n = XLookupString(&e.xkey, str, sizeof(str), &k, nil);
			if (k == XK_Escape) {
				done = 1;
				rc = 0;
			} else if (k == XK_Return) {
				done = 1;
				rc = len;
			} else if (k == XK_BackSpace && len > 0) {
				buf[--len] = 0;
			} else if (n == 1 && isprint((unsigned char)str[0]) && len < nbuf - 1) {
				buf[len++] = str[0];
				buf[len] = 0;
			}
		}
	}
	redraw();
	return rc;
}

int
menuhit(int x, int y, char **items, int nitems)
{
	int itemh = font->height + Padding;
	int w = 0;
	for (int i = 0; i < nitems; i++) {
		int tw = xstringwidth(items[i]);
		if (tw > w)
			w = tw;
	}
	w += 2 * Padding;
	int h = nitems * itemh + Padding;
	Rectangle r = Rect(x, y, x + w, y + h);
	if (r.max.x > screenr.max.x) {
		int dx = Dx(r);
		r.min.x = screenr.max.x - dx;
		r.max.x = screenr.max.x;
	}
	if (r.max.y > screenr.max.y) {
		int dy = Dy(r);
		r.min.y = screenr.max.y - dy;
		r.max.y = screenr.max.y;
	}

	int sel = -1;
	int done = 0;
	while (!done) {
		xfillrect(r, c_viewbg);
		xdrawrect(r, c_high);
		for (int i = 0; i < nitems; i++) {
			Point p = addpt(r.min, Pt(Padding, Padding + font->ascent + i * itemh));
			xdrawstring(p, c_viewfg, items[i]);
		}
		XFlush(dpy);

		XEvent e;
		XNextEvent(dpy, &e);
		if (e.type == ButtonRelease) {
			Point p = Pt(e.xbutton.x, e.xbutton.y);
			if (ptinrect(p, r)) {
				sel = (p.y - r.min.y) / itemh;
				if (sel < 0 || sel >= nitems)
					sel = -1;
			}
			done = 1;
		}
	}
	redraw();
	return sel;
}

char *
mdate(Dir d)
{
	char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	struct tm *tm = localtime(&d.mtime);
	char *s = malloc(32);
	snprintf(s, 32, "%s %02d %02d:%02d", months[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min);
	return s;
}

void
drawdir(int n, int isselected)
{
	if (offset + n >= ndirs)
		return;
	Dir *d = &dirs[offset + n];
	XftColor *bg, *fg;
	if (isselected) {
		bg = &c_sel2bg;
		fg = &c_sel2fg;
	} else {
		bg = &c_viewbg;
		fg = &c_viewfg;
	}

	Point p = addpt(viewr.min, Pt(Toolpadding, Toolpadding));
	p.y += n * lineh;
	Rectangle r = Rect(p.x, p.y, viewr.max.x - Toolpadding, p.y + lineh);
	xfillrect(r, *bg);

	char *t = mdate(*d);
	char buf[256];
	snprintf(buf, sizeof(buf), "%*lld  %s", sizew, d->length, t);
	free(t);

	char *prefix = d->isdir ? "d" : "-";
	int pw = xstringwidth(prefix);
	Point pp = addpt(p, Pt(0, (lineh - font->height) / 2 + font->ascent));
	xdrawstring(pp, *fg, prefix);
	p.x += pw + 4 + Padding;
	p.y += (lineh - font->height) / 2 + font->ascent;
	p = drawtext(p, *fg, d->name, viewr.max.x - xstringwidth(buf) - 2 * Padding - Toolpadding);
	p.x = viewr.max.x - xstringwidth(buf) - 2 * Padding - Toolpadding;
	xdrawstring(p, *fg, buf);
}

void
flash(int n)
{
	for (int i = 0; i < 5; i++) {
		drawdir(n, i & 1);
		XFlush(dpy);
		usleep(50000);
	}
}

void
redraw(void)
{
	xfillrect(screenr, c_viewbg);

	xfillrect(toolr, c_toolbg);
	xdrawline(Pt(toolr.min.x, toolr.max.y), Pt(toolr.max.x, toolr.max.y), c_toolfg);

	Point p = addpt(toolr.min, Pt(0, Toolpadding));
	homer = drawbutton(&p, c_toolfg, "~");
	cdr = drawbutton(&p, c_toolfg, "@");
	upr = drawbutton(&p, c_toolfg, "^");
	p.x += Toolpadding;
	p.y = toolr.min.y + (Dy(toolr) - font->height) / 2 + font->ascent;
	pathr = Rect(p.x, p.y - font->ascent, p.x + xstringwidth(path), p.y + font->descent);
	p = drawtext(p, c_toolfg, path, screenr.max.x - 2 * (Toolpadding + xstringwidth("+") + Toolpadding));
	p.x = screenr.max.x - 2 * (Toolpadding + xstringwidth("+") + Toolpadding);
	p.y = toolr.min.y + Toolpadding;
	newdirr = drawbutton(&p, c_toolfg, "+");
	newfiler = drawbutton(&p, c_toolfg, "f");

	xfillrect(scrollr, c_scrollbg);
	if (ndirs > 0) {
		int h = ((double)nlines / ndirs) * Dy(scrollr);
		int y = ((double)offset / ndirs) * Dy(scrollr);
		scrposr = Rect(scrollr.min.x, scrollr.min.y + y, scrollr.max.x - 1, scrollr.min.y + y + h);
	} else {
		scrposr = scrollr;
	}
	xfillrect(scrposr, c_scrollfg);

	for (int i = 0; i < nlines && offset + i < ndirs; i++)
		drawdir(i, (offset + i == selected) ? 1 : 0);

	XFlush(dpy);
}

int
scrollclamp(int off)
{
	if (nlines >= ndirs)
		return 0;
	if (off < 0)
		return 0;
	if (off + nlines > ndirs)
		return ndirs - nlines;
	return off;
}

void
scrollup(int off)
{
	int newoff = scrollclamp(offset - off);
	if (newoff != offset) {
		offset = newoff;
		redraw();
	}
}

void
scrolldown(int off)
{
	int newoff = scrollclamp(offset + off);
	if (newoff != offset) {
		offset = newoff;
		redraw();
	}
}

void
evtresize(int w, int h)
{
	screenr = Rect(0, 0, w, h);
	lineh = Padding + font->height + Padding;
	toolr = Rect(0, 0, w, 16 + 2 * Toolpadding);
	scrollr = Rect(0, toolr.max.y, Scrollwidth, h);
	viewr = Rect(Scrollwidth, toolr.max.y, w, h);
	nlines = Dy(viewr) / lineh;
	if (xftdraw)
		XftDrawDestroy(xftdraw);
	xftdraw = XftDrawCreate(dpy, win, visual, cmap);
	redraw();
}

void
evtkey(XEvent *e)
{
	KeySym k;
	XLookupString(&e->xkey, nil, 0, &k, nil);
	switch (k) {
	case XK_q:
	case XK_Delete:
		exit(0);
	case XK_Page_Up:
		scrollup(nlines);
		break;
	case XK_Page_Down:
		scrolldown(nlines);
		break;
	case XK_Home:
		cd(home);
		redraw();
		break;
	case XK_Up:
		up();
		redraw();
		break;
	case XK_Return:
		if (selected >= 0 && selected < ndirs) {
			Dir *d = &dirs[selected];
			if (d->isdir) {
				cd(d->name);
				redraw();
			} else {
				plumbfile(path, d->name);
				flash(selected - offset);
			}
		}
		break;
	case XK_space:
		break;
	case XK_f:
	case XK_F:
		cyclefont();
		break;
	}
}

Point
cept(const char *text)
{
	Point p;
	p.x = screenr.min.x + (Dx(screenr) - xstringwidth(text) - 4) / 2;
	p.y = screenr.min.y + (Dy(screenr) - font->height - 4) / 2;
	return p;
}

int
indexat(Point p)
{
	if (!ptinrect(p, viewr))
		return -1;
	int n = (p.y - viewr.min.y) / lineh;
	if (offset + n >= ndirs)
		return -1;
	return offset + n;
}

void
evtmouse(XEvent *e)
{
	static int buttons = 0;
	static Time lastclick = 0;
	static int lastclickidx = -1;
	Point mxy;
	int b = 0;

	if (e->type == ButtonPress) {
		b = e->xbutton.button;
		mxy = Pt(e->xbutton.x, e->xbutton.y);
		if (b == 4) {
			scrollup(Slowscroll);
			return;
		}
		if (b == 5) {
			scrolldown(Slowscroll);
			return;
		}
		if (b == 1)
			buttons |= 1;
		else if (b == 2)
			buttons |= 2;
		else if (b == 3)
			buttons |= 4;
	} else if (e->type == ButtonRelease) {
		b = e->xbutton.button;
		mxy = Pt(e->xbutton.x, e->xbutton.y);
		if (b == 1)
			buttons &= ~1;
		else if (b == 2)
			buttons &= ~2;
		else if (b == 3)
			buttons &= ~4;
	} else if (e->type == MotionNotify) {
		mxy = Pt(e->xmotion.x, e->xmotion.y);
	}

	if (oldbuttons == 0 && buttons != 0 && ptinrect(mxy, scrollr))
		scrolling = 1;
	else if (buttons == 0)
		scrolling = 0;

	if (buttons & 1) {
		if (scrolling && e->type == MotionNotify) {
			int dy = 1 + nlines * ((double)(mxy.y - scrollr.min.y) / Dy(scrollr));
			scrollup(dy);
		} else if (e->type == ButtonPress && ptinrect(mxy, viewr)) {
			int idx = indexat(mxy);
			if (idx >= 0) {
				Time now = e->xbutton.time;
				if (idx == lastclickidx && (now - lastclick < 400)) {
					if (dirs[idx].isdir) {
						cd(dirs[idx].name);
						redraw();
					} else {
						if (plumbfile(path, dirs[idx].name))
							flash(idx - offset);
					}
					lastclickidx = -1;
				} else {
					selected = idx;
					lastclickidx = idx;
					lastclick = now;
					redraw();
				}
			} else {
				selected = -1;
				lastclickidx = -1;
				redraw();
			}
		}
	} else if (buttons & 2) {
		if (e->type == ButtonPress && ptinrect(mxy, viewr)) {
			int idx = indexat(mxy);
			if (idx >= 0) {
				char *items[] = { "rename", "delete", "move" };
				int sel = menuhit(mxy.x, mxy.y, items, 3);
				if (sel == 1) {
					rm(dirs[idx]);
					if (selected == idx) selected = -1;
					redraw();
				} else if (sel == 0) {
					char buf[4096] = {0};
					snprintf(buf, sizeof(buf), "%s", dirs[idx].name);
					if (enter("Rename to", buf, sizeof(buf)) > 0) {
						mv(dirs[idx].name, buf);
						if (selected == idx) selected = -1;
						redraw();
					}
				} else if (sel == 2) {
					char destdir[4096] = {0};
					if (enter("Move to directory", destdir, sizeof(destdir)) > 0) {
						char *expanded_dest = expandtilde(destdir);
						char *destdir_abs = abspath(path, expanded_dest);
						struct stat st;
						if (stat(destdir_abs, &st) < 0 || !S_ISDIR(st.st_mode)) {
							alert("Invalid destination", "The path is not a directory or does not exist.");
						} else {
							char *src = smprint("%s/%s", path, dirs[idx].name);
							char *dest = smprint("%s/%s", destdir_abs, dirs[idx].name);
							movefile(src, dest);
							free(src);
							free(dest);
							if (selected == idx) selected = -1;
						}
						free(destdir_abs);
						free(expanded_dest);
						redraw();
					}
				}
			}
		} else if (scrolling && e->type == MotionNotify) {
			if (nlines < ndirs) {
				offset = scrollclamp(mxy.y * ndirs / Dy(scrollr));
				redraw();
			}
		} else if (ptinrect(mxy, pathr) && e->type == ButtonPress) {
			loaddirs();
			redraw();
		}
	}
	if ((buttons & 4) && oldbuttons == 0 && e->type == ButtonPress) {
		if (scrolling) {
			int dy = 1 + nlines * ((double)(mxy.y - scrollr.min.y) / Dy(scrollr));
			scrolldown(dy);
		} else if (ptinrect(mxy, homer)) {
			cd(home);
			redraw();
		} else if (ptinrect(mxy, upr)) {
			up();
			redraw();
		} else if (ptinrect(mxy, cdr)) {
			char buf[4096] = {0};
			if (enter("Go to directory", buf, sizeof(buf)) > 0) {
				cd(buf);
				redraw();
			}
		} else if (ptinrect(mxy, pathr)) {
			loaddirs();
			redraw();
		} else if (ptinrect(mxy, newdirr)) {
			char buf[4096] = {0};
			if (enter("Create directory", buf, sizeof(buf)) > 0) {
				mk_dir(buf);
				redraw();
			}
		} else if (ptinrect(mxy, newfiler)) {
			char buf[4096] = {0};
			if (enter("Create file", buf, sizeof(buf)) > 0) {
				touch(buf);
				redraw();
			}
		} else if (ptinrect(mxy, viewr)) {
			int idx = indexat(mxy);
			if (idx >= 0) {
				char *items[] = { "open" };
				int sel = menuhit(mxy.x, mxy.y, items, 1);
				if (sel == 0) {
					Dir *d = &dirs[idx];
					if (d->isdir) {
						cd(d->name);
						redraw();
					} else {
						if (plumbfile(path, d->name))
							flash(idx - offset);
					}
				}
			}
		}
	}

	if (e->type == MotionNotify && buttons == 0) {
		int idx = indexat(mxy);
		if (idx != lastn) {
			if (lastn != -1)
				drawdir(lastn - offset, 0);
			if (idx != -1)
				drawdir(idx - offset, 1);
			lastn = idx;
			XFlush(dpy);
		}
	}

	oldbuttons = buttons;
}

void
showerrstr(const char *msg)
{
	alert(msg, strerror(errno));
}

char *
smprint(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char *s;
	vasprintf(&s, fmt, ap);
	va_end(ap);
	return s;
}

void
readhome(void)
{
	home = getenv("HOME");
	if (!home)
		home = "/";
}

void
usage(void)
{
	fprintf(stderr, "usage: jfm [path]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	setlocale(LC_CTYPE, "");

	dpy = XOpenDisplay(nil);
	if (!dpy) {
		fprintf(stderr, "cannot open display\n");
		return 1;
	}
	scr = DefaultScreen(dpy);
	visual = DefaultVisual(dpy, scr);
	cmap = DefaultColormap(dpy, scr);
	depth = DefaultDepth(dpy, scr);

	int w = 800, h = 600;
	win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, w, h, 0,
				  BlackPixel(dpy, scr), WhitePixel(dpy, scr));
	XSelectInput(dpy, win, ExposureMask | StructureNotifyMask | ButtonPressMask |
		     ButtonReleaseMask | PointerMotionMask | KeyPressMask);
	XMapWindow(dpy, win);

	gc = XCreateGC(dpy, win, 0, nil);
	loadfont("Tamzen-12");

	XStoreName(dpy, win, "jfm");

	Atom wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wm_delete, 1);

	if (getcwd(path, sizeof(path)) == NULL)
		strcpy(path, "/");
	if (argc > 2)
		usage();
	if (argc == 2 && access(argv[1], F_OK) == 0) {
		char *tmp = abspath(path, argv[1]);
		strncpy(path, tmp, sizeof(path) - 1);
		path[sizeof(path) - 1] = 0;
		free(tmp);
	}

	readhome();
	loaddirs();
	initcolors();

	evtresize(w, h);

	while (1) {
		XEvent e;
		XNextEvent(dpy, &e);
		switch (e.type) {
		case Expose:
			if (e.xexpose.count == 0)
				redraw();
			break;
		case ConfigureNotify:
			if (e.xconfigure.width != Dx(screenr) || e.xconfigure.height != Dy(screenr))
				evtresize(e.xconfigure.width, e.xconfigure.height);
			break;
		case ButtonPress:
		case ButtonRelease:
		case MotionNotify:
			evtmouse(&e);
			break;
		case KeyPress:
			evtkey(&e);
			break;
		case ClientMessage:
			if ((Atom)e.xclient.data.l[0] == wm_delete)
				exit(0);
			break;
		}
	}
	return 0;
}
