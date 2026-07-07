#include "jfm.h"

#define C_TOOLBG_R 0x0000
#define C_TOOLBG_G 0x0000
#define C_TOOLBG_B 0x0000

#define C_TOOLFG_R 0xd5d5
#define C_TOOLFG_G 0xc0c0
#define C_TOOLFG_B 0xa4a4

#define C_VIEWBG_R 0x0000
#define C_VIEWBG_G 0x0000
#define C_VIEWBG_B 0x0000

#define C_VIEWFG_R 0xd5d5
#define C_VIEWFG_G 0xc0c0
#define C_VIEWFG_B 0xa4a4

#define C_SELBG_R 0x1a1a
#define C_SELBG_G 0x1a1a
#define C_SELBG_B 0x1a1a

#define C_SELFG_R 0xd5d5
#define C_SELFG_G 0xc0c0
#define C_SELFG_B 0xa4a4

#define C_SCROLLBG_R 0x3333
#define C_SCROLLBG_G 0x3333
#define C_SCROLLBG_B 0x3333

#define C_SCROLLFG_R 0xd5d5
#define C_SCROLLFG_G 0xc0c0
#define C_SCROLLFG_B 0xa4a4

#define C_HIGH_R 0xd5d5
#define C_HIGH_G 0xc0c0
#define C_HIGH_B 0xa4a4

#define C_ALERTBG_R 0x4a4a
#define C_ALERTBG_G 0x1a1a
#define C_ALERTBG_B 0x1a1a

#define C_ALERTFG_R 0xd5d5
#define C_ALERTFG_G 0xc0c0
#define C_ALERTFG_B 0xa4a4

#define C_SEL2BG_R 0x3d3d
#define C_SEL2BG_G 0x3535
#define C_SEL2BG_B 0x2b2b

#define C_SEL2FG_R 0xd5d5
#define C_SEL2FG_G 0xc0c0
#define C_SEL2FG_B 0xa4a4

static char *fontlist[] = {
	"Tamzen-12",
	"Tamzen-10",
	"Tamzen-14",
	"Tamzen-16",
	"monospace-12",
	"monospace-10",
	"monospace-14",
	"monospace-16",
};
#define NFONTS (sizeof(fontlist) / sizeof(fontlist[0]))

char *fontname;

void
initcolors(void)
{
	XRenderColor rc;
	rc.alpha = 0xffff;

	rc.red = C_TOOLBG_R; rc.green = C_TOOLBG_G; rc.blue = C_TOOLBG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_toolbg);
	rc.red = C_TOOLFG_R; rc.green = C_TOOLFG_G; rc.blue = C_TOOLFG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_toolfg);
	rc.red = C_VIEWBG_R; rc.green = C_VIEWBG_G; rc.blue = C_VIEWBG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_viewbg);
	rc.red = C_VIEWFG_R; rc.green = C_VIEWFG_G; rc.blue = C_VIEWFG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_viewfg);
	rc.red = C_SELBG_R; rc.green = C_SELBG_G; rc.blue = C_SELBG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_selbg);
	rc.red = C_SELFG_R; rc.green = C_SELFG_G; rc.blue = C_SELFG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_selfg);
	rc.red = C_SCROLLBG_R; rc.green = C_SCROLLBG_G; rc.blue = C_SCROLLBG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_scrollbg);
	rc.red = C_SCROLLFG_R; rc.green = C_SCROLLFG_G; rc.blue = C_SCROLLFG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_scrollfg);
	rc.red = C_HIGH_R; rc.green = C_HIGH_G; rc.blue = C_HIGH_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_high);
	rc.red = C_ALERTBG_R; rc.green = C_ALERTBG_G; rc.blue = C_ALERTBG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_alertbg);
	rc.red = C_ALERTFG_R; rc.green = C_ALERTFG_G; rc.blue = C_ALERTFG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_alertfg);

	rc.red = C_SEL2BG_R; rc.green = C_SEL2BG_G; rc.blue = C_SEL2BG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_sel2bg);
	rc.red = C_SEL2FG_R; rc.green = C_SEL2FG_G; rc.blue = C_SEL2FG_B;
	XftColorAllocValue(dpy, visual, cmap, &rc, &c_sel2fg);
}

void
loadfont(const char *name)
{
	XftFont *nf = XftFontOpenName(dpy, scr, name);
	if (!nf) {
		static char *fallbacks[] = {
			"Tamzen-12", "Tamzen-10", "Tamzen-14", "Tamzen-16",
			"monospace-12", "monospace-10", "monospace-14", "monospace-16",
			"fixed"
		};
		for (int i = 0; i < 9 && !nf; i++) {
			if (strcmp(name, fallbacks[i]) != 0)
				nf = XftFontOpenName(dpy, scr, fallbacks[i]);
		}
	}
	if (nf) {
		if (font)
			XftFontClose(dpy, font);
		font = nf;
		free(fontname);
		fontname = strdup(name);
		if (xftdraw)
			XftDrawDestroy(xftdraw);
		xftdraw = XftDrawCreate(dpy, win, visual, cmap);
		evtresize(Dx(screenr), Dy(screenr));
	}
}

void
cyclefont(void)
{
	static int idx = 0;
	idx = (idx + 1) % NFONTS;
	loadfont(fontlist[idx]);
}
