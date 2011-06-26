/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance.  Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag.  Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask))
#define INRECT(X,Y,RX,RY,RW,RH) ((X) >= (RX) && (X) < (RX) + (RW) && (Y) >= (RY) && (Y) < (RY) + (RH))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TEXTW(X)                (textnw(X, strlen(X)) + dc.font.height)
#define SELVIEW(M)              (M->views[ M->selview ])
#define NUMVIEWS                9

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast };        /* cursor */
enum { ColBorder, ColFG, ColBG, ColLast };              /* color */
enum { NetSupported, NetWMName, NetWMState,
       NetWMFullscreen, NetLast };                      /* EWMH atoms */
enum { WMProtocols, WMDelete, WMState, WMLast };        /* default atoms */
enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
       ClkClientWin, ClkRootWin, ClkLast };             /* clicks */

typedef union {
	int i;
	unsigned int ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int click;
	unsigned int mask;
	unsigned int button;
	void (*func)(const Arg *arg);
	const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
	char name[256];
	float mina, maxa;
	int x, y, w, h;
	int oldx, oldy, oldw, oldh;
	int basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int bw, oldbw;
	Bool isfixed, isfloating, isurgent, oldstate;
	Client *next;
	Client *snext;
	Monitor *mon;
	unsigned int view;
	Window win;
};

typedef struct {
	int x, y, w, h;
	unsigned long norm[ColLast];
	unsigned long sel[ColLast];
	Drawable drawable;
	GC gc;
	struct {
		int ascent;
		int descent;
		int height;
		XFontSet set;
		XFontStruct *xfont;
	} font;
} DC; /* draw context */

typedef struct {
	unsigned int mod;
	KeySym keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	const char *symbol;
	void (*arrange)(Monitor *);
} Layout;

typedef struct {
	float mfact;
	Client *clients;
	Client *sel;
	Client *stack;
	const Layout *lt;
} View;

struct Monitor {
	char ltsymbol[16];
	int num;
	int by;               /* bar geometry */
	int mx, my, mw, mh;   /* screen size */
	int wx, wy, ww, wh;   /* window area  */
	Bool showbar;
	Bool topbar;
	Monitor *next;
	Window barwin;
	unsigned int selview;
	View views[ 9 ];
};

/* function declarations */
static Bool applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact);
static void arrange(Monitor *const m);
static void arrangemon( Monitor *const m );
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress( XEvent *e );
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clearurgent(Client *c);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest( XEvent *e );
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach( Client *c );
static void detachstack(Client *c);
static void die(const char *errstr, ...);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawsquare(Bool filled, Bool empty, Bool invert, unsigned long col[ColLast]);
static void drawtext(const char *text, unsigned long col[ColLast], Bool invert);
static void enternotify( XEvent *e );
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin( XEvent *e );
static void focusmon( const Arg *arg );
static void focusstack(const Arg *arg);
static unsigned long getcolor(const char *colstr);
static Bool getrootptr(int *x, int *y);
static long getstate(Window w);
static Bool gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, Bool focused);
static void grabkeys(void);
static Bool hasurgentclient( const View *const v );
static void initfont(const char *fontstr);
static Bool isprotodel(Client *c);
static void keypress(XEvent *e);
static void killclient( const Arg *arg );
static void manage( Window w, XWindowAttributes *wa );
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void mirrortile( Monitor *const m );
static void monocle( Monitor *const m );
static void movemouse( const Arg *arg );
static Client *nexttiled(Client *c);
static Monitor *ptrtomon(int x, int y);
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static void resize(Client *c, int x, int y, int w, int h, Bool interact);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse( const Arg *arg );
static void restack( Monitor *const m );
static void run(void);
static void scan(void);
static void sendmon( Client *c, Monitor *m );
static void setclientstate(Client *c, long state);
static void setlayout( const Arg *arg );
static void setmfact(const Arg *arg);
static void setup(void);
static void showhide(Client *c);
static void sigchld(int unused);
static void spawn(const Arg *arg);
static void tag(const Arg *arg);
static void tagmon( const Arg *arg );
static int textnw(const char *text, unsigned int len);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void togglefloating( const Arg *arg );
static void unfocus(Client *c, Bool setfocus);
static void unmanage( Client *c, Bool destroyed );
static void unmapnotify(XEvent *e);
static Bool updategeom(void);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatetitle(Client *c);
static void updatewmhints( Client *c );
static void view(const Arg *arg);
static Client *wintoclient( Window w );
static Monitor *wintomon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);

/* variables */
static const char broken[] = "broken";
static char stext[256];
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
	[ButtonPress] = buttonpress,
	[ClientMessage] = clientmessage,
	[ConfigureRequest] = configurerequest,
	[ConfigureNotify] = configurenotify,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast];
static Bool otherwm;
static Bool running = True;
static Cursor cursor[CurLast];
static Display *dpy;
static DC dc;
static Monitor *mons = NULL, *selmon = NULL;
static Window root;

/* configuration, allows nested code to access above variables */
#include "config.h"

/* function implementations */
Bool
applysizehints(Client *c, int *x, int *y, int *w, int *h, Bool interact) {
	Bool baseismin;
	Monitor *m = c->mon;

	/* set minimum possible */
	*w = MAX(1, *w);
	*h = MAX(1, *h);
	if(interact) {
		if(*x > sw)
			*x = sw - WIDTH(c);
		if(*y > sh)
			*y = sh - HEIGHT(c);
		if(*x + *w + 2 * c->bw < 0)
			*x = 0;
		if(*y + *h + 2 * c->bw < 0)
			*y = 0;
	}
	else {
		if(*x > m->mx + m->mw)
			*x = m->mx + m->mw - WIDTH(c);
		if(*y > m->my + m->mh)
			*y = m->my + m->mh - HEIGHT(c);
		if(*x + *w + 2 * c->bw < m->mx)
			*x = m->mx;
		if(*y + *h + 2 * c->bw < m->my)
			*y = m->my;
	}
	if(*h < bh)
		*h = bh;
	if(*w < bh)
		*w = bh;
	if(resizehints || c->isfloating) {
		/* see last two sentences in ICCCM 4.1.2.3 */
		baseismin = c->basew == c->minw && c->baseh == c->minh;
		if(!baseismin) { /* temporarily remove base dimensions */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for aspect limits */
		if(c->mina > 0 && c->maxa > 0) {
			if(c->maxa < (float)*w / *h)
				*w = *h * c->maxa + 0.5;
			else if(c->mina < (float)*h / *w)
				*h = *w * c->mina + 0.5;
		}
		if(baseismin) { /* increment calculation requires this */
			*w -= c->basew;
			*h -= c->baseh;
		}
		/* adjust for increment value */
		if(c->incw)
			*w -= *w % c->incw;
		if(c->inch)
			*h -= *h % c->inch;
		/* restore base dimensions */
		*w += c->basew;
		*h += c->baseh;
		*w = MAX(*w, c->minw);
		*h = MAX(*h, c->minh);
		if(c->maxw)
			*w = MIN(*w, c->maxw);
		if(c->maxh)
			*h = MIN(*h, c->maxh);
	}
	return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

void
arrange( Monitor *m ) {
	if ( m )
		showhide( SELVIEW( m ).stack );
	else for( m = mons ; m ; m = m->next )
		showhide( SELVIEW( m ).stack );
	focus( NULL );
	if ( m )
		arrangemon( m );
	else for( m = mons ; m ; m = m->next )
		arrangemon( m );
}

void
arrangemon( Monitor *const m ) {
	strncpy( m->ltsymbol, SELVIEW( m ).lt->symbol, sizeof m->ltsymbol );
	if ( SELVIEW( m ).lt->arrange )
		SELVIEW( m ).lt->arrange( m );
	restack( m );
}

void
attach(Client *c) {
	c->next = c->mon->views[ c->view ].clients;
	c->mon->views[ c->view ].clients = c;
}

void
attachstack(Client *c) {
	c->snext = c->mon->views[ c->view ].stack;
	c->mon->views[ c->view ].stack = c;
}

void
buttonpress( XEvent *e ) {
	unsigned int i, x, click;
	Arg arg = {0};
	Client *c;
	Monitor *m;
	XButtonPressedEvent *ev = &e->xbutton;

	click = ClkRootWin;
	/* focus monitor if necessary */
	if ( ( m = wintomon( ev->window ) ) && m != selmon ) {
		unfocus( SELVIEW( selmon ).sel, True );
		selmon = m;
		focus( NULL );
	}
	if ( ev->window == selmon->barwin ) {
		i = x = 0;
		do {
			x += TEXTW( tags[ i ] );
		} while( ev->x >= x && ++i < NUMVIEWS );
		if ( i < NUMVIEWS ) {
			click = ClkTagBar;
			arg.ui = 1 << i;
		}
		else if ( ev->x < x + blw )
			click = ClkLtSymbol;
		else if ( ev->x > selmon->wx + selmon->ww - TEXTW( stext ) )
			click = ClkStatusText;
		else
			click = ClkWinTitle;
	}
	else if ( ( c = wintoclient( ev->window ) ) ) {
		focus( c );
		click = ClkClientWin;
	}
	for ( i = 0 ; i < LENGTH( buttons ) ; i++ )
		if ( click == buttons[ i ].click
			&& buttons[ i ].func
			&& buttons[ i ].button == ev->button
			&& ( CLEANMASK( buttons[ i ].mask ) == CLEANMASK( ev->state ) ) )
			buttons[ i ].func( click == ClkTagBar && buttons[ i ].arg.i == 0 ? &arg : &buttons[ i ].arg );
}

void
checkotherwm(void) {
	otherwm = False;
	xerrorxlib = XSetErrorHandler(xerrorstart);
	/* this causes an error if some other window manager is running */
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	if(otherwm)
		die("dwm: another window manager is already running\n");
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void) {
	Monitor *m;
	int i;

	for ( m = mons ; m ; m = m->next )
		for ( i = 0 ; i < NUMVIEWS ; i++ )
			while ( m->views[ i ].clients )
				unmanage( m->views[ i ].clients, False );
	if ( dc.font.set )
		XFreeFontSet( dpy, dc.font.set );
	else
		XFreeFont( dpy, dc.font.xfont );
	XUngrabKey( dpy, AnyKey, AnyModifier, root );
	XFreePixmap( dpy, dc.drawable );
	XFreeGC( dpy, dc.gc );
	XFreeCursor( dpy, cursor[ CurNormal ] );
	XFreeCursor( dpy, cursor[ CurResize ] );
	XFreeCursor( dpy, cursor[ CurMove ] );
	while ( mons )
		cleanupmon( mons );
	XSync( dpy, False );
	XSetInputFocus( dpy, PointerRoot, RevertToPointerRoot, CurrentTime );
}

void
cleanupmon(Monitor *mon) {
	Monitor *m;

	if(mon == mons)
		mons = mons->next;
	else {
		for(m = mons; m && m->next != mon; m = m->next);
		m->next = mon->next;
	}
	XUnmapWindow(dpy, mon->barwin);
	XDestroyWindow(dpy, mon->barwin);
	free(mon);
}

void
clearurgent(Client *c) {
	XWMHints *wmh;

	c->isurgent = False;
	if(!(wmh = XGetWMHints(dpy, c->win)))
		return;
	wmh->flags &= ~XUrgencyHint;
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
configure(Client *c) {
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

void
configurenotify(XEvent *e) {
	Monitor *m;
	XConfigureEvent *ev = &e->xconfigure;

	if(ev->window == root) {
		sw = ev->width;
		sh = ev->height;
		if(updategeom()) {
			if(dc.drawable != 0)
				XFreePixmap(dpy, dc.drawable);
			dc.drawable = XCreatePixmap(dpy, root, sw, bh, DefaultDepth(dpy, screen));
			updatebars();
			for(m = mons; m; m = m->next)
				XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, m->ww, bh);
			arrange(NULL);
		}
	}
}

void
configurerequest( XEvent *e ) {
	Client *c;
	Monitor *m;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ( ( c = wintoclient( ev->window ) ) ) {
		if ( ev->value_mask & CWBorderWidth )
			c->bw = ev->border_width;
		else if ( c->isfloating || !SELVIEW( selmon ).lt->arrange ) {
			m = c->mon;
			if ( ev->value_mask & CWX )
				c->x = m->mx + ev->x;
			if ( ev->value_mask & CWY )
				c->y = m->my + ev->y;
			if ( ev->value_mask & CWWidth )
				c->w = ev->width;
			if ( ev->value_mask & CWHeight )
				c->h = ev->height;
			if ( ( c->x + c->w ) > m->mx + m->mw && c->isfloating )
				c->x = m->mx + ( m->mw / 2 - c->w / 2 ); /* center in x direction */
			if ( ( c->y + c->h ) > m->my + m->mh && c->isfloating )
				c->y = m->my + ( m->mh / 2 - c->h / 2 ); /* center in y direction */
			if ( ( ev->value_mask & ( CWX | CWY ) ) && !( ev->value_mask & ( CWWidth | CWHeight ) ) )
				configure( c );
			XMoveResizeWindow( dpy, c->win, c->x, c->y, c->w, c->h );
		}
		else
			configure( c );
	}
	else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow( dpy, ev->window, ev->value_mask, &wc );
	}
	XSync( dpy, False );
}

Monitor *
createmon(void) {
	Monitor *m;
	int i;
	View *view;

	if ( !( m = ( Monitor * ) malloc( sizeof( Monitor ) ) ) )
		die( "fatal: could not malloc() %u bytes\n", sizeof( Monitor ) );
	m->showbar = showbar;
	m->topbar = topbar;
	strncpy( m->ltsymbol, layouts[ 0 ].symbol, sizeof m->ltsymbol );
	for ( i = 0 ; i < LENGTH( m->views ) ; i++ ) {
		view = &m->views[ i ];
		view->mfact = mfact;
		view->lt = &layouts[ 0 ];
	}

	return m;
}

void
destroynotify(XEvent *e) {
	Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if((c = wintoclient(ev->window)))
		unmanage(c, True);
}

void
detach( Client *c ) {
	Client **tc;

	for ( tc = &c->mon->views[ c->view ].clients ; *tc != c ; tc = &( *tc )->next );
	*tc = c->next;
}

void
detachstack( Client *c ) {
	Client **tc;

	for ( tc = &c->mon->views[ c->view ].stack ; *tc != c ; tc = &( *tc )->snext );
	*tc = c->snext;

	if ( c == c->mon->views[ c->view ].sel )
		c->mon->views[ c->view ].sel = c->mon->views[ c->view ].stack;
}

void
die(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

Monitor *
dirtomon(int dir) {
	Monitor *m = NULL;

	if(dir > 0) {
		if(!(m = selmon->next))
			m = mons;
	}
	else {
		if(selmon == mons)
			for(m = mons; m->next; m = m->next);
		else
			for(m = mons; m->next != selmon; m = m->next);
	}
	return m;
}

void
drawbar(Monitor *m) {
	int x;
	Bool occ, urg;
	unsigned int i;
	unsigned long *col;

	// view tags

	dc.x = 0;
	for ( i = 0 ; i < NUMVIEWS ; i++ ) {
		dc.w = TEXTW( tags[ i ] );
		col = ( i == m->selview ) ? dc.sel : dc.norm;
		occ = ( NULL == m->views[ i ].clients );
		urg = hasurgentclient( &m->views[ i ] );
		drawtext( tags[ i ], col, urg );
		drawsquare( m == selmon && m->views[ i ].sel && i == m->selview,
			occ, urg, col );
		dc.x += dc.w;
	}

	// layout name of selected view

	dc.w = blw = TEXTW( m->ltsymbol );
	drawtext( m->ltsymbol, dc.norm, False );
	dc.x += dc.w;

	// status

	x = dc.x;
	if ( m == selmon ) { /* status is only drawn on selected monitor */
		dc.w = TEXTW( stext );
		dc.x = m->ww - dc.w;
		if ( !( x <= dc.x ) ) {
			dc.x = x;
			dc.w = m->ww - x;
		}
		drawtext( stext, dc.norm, False );
	}
	else
		dc.x = m->ww;

	// name of selected client of selected view

	if ( ( dc.w = dc.x - x ) > bh ) {
		dc.x = x;
		if ( SELVIEW( m ).sel ) {
			col = ( m == selmon ) ? dc.sel : dc.norm;
			drawtext( SELVIEW( m ).sel->name, col, False );
			drawsquare( SELVIEW( m ).sel->isfixed, SELVIEW( m ).sel->isfloating, False, col );
		}
		else
			drawtext( NULL, dc.norm, False );
	}

	XCopyArea( dpy, dc.drawable, m->barwin, dc.gc, 0, 0, m->ww, bh, 0, 0 );
	XSync( dpy, False );
}

void
drawbars(void) {
	Monitor *m;

	for ( m = mons ; m ; m = m->next )
		drawbar( m );
}

void
drawsquare(Bool filled, Bool empty, Bool invert, unsigned long col[ColLast]) {
	int x;
	XGCValues gcv;
	XRectangle r = { dc.x, dc.y, dc.w, dc.h };

	gcv.foreground = col[invert ? ColBG : ColFG];
	XChangeGC(dpy, dc.gc, GCForeground, &gcv);
	x = (dc.font.ascent + dc.font.descent + 2) / 4;
	r.x = dc.x + 1;
	r.y = dc.y + 1;
	if(filled) {
		r.width = r.height = x + 1;
		XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);
	}
	else if(empty) {
		r.width = r.height = x;
		XDrawRectangles(dpy, dc.drawable, dc.gc, &r, 1);
	}
}

void
drawtext(const char *text, unsigned long col[ColLast], Bool invert) {
	char buf[256];
	int i, x, y, h, len, olen;
	XRectangle r = { dc.x, dc.y, dc.w, dc.h };

	XSetForeground(dpy, dc.gc, col[invert ? ColFG : ColBG]);
	XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);
	if(!text)
		return;
	olen = strlen(text);
	h = dc.font.ascent + dc.font.descent;
	y = dc.y + (dc.h / 2) - (h / 2) + dc.font.ascent;
	x = dc.x + (h / 2);
	/* shorten text if necessary */
	for(len = MIN(olen, sizeof buf); len && textnw(text, len) > dc.w - h; len--);
	if(!len)
		return;
	memcpy(buf, text, len);
	if(len < olen)
		for(i = len; i && i > len - 3; buf[--i] = '.');
	XSetForeground(dpy, dc.gc, col[invert ? ColBG : ColFG]);
	if(dc.font.set)
		XmbDrawString(dpy, dc.drawable, dc.font.set, dc.gc, x, y, buf, len);
	else
		XDrawString(dpy, dc.drawable, dc.gc, x, y, buf, len);
}

void
enternotify( XEvent *e ) {
	Client *c;
	Monitor *m;
	XCrossingEvent *ev = &e->xcrossing;

	if ( ( ev->window == root )
		|| ( ev->mode == NotifyNormal && ev->detail != NotifyInferior ) ) {
		m = wintomon( ev->window );
		if ( m && m != selmon ) {
			unfocus( SELVIEW( selmon ).sel, True );
			selmon = m;
		}
		c = wintoclient( ev->window );
		focus( c );
	}
}

void
expose(XEvent *e) {
	Monitor *m;
	XExposeEvent *ev = &e->xexpose;

	if(ev->count == 0 && (m = wintomon(ev->window)))
		drawbar(m);
}

void
focus( Client *c ) {
	if ( !c )
		c = SELVIEW( selmon ).stack;
	if ( SELVIEW( selmon ).sel && SELVIEW( selmon ).sel != c )
		unfocus( SELVIEW( selmon ).sel, False );
	if ( c ) {
		if ( c->mon != selmon )
			selmon = c->mon;
		if ( c->isurgent )
			clearurgent( c );
		detachstack( c );
		attachstack( c );
		grabbuttons( c, True );
		XSetWindowBorder( dpy, c->win, dc.sel[ ColBorder ] );
		XSetInputFocus( dpy, c->win, RevertToPointerRoot, CurrentTime );
	}
	else
		XSetInputFocus( dpy, root, RevertToPointerRoot, CurrentTime );
	SELVIEW( selmon ).sel = c;
	drawbars();
}

void
focusin( XEvent *e ) { /* XXX: there are some broken focus acquiring clients */
	const XFocusChangeEvent *const ev = &e->xfocus;

	if ( SELVIEW( selmon ).sel && SELVIEW( selmon ).sel->win != ev->window )
		XSetInputFocus( dpy, SELVIEW( selmon ).sel->win, RevertToPointerRoot, CurrentTime );
}

void
focusmon( const Arg *arg ) {
	Monitor *m;

	if ( ( m = dirtomon( arg->i ) ) != selmon ) {
		unfocus( SELVIEW( selmon ).sel, True );
		selmon = m;
		focus( NULL );
	}
}

void
focusstack(const Arg *arg) {
	Client *c = NULL, *i;

	if ( SELVIEW( selmon ).sel ) {
		if ( arg->i > 0 ) {
			c = SELVIEW( selmon ).sel->next;
			if ( !c )
				c = SELVIEW( selmon ).clients;
		}
		else {
			for ( i = SELVIEW( selmon ).clients ; i != SELVIEW( selmon ).sel ; i = i->next )
				c = i;
			if ( !c )
				for ( ; i ; i = i->next )
					c = i;
		}
		if ( c ) {
			focus( c );
			restack( selmon );
		}
	}
}

unsigned long
getcolor(const char *colstr) {
	Colormap cmap = DefaultColormap(dpy, screen);
	XColor color;

	if(!XAllocNamedColor(dpy, cmap, colstr, &color, &color))
		die("error, cannot allocate color '%s'\n", colstr);
	return color.pixel;
}

Bool
getrootptr(int *x, int *y) {
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

long
getstate(Window w) {
	int format;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	if(XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
	                      &real, &format, &n, &extra, (unsigned char **)&p) != Success)
		return -1;
	if(n != 0)
		result = *p;
	XFree(p);
	return result;
}

Bool
gettextprop(Window w, Atom atom, char *text, unsigned int size) {
	char **list = NULL;
	int n;
	XTextProperty name;

	if(!text || size == 0)
		return False;
	text[0] = '\0';
	XGetTextProperty(dpy, w, &name, atom);
	if(!name.nitems)
		return False;
	if(name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if(XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return True;
}

void
grabbuttons(Client *c, Bool focused) {
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
		if(focused) {
			for(i = 0; i < LENGTH(buttons); i++)
				if(buttons[i].click == ClkClientWin)
					for(j = 0; j < LENGTH(modifiers); j++)
						XGrabButton(dpy, buttons[i].button,
						            buttons[i].mask | modifiers[j],
						            c->win, False, BUTTONMASK,
						            GrabModeAsync, GrabModeSync, None, None);
		}
		else
			XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
			            BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
	}
}

void
grabkeys(void) {
	updatenumlockmask();
	{
		unsigned int i, j;
		unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
		KeyCode code;

		XUngrabKey(dpy, AnyKey, AnyModifier, root);
		for(i = 0; i < LENGTH(keys); i++) {
			if((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for(j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
						 True, GrabModeAsync, GrabModeAsync);
		}
	}
}

Bool
hasurgentclient( const View *const v ) {
	const Client *c;
	for ( c = v->clients ; c ; c = c->next )
		if ( c->isurgent )
			return True;
	return False;
}

void
initfont(const char *fontstr) {
	char *def, **missing;
	int i, n;

	missing = NULL;
	dc.font.set = XCreateFontSet(dpy, fontstr, &missing, &n, &def);
	if(missing) {
		while(n--)
			fprintf(stderr, "dwm: missing fontset: %s\n", missing[n]);
		XFreeStringList(missing);
	}
	if(dc.font.set) {
		XFontSetExtents *font_extents;
		XFontStruct **xfonts;
		char **font_names;

		dc.font.ascent = dc.font.descent = 0;
		font_extents = XExtentsOfFontSet(dc.font.set);
		n = XFontsOfFontSet(dc.font.set, &xfonts, &font_names);
		for(i = 0, dc.font.ascent = 0, dc.font.descent = 0; i < n; i++) {
			dc.font.ascent = MAX(dc.font.ascent, (*xfonts)->ascent);
			dc.font.descent = MAX(dc.font.descent,(*xfonts)->descent);
			xfonts++;
		}
	}
	else {
		if(!(dc.font.xfont = XLoadQueryFont(dpy, fontstr))
		&& !(dc.font.xfont = XLoadQueryFont(dpy, "fixed")))
			die("error, cannot load font: '%s'\n", fontstr);
		dc.font.ascent = dc.font.xfont->ascent;
		dc.font.descent = dc.font.xfont->descent;
	}
	dc.font.height = dc.font.ascent + dc.font.descent;
}

Bool
isprotodel(Client *c) {
	int i, n;
	Atom *protocols;
	Bool ret = False;

	if(XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		for(i = 0; !ret && i < n; i++)
			if(protocols[i] == wmatom[WMDelete])
				ret = True;
		XFree(protocols);
	}
	return ret;
}

#ifdef XINERAMA
static Bool
isuniquegeom(XineramaScreenInfo *unique, size_t len, XineramaScreenInfo *info) {
	unsigned int i;

	for(i = 0; i < len; i++)
		if(unique[i].x_org == info->x_org && unique[i].y_org == info->y_org
		&& unique[i].width == info->width && unique[i].height == info->height)
			return False;
	return True;
}
#endif /* XINERAMA */

void
keypress(XEvent *e) {
	unsigned int i;
	KeySym keysym;
	XKeyEvent *ev;

	ev = &e->xkey;
	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for(i = 0; i < LENGTH(keys); i++)
		if(keysym == keys[i].keysym
		&& CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		&& keys[i].func)
			keys[i].func(&(keys[i].arg));
}

void
killclient( const Arg *arg ) {
	XEvent ev;

	if ( SELVIEW( selmon ).sel ) {
		if ( isprotodel( SELVIEW( selmon ).sel ) ) {
			ev.type = ClientMessage;
			ev.xclient.window = SELVIEW( selmon ).sel->win;
			ev.xclient.message_type = wmatom[ WMProtocols ];
			ev.xclient.format = 32;
			ev.xclient.data.l[ 0 ] = wmatom[ WMDelete ];
			ev.xclient.data.l[ 1 ] = CurrentTime;
			XSendEvent( dpy, SELVIEW( selmon ).sel->win, False, NoEventMask, &ev );
		}
		else {
			XGrabServer( dpy );
			XSetErrorHandler( xerrordummy );
			XSetCloseDownMode( dpy, DestroyAll );
			XKillClient( dpy, SELVIEW( selmon ).sel->win );
			XSync( dpy, False );
			XSetErrorHandler( xerror );
			XUngrabServer( dpy );
		}
	}
}

void
manage( Window w, XWindowAttributes *wa ) {
	static Client cz;
	Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	if ( !( c = malloc( sizeof( Client ) ) ) )
		die( "fatal: could not malloc() %u bytes\n", sizeof( Client ) );
	*c = cz;
	c->win = w;
	updatetitle( c );
	if ( XGetTransientForHint( dpy, w, &trans ) )
		t = wintoclient( trans );
	c->mon = t ? t->mon : selmon;
	c->view = c->mon->selview;
	/* geometry */
	c->x = c->oldx = wa->x + c->mon->wx;
	c->y = c->oldy = wa->y + c->mon->wy;
	c->w = c->oldw = wa->width;
	c->h = c->oldh = wa->height;
	c->oldbw = wa->border_width;
	if ( c->w == c->mon->mw && c->h == c->mon->mh ) {
		c->isfloating = 1;
		c->x = c->mon->mx;
		c->y = c->mon->my;
		c->bw = 0;
	}
	else {
		if ( c->x + WIDTH( c ) > c->mon->mx + c->mon->mw )
			c->x = c->mon->mx + c->mon->mw - WIDTH( c );
		if ( c->y + HEIGHT( c ) > c->mon->my + c->mon->mh )
			c->y = c->mon->my + c->mon->mh - HEIGHT( c );
		c->x = MAX( c->x, c->mon->mx );
		/* only fix client y-offset, if the client center might cover the bar */
		c->y = MAX( c->y, ( ( c->mon->by == 0 ) && ( c->x + ( c->w / 2 ) >= c->mon->wx )
			&& ( c->x + ( c->w / 2 ) < c->mon->wx + c->mon->ww ) ) ? bh : c->mon->my );
		c->bw = borderpx;
	}
	wc.border_width = c->bw;
	XConfigureWindow( dpy, w, CWBorderWidth, &wc );
	XSetWindowBorder( dpy, w, dc.norm[ ColBorder ] );
	configure( c ); /* propagates border_width, if size doesn't change */
	updatesizehints( c );
	XSelectInput( dpy, w, EnterWindowMask | FocusChangeMask | PropertyChangeMask | StructureNotifyMask );
	grabbuttons( c, False );
	if ( !c->isfloating )
		c->isfloating = c->oldstate = trans != None || c->isfixed;
	if ( c->isfloating )
		XRaiseWindow( dpy, c->win );
	attach( c );
	attachstack( c );
	XMoveResizeWindow( dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h ); /* some windows require this */
	XMapWindow( dpy, c->win );
	setclientstate( c, NormalState );
	arrange( c->mon );
}

void
mappingnotify(XEvent *e) {
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if(ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e) {
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if(!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if(wa.override_redirect)
		return;
	if(!wintoclient(ev->window))
		manage(ev->window, &wa);
}

void
mirrortile( Monitor *const m ) {
	int x, y, h, w, rw, mh;
	unsigned int i, n;
	Client *c;

	for ( n = 0, c = nexttiled( SELVIEW( m ).clients ) ; c ; c = nexttiled( c->next ), n++ );
	if ( n == 0 )
		return;

	// master

	c = nexttiled( SELVIEW( m ).clients );
	mh = SELVIEW( m ).mfact * m->wh;
	resize( c, m->wx, m->wy, m->ww - 2 * c->bw, ( n == 1 ? m->wh : mh ) - 2 * c->bw, False );
	if ( --n == 0 )
		return;

	// tile stack

	x = m->wx;
	y = ( m->wy + mh > c->y + c->h ) ? c->y + c->h + 2 * c->bw : m->wy + mh;
	w = m->ww / n;
	rw = m->ww % n;
	h = ( m->wy + mh > c->y + c->h ) ? m->wy + m->wh - y : m->wh - mh;

	for ( i = 0, c = nexttiled( c->next ) ; c ; c = nexttiled( c->next ), i++, rw-- ) {
		resize( c, x, y,
			( ( ( i + 1 == n ) ?  m->wx + m->mw - x - 2 * c->bw : w - 2 * c->bw ) + ( ( 0 < rw ) ? 1 : 0 ) ),
			h - 2 * c->bw, False );
		if ( w != m->ww )
			x = c->x + WIDTH( c );
	}
}

void
monocle( Monitor *const m ) {
	unsigned int n = 0;
	Client *c;

	for ( c = SELVIEW( m ).clients ; c ; c = c->next )
		n++;
	if ( n > 0 ) /* override layout symbol */
		snprintf( m->ltsymbol, sizeof m->ltsymbol, "[%d]", n );
	for ( c = nexttiled( SELVIEW( m ).clients ) ; c ; c = nexttiled( c->next ) )
		resize( c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, False );
}

void
movemouse( const Arg *arg ) {
	// TODO: Refactor me.
	int x, y, ocx, ocy, nx, ny;
	Client *const c = SELVIEW( selmon ).sel;
	Monitor *m;
	XEvent ev;

	if ( !c )
		return;

	restack( selmon );
	ocx = c->x;
	ocy = c->y;

	if ( !( XGrabPointer( dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[ CurMove ], CurrentTime ) == GrabSuccess ) )
		return;
	if ( !getrootptr( &x, &y ) )
		return;
	do {
		XMaskEvent( dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev );

		switch ( ev.type ) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ ev.type ]( &ev );
			break;

		case MotionNotify:
			nx = ocx + ( ev.xmotion.x - x );
			ny = ocy + ( ev.xmotion.y - y );
			if ( snap
				&& ( selmon->wx <= nx && nx <= selmon->wx + selmon->ww )
				&& ( selmon->wy <= ny && ny <= selmon->wy + selmon->wh ) ) {
				if ( abs( selmon->wx - nx ) < snap )
					nx = selmon->wx;
				else if ( abs( ( selmon->wx + selmon->ww ) - ( nx + WIDTH( c ) ) ) < snap )
					nx = selmon->wx + selmon->ww - WIDTH( c );

				if ( abs( selmon->wy - ny ) < snap )
					ny = selmon->wy;
				else if ( abs( ( selmon->wy + selmon->wh ) - ( ny + HEIGHT( c ) ) ) < snap )
					ny = selmon->wy + selmon->wh - HEIGHT( c );

				if ( ( !c->isfloating && SELVIEW( selmon ).lt->arrange )
					&& ( snap < abs( nx - c->x ) || snap < abs( ny - c->y ) ) )
					togglefloating( NULL );
			}
			if ( !( !c->isfloating && SELVIEW( selmon ).lt->arrange ) )
				resize( c, nx, ny, c->w, c->h, True );
			break;
		}
	} while ( ev.type != ButtonRelease );
	XUngrabPointer( dpy, CurrentTime );

	if ( ( m = ptrtomon( c->x + c->w / 2, c->y + c->h / 2 ) ) != selmon ) {
		sendmon( c, m );
		selmon = m;
		focus( NULL );
	}
}

Client *
nexttiled( Client *c ) {
	for ( ; c && c->isfloating ; c = c->next );
	return c;
}

Monitor *
ptrtomon(int x, int y) {
	Monitor *m;

	for(m = mons; m; m = m->next)
		if(INRECT(x, y, m->wx, m->wy, m->ww, m->wh))
			return m;
	return selmon;
}

void
propertynotify( XEvent *e ) {
	Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ( ( ev->window == root ) && ( ev->atom == XA_WM_NAME ) )
		updatestatus();
	else if ( ev->state == PropertyDelete )
		return; /* ignore */
	else if ( ( c = wintoclient( ev->window ) ) ) {
		switch ( ev->atom ) {
		case XA_WM_TRANSIENT_FOR:
			XGetTransientForHint( dpy, c->win, &trans );
			if ( !c->isfloating && ( c->isfloating = ( wintoclient( trans ) != NULL ) ) )
				arrange( c->mon );
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints( c );
			break;
		case XA_WM_HINTS:
			updatewmhints( c );
			drawbars();
			break;
		default:
			break;
		}
		if ( ev->atom == XA_WM_NAME || ev->atom == netatom[ NetWMName ] ) {
			updatetitle( c );
			if ( c == c->mon->views[ c->view ].sel )
				drawbar( c->mon );
		}
	}
}

void
clientmessage(XEvent *e) {
	XClientMessageEvent *cme = &e->xclient;
	Client *c;

	if((c = wintoclient(cme->window))
	&& (cme->message_type == netatom[NetWMState] && cme->data.l[1] == netatom[NetWMFullscreen]))
	{
		if(cme->data.l[0]) {
			XChangeProperty(dpy, cme->window, netatom[NetWMState], XA_ATOM, 32,
			                PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
			c->oldstate = c->isfloating;
			c->oldbw = c->bw;
			c->bw = 0;
			c->isfloating = 1;
			resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
			XRaiseWindow(dpy, c->win);
		}
		else {
			XChangeProperty(dpy, cme->window, netatom[NetWMState], XA_ATOM, 32,
			                PropModeReplace, (unsigned char*)0, 0);
			c->isfloating = c->oldstate;
			c->bw = c->oldbw;
			c->x = c->oldx;
			c->y = c->oldy;
			c->w = c->oldw;
			c->h = c->oldh;
			resizeclient(c, c->x, c->y, c->w, c->h);
			arrange(c->mon);
		}
	}
}

void
quit(const Arg *arg) {
	running = False;
}

void
resize(Client *c, int x, int y, int w, int h, Bool interact) {
	if(applysizehints(c, &x, &y, &w, &h, interact))
		resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h) {
	XWindowChanges wc;

	c->oldx = c->x; c->x = wc.x = x;
	c->oldy = c->y; c->y = wc.y = y;
	c->oldw = c->w; c->w = wc.width = w;
	c->oldh = c->h; c->h = wc.height = h;
	wc.border_width = c->bw;
	XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
	configure(c);
	XSync(dpy, False);
}

void
resizemouse( const Arg *arg ) {
	int ocx, ocy, nw, nh;
	Client *const c = SELVIEW( selmon ).sel;
	Monitor *m;
	XEvent ev;

	if ( !c )
		return;

	restack( selmon );
	ocx = c->x;
	ocy = c->y;

	if ( !( XGrabPointer( dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, cursor[ CurResize ], CurrentTime ) == GrabSuccess ) )
		return;
	XWarpPointer( dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1 );
	do {
		XMaskEvent( dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev );

		switch ( ev.type ) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ ev.type ]( &ev );
			break;

		case MotionNotify:
			nw = MAX( ev.xmotion.x - ocx - 2 * c->bw + 1, 1 );
			nh = MAX( ev.xmotion.y - ocy - 2 * c->bw + 1, 1 );
			if ( snap
				&& ( selmon->wx <= nw && nw <= selmon->wx + selmon->ww )
				&& ( selmon->wy <= nh && nh <= selmon->wy + selmon->wh ) ) {
				if ( ( !c->isfloating && SELVIEW( selmon ).lt->arrange )
					&& ( abs( nw - c->w ) > snap || abs( nh - c->h ) > snap ) )
					togglefloating( NULL );
			}
			if ( !c->isfloating && SELVIEW( selmon ).lt->arrange )
				resize( c, c->x, c->y, nw, nh, True );
			break;
		}
	} while ( ev.type != ButtonRelease );
	XWarpPointer( dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1 );
	XUngrabPointer( dpy, CurrentTime );

	while ( XCheckMaskEvent( dpy, EnterWindowMask, &ev ) );
	if ( ( m = ptrtomon( c->x + c->w / 2, c->y + c->h / 2 ) ) != selmon ) {
		sendmon( c, m );
		selmon = m;
		focus( NULL );
	}
}

void
restack( Monitor *const m ) {
	Client *c;
	XEvent ev;
	XWindowChanges wc;

	drawbar( m );
	if ( !SELVIEW( m ).sel )
		return;
	if ( SELVIEW( m ).sel->isfloating || !SELVIEW( m ).lt->arrange )
		XRaiseWindow( dpy, SELVIEW( m ).sel->win );
	if ( SELVIEW( m ).lt->arrange ) {
		wc.stack_mode = Below;
		wc.sibling = m->barwin;
		for ( c = SELVIEW( m ).stack ; c ; c = c->snext )
			if ( !c->isfloating ) {
				XConfigureWindow( dpy, c->win, CWSibling | CWStackMode, &wc );
				wc.sibling = c->win;
			}
	}
	XSync( dpy, False );
	while ( XCheckMaskEvent( dpy, EnterWindowMask, &ev ) );
}

void
run(void) {
	XEvent ev;
	/* main event loop */
	XSync(dpy, False);
	while(running && !XNextEvent(dpy, &ev)) {
		if(handler[ev.type])
			handler[ev.type](&ev); /* call handler */
	}
}

void
scan(void) {
	unsigned int i, num;
	Window d1, d2, *wins = NULL;
	XWindowAttributes wa;

	if(XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for(i = 0; i < num; i++) {
			if(!XGetWindowAttributes(dpy, wins[i], &wa)
			|| wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if(wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for(i = 0; i < num; i++) { /* now the transients */
			if(!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if(XGetTransientForHint(dpy, wins[i], &d1)
			&& (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if(wins)
			XFree(wins);
	}
}

void
sendmon( Client *c, Monitor *m ) {
	if ( c->mon != m ) {
		unfocus( c, True );
		detach( c );
		detachstack( c );
		c->mon = m;
		c->view = m->selview;
		attach( c );
		attachstack( c );
		focus( NULL );
		arrange( NULL );
	}
}

void
setclientstate(Client *c, long state) {
	long data[] = { state, None };

	XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
			PropModeReplace, (unsigned char *)data, 2);
}

void
setlayout( const Arg *arg ) {
	SELVIEW( selmon ).lt = ( Layout * ) arg->v;
	strncpy( selmon->ltsymbol, SELVIEW( selmon ).lt->symbol, sizeof( selmon->ltsymbol ) );
	if ( SELVIEW( selmon ).sel )
		arrange( selmon );
	else
		drawbar( selmon );
}

void
setmfact( const Arg *arg ) {
	float f;

	if ( arg && -1.0 < arg->f && arg->f < 1.0 && SELVIEW( selmon ).lt->arrange ) {
		f = arg->f + SELVIEW( selmon ).mfact;
		if ( 0.1 <= f && f <= 0.9 ) {
			SELVIEW( selmon ).mfact = f;
			arrange( selmon );
		}
	}
}

void
setup(void) {
	XSetWindowAttributes wa;

	/* clean up any zombies immediately */
	sigchld(0);

	/* init screen */
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	initfont(font);
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	bh = dc.h = dc.font.height + 2;
	updategeom();
	/* init atoms */
	wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
	netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
	netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
	netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
	/* init cursors */
	cursor[CurNormal] = XCreateFontCursor(dpy, XC_left_ptr);
	cursor[CurResize] = XCreateFontCursor(dpy, XC_sizing);
	cursor[CurMove] = XCreateFontCursor(dpy, XC_fleur);
	/* init appearance */
	dc.norm[ColBorder] = getcolor(normbordercolor);
	dc.norm[ColBG] = getcolor(normbgcolor);
	dc.norm[ColFG] = getcolor(normfgcolor);
	dc.sel[ColBorder] = getcolor(selbordercolor);
	dc.sel[ColBG] = getcolor(selbgcolor);
	dc.sel[ColFG] = getcolor(selfgcolor);
	dc.drawable = XCreatePixmap(dpy, root, DisplayWidth(dpy, screen), bh, DefaultDepth(dpy, screen));
	dc.gc = XCreateGC(dpy, root, 0, NULL);
	XSetLineAttributes(dpy, dc.gc, 1, LineSolid, CapButt, JoinMiter);
	if(!dc.font.set)
		XSetFont(dpy, dc.gc, dc.font.xfont->fid);
	/* init bars */
	updatebars();
	updatestatus();
	/* EWMH support per view */
	XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
			PropModeReplace, (unsigned char *) netatom, NetLast);
	/* select for events */
	wa.cursor = cursor[CurNormal];
	wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask|ButtonPressMask
	                |EnterWindowMask|LeaveWindowMask|StructureNotifyMask
	                |PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);
	grabkeys();
}

void
showhide( Client *c ) {
	if ( !c )
		return;
	XMoveWindow( dpy, c->win, c->x, c->y );
	if ( !( !c->isfloating && c->mon->views[ c->view ].lt->arrange ) )
		resize( c, c->x, c->y, c->w, c->h, False );
	showhide( c->snext );
}


void
sigchld(int unused) {
	if(signal(SIGCHLD, sigchld) == SIG_ERR)
		die("Can't install SIGCHLD handler");
	while(0 < waitpid(-1, NULL, WNOHANG));
}

void
spawn(const Arg *arg) {
	if(fork() == 0) {
		if(dpy)
			close(ConnectionNumber(dpy));
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
		perror(" failed");
		exit(0);
	}
}

void
tag(const Arg *arg) {
	Client *const c = SELVIEW( selmon ).sel;

	if ( c ) {
		detach( c );
		detachstack( c );
		c->view = arg->ui;
		attach( c );
		attachstack( c );
		arrange( selmon );
	}
}

void
tagmon( const Arg *arg ) {
	if ( SELVIEW( selmon ).sel ) {
		sendmon( SELVIEW( selmon ).sel, dirtomon( arg->i ) );
	}
}

int
textnw(const char *text, unsigned int len) {
	XRectangle r;

	if(dc.font.set) {
		XmbTextExtents(dc.font.set, text, len, NULL, &r);
		return r.width;
	}
	return XTextWidth(dc.font.xfont, text, len);
}

void
tile( Monitor *const m ) {
	int x, y, h, rh, w, mw;
	unsigned int i, n;
	Client *c;

	for ( n = 0, c = nexttiled( SELVIEW( m ).clients ) ; c ; c = nexttiled( c->next ), n++ );
	if ( n == 0 )
		return;

	// master

	c = nexttiled( SELVIEW( m ).clients );
	mw = SELVIEW( m ).mfact * m->ww;
	resize( c, m->wx, m->wy, ( n == 1 ? m->ww : mw ) - 2 * c->bw, m->wh - 2 * c->bw, False );
	if ( --n == 0 )
		return;

	// tile stack

	x = ( m->wx + mw > c->x + c->w ) ? c->x + c->w + 2 * c->bw : m->wx + mw;
	y = m->wy;
	w = ( m->wx + mw > c->x + c->w ) ? m->wx + m->ww - x : m->ww - mw;
	h = m->wh / n;
	rh = m->wh % n;
	if ( h < bh ) {
		h = m->wh;
		rh = 0;
	}
	for (i = 0, c = nexttiled( c->next ) ; c ; c = nexttiled( c->next ), i++, rh-- ) {
		resize( c, x, y, w - 2 * c->bw,
			( ( ( i + 1 == n ) ? m->wy + m->wh - y - 2 * c->bw : h - 2 * c->bw ) + ( ( 0 < rh ) ? 1 : 0 ) ),
			False);
		if ( h != m->wh )
			y = c->y + HEIGHT( c );
	}
}

void
togglebar(const Arg *arg) {
	selmon->showbar = !selmon->showbar;
	updatebarpos(selmon);
	XMoveResizeWindow(dpy, selmon->barwin, selmon->wx, selmon->by, selmon->ww, bh);
	arrange(selmon);
}

void
togglefloating( const Arg *arg ) {
	Client *const c = SELVIEW( selmon ).sel;

	if ( c ) {
		c->isfloating = !c->isfloating || c->isfixed;
		if ( c->isfloating )
			resize( c, c->x, c->y, c->w, c->h, False );
		arrange( selmon );
	}
}

void
unfocus(Client *c, Bool setfocus) {
	if(!c)
		return;
	grabbuttons(c, False);
	XSetWindowBorder(dpy, c->win, dc.norm[ColBorder]);
	if(setfocus)
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
}

void
unmanage( Client *c, Bool destroyed ) {
	Monitor *m = c->mon;
	XWindowChanges wc;

	/* The server grab construct avoids race conditions. */
	detach( c );
	detachstack( c );
	if ( !destroyed ) {
		wc.border_width = c->oldbw;
		XGrabServer( dpy );
		XSetErrorHandler( xerrordummy );
		XConfigureWindow( dpy, c->win, CWBorderWidth, &wc ); /* restore border */
		XUngrabButton( dpy, AnyButton, AnyModifier, c->win );
		setclientstate( c, WithdrawnState );
		XSync( dpy, False );
		XSetErrorHandler( xerror );
		XUngrabServer( dpy );
	}
	free( c );
	focus( NULL );
	arrange( m );
}

void
unmapnotify(XEvent *e) {
	Client *c;
	XUnmapEvent *ev = &e->xunmap;

	if((c = wintoclient(ev->window)))
		unmanage(c, False);
}

void
updatebars(void) {
	Monitor *m;
	XSetWindowAttributes wa;

	wa.override_redirect = True;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ButtonPressMask|ExposureMask;
	for(m = mons; m; m = m->next) {
		m->barwin = XCreateWindow(dpy, root, m->wx, m->by, m->ww, bh, 0, DefaultDepth(dpy, screen),
		                          CopyFromParent, DefaultVisual(dpy, screen),
		                          CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
		XDefineCursor(dpy, m->barwin, cursor[CurNormal]);
		XMapRaised(dpy, m->barwin);
	}
}

void
updatebarpos(Monitor *m) {
	m->wy = m->my;
	m->wh = m->mh;
	if(m->showbar) {
		m->wh -= bh;
		m->by = m->topbar ? m->wy : m->wy + m->wh;
		m->wy = m->topbar ? m->wy + bh : m->wy;
	}
	else
		m->by = -bh;
}

Bool
updategeom(void) {
	Bool dirty = False;

#ifdef XINERAMA
	if ( XineramaIsActive( dpy ) ) {
		int i, j, n, nn;
		Client *c;
		Monitor *m;
		XineramaScreenInfo *info = XineramaQueryScreens( dpy, &nn );
		XineramaScreenInfo *unique = NULL;

		info = XineramaQueryScreens( dpy, &nn );
		for ( n = 0, m = mons ; m ; m = m->next, n++ );
		/* only consider unique geometries as separate screens */
		if ( !( unique = ( XineramaScreenInfo * ) malloc( sizeof( XineramaScreenInfo ) * nn ) ) )
			die( "fatal: could not malloc() %u bytes\n", sizeof( XineramaScreenInfo ) * nn );
		for ( i = 0, j = 0 ; i < nn ; i++ )
			if ( isuniquegeom( unique, j, &info[ i ] ) )
				memcpy( &unique[ j++ ], &info[ i ], sizeof( XineramaScreenInfo ) );
		XFree( info );
		nn = j;
		if ( n <= nn ) {
			for ( i = 0 ; i < ( nn - n ) ; i++ ) { /* new monitors available */
				for ( m = mons ; m && m->next ; m = m->next );
				if ( m )
					m->next = createmon();
				else
					mons = createmon();
			}
			for ( i = 0, m = mons ; i < nn && m ; m = m->next, i++ )
				if ( n <= i
					|| ( unique[ i ].x_org != m->mx || unique[ i ].y_org != m->my
				    || unique[ i ].width != m->mw || unique[ i ].height != m->mh ) ) {
					dirty = True;
					m->num = i;
					m->mx = m->wx = unique[ i ].x_org;
					m->my = m->wy = unique[ i ].y_org;
					m->mw = m->ww = unique[ i ].width;
					m->mh = m->wh = unique[ i ].height;
					updatebarpos( m );
				}
		}
		else { /* less monitors available nn < n */
			for ( i = nn ; i < n ; i++ ) {
				for ( m = mons ; m && m->next ; m = m->next );
				for ( j = 0 ; j < NUMVIEWS ; j++ ) {
					while ( m->views[ j ].clients ) {
						dirty = True;
						c = m->views[ j ].clients;
						detach( c );
						detachstack( c );
						c->mon = mons;
						attach( c );
						attachstack( c );
					}
				}
				if ( m == selmon )
					selmon = mons;
				cleanupmon( m );
			}
		}
		free( unique );
	}
	else
#endif /* XINERAMA */
	/* default monitor setup */
	{
		if ( !mons )
			mons = createmon();
		if ( mons->mw != sw || mons->mh != sh ) {
			dirty = True;
			mons->mw = mons->ww = sw;
			mons->mh = mons->wh = sh;
			updatebarpos( mons );
		}
	}
	if ( dirty ) {
		selmon = mons;
		selmon = wintomon( root );
	}
	return dirty;
}

void
updatenumlockmask(void) {
	unsigned int i, j;
	XModifierKeymap *modmap;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for(i = 0; i < 8; i++)
		for(j = 0; j < modmap->max_keypermod; j++)
			if(modmap->modifiermap[i * modmap->max_keypermod + j]
			   == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c) {
	long msize;
	XSizeHints size;

	if(!XGetWMNormalHints(dpy, c->win, &size, &msize))
		/* size is uninitialized, ensure that size.flags aren't used */
		size.flags = PSize;
	if(size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	}
	else if(size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	}
	else
		c->basew = c->baseh = 0;
	if(size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	}
	else
		c->incw = c->inch = 0;
	if(size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	}
	else
		c->maxw = c->maxh = 0;
	if(size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	}
	else if(size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	}
	else
		c->minw = c->minh = 0;
	if(size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
	}
	else
		c->maxa = c->mina = 0.0;
	c->isfixed = (c->maxw && c->minw && c->maxh && c->minh
	             && c->maxw == c->minw && c->maxh == c->minh);
}

void
updatetitle(Client *c) {
	if(!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
	if(c->name[0] == '\0') /* hack to mark broken clients */
		strcpy(c->name, broken);
}

void
updatestatus(void) {
	if ( !gettextprop( root, XA_WM_NAME, stext, sizeof( stext ) ) )
		strcpy( stext, "myDWM-"VERSION );
	drawbar( selmon );
}

void
updatewmhints( Client *c ) {
	XWMHints *wmh;

	if ( ( wmh = XGetWMHints( dpy, c->win ) ) ) {
		if ( c == SELVIEW( selmon ).sel && wmh->flags & XUrgencyHint ) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints( dpy, c->win, wmh );
		}
		else
			c->isurgent = ( wmh->flags & XUrgencyHint ) ? True : False;
		XFree( wmh );
	}
}

void
view(const Arg *arg) {
	if ( arg->ui != selmon->selview ) {
		selmon->selview = arg->ui;
		arrange( selmon );
	}
}

Client *
wintoclient( Window w ) {
	Client *c;
	const Monitor *m;
	int i;

	for ( m = mons ; m ; m = m->next )
		for ( i = 0 ; i < NUMVIEWS ; i++ )
			for ( c = m->views[ i ].clients ; c ; c = c->next )
				if ( w == c->win )
					return c;
	return NULL;
}

Monitor *
wintomon(Window w) {
	int x, y;
	Client *c;
	Monitor *m;

	if(w == root && getrootptr(&x, &y))
		return ptrtomon(x, y);
	for(m = mons; m; m = m->next)
		if(w == m->barwin)
			return m;
	if((c = wintoclient(w)))
		return c->mon;
	return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's).  Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int
xerror(Display *dpy, XErrorEvent *ee) {
	if(ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
			ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee) {
	return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee) {
	otherwm = True;
	return -1;
}

void
zoom( const Arg *arg ) {
	Client *c;

	if ( ( SELVIEW( selmon ).lt->arrange && SELVIEW( selmon ).lt->arrange != monocle )
		&& !( SELVIEW( selmon ).sel && SELVIEW( selmon ).sel->isfloating ) ) {
		c = SELVIEW( selmon ).sel;
		if ( c && c == nexttiled( SELVIEW( selmon ).clients ) ) {
			c = nexttiled( c->next );
			if ( !c )
				return;
		}
		detach( c );
		attach( c );
		focus( c );
		arrange( c->mon );
	}
}

int
main(int argc, char *argv[]) {
	if(argc == 2 && !strcmp("-v", argv[1]))
		die("dwm-"VERSION", © 2006-2010 dwm engineers, see LICENSE for details\n");
	else if(argc != 1)
		die("usage: dwm [-v]\n");
	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if(!(dpy = XOpenDisplay(NULL)))
		die("dwm: cannot open display\n");
	checkotherwm();
	setup();
	scan();
	run();
	cleanup();
	XCloseDisplay(dpy);
	return 0;
}
