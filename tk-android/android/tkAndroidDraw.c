/*
 * tkAndroidDraw.c --
 *
 *	A small software rasteriser behind Tk's Xlib-shaped drawing API:
 *	rectangles, lines (width, dashes, caps), polygons, arcs, area copies,
 *	images, stipples, clip regions/masks, and text blits. Everything draws
 *	into pixel buffers: the root's screen buffer (all windows) or a pixmap.
 *
 *	No anti-aliasing for shapes (Tk's classic widgets are drawn aliased);
 *	text is anti-aliased by the text engine and alpha-blended here.
 */

#include "tkAndroidPriv.h"
#include <math.h>

/* ---- dirty tracking -------------------------------------------------------- */

void
TkAMarkDirty(TkARect r)
{
    if (r.x1 >= r.x2 || r.y1 >= r.y2) {
	return;
    }
    if (!tkA.dirty) {
	tkA.dirtyRect = r;
	tkA.dirty = 1;
	return;
    }
    if (r.x1 < tkA.dirtyRect.x1) tkA.dirtyRect.x1 = r.x1;
    if (r.y1 < tkA.dirtyRect.y1) tkA.dirtyRect.y1 = r.y1;
    if (r.x2 > tkA.dirtyRect.x2) tkA.dirtyRect.x2 = r.x2;
    if (r.y2 > tkA.dirtyRect.y2) tkA.dirtyRect.y2 = r.y2;
}

/* ---- drawing target ------------------------------------------------------------- */

typedef struct Target {
    uint32_t *pix;
    int stride, w, h;
    int ox, oy;			/* drawable origin in storage coordinates */
    TkARegion *clip;		/* storage coordinates; owned */
    int depth;
    int isRoot;
    unsigned long fg, bg;
    int fillStyle;
    TkADrawable *pat;		/* stipple or tile */
    int tsx, tsy;		/* pattern origin, storage coordinates */
    TkADrawable *maskPix;	/* clip mask bitmap */
    int mx, my;
    int lineWidth, lineStyle, capStyle, joinStyle, dashLen;
    int fillRule, arcMode;
} Target;

static void
FreeTarget(Target *t)
{
    TkARegionFree(t->clip);
    t->clip = NULL;
}

static int
Resolve(Drawable drawable, GC gc, Target *t)
{
    TkADrawable *d = TKA_DRAWABLE(drawable);
    TkpClipMask *cm;

    memset(t, 0, sizeof(*t));
    if (d == NULL) {
	return 0;
    }
    t->clip = TkARegionNew();
    if (d->isWindow) {
	if (!TkAIsViewable(d)) {
	    FreeTarget(t);
	    return 0;
	}
	t->pix = tkA.root->pix;
	t->stride = tkA.root->w;
	t->w = tkA.root->w;
	t->h = tkA.root->h;
	TkAWindowOrigin(d, &t->ox, &t->oy);
	TkAComputeClip(d, t->clip);
	t->depth = 24;
	t->isRoot = 1;
    } else {
	t->pix = d->pix;
	t->stride = d->w;
	t->w = d->w;
	t->h = d->h;
	t->depth = d->depth;
	TkARegionAddRect(t->clip, (TkARect) {0, 0, d->w, d->h});
    }
    if (gc == NULL) {
	return 1;
    }
    t->fg = gc->foreground;
    t->bg = gc->background;
    t->fillStyle = gc->fill_style;
    t->lineWidth = gc->line_width;
    t->lineStyle = gc->line_style;
    t->capStyle = gc->cap_style;
    t->joinStyle = gc->join_style;
    t->dashLen = gc->dashes ? gc->dashes : 4;
    t->fillRule = gc->fill_rule;
    t->arcMode = gc->arc_mode;
    if (gc->fill_style == FillStippled || gc->fill_style == FillOpaqueStippled) {
	t->pat = TKA_DRAWABLE(gc->stipple);
    } else if (gc->fill_style == FillTiled) {
	t->pat = TKA_DRAWABLE(gc->tile);
    }
    if (t->fillStyle != FillSolid && t->pat == NULL) {
	t->fillStyle = FillSolid;
    }
    t->tsx = t->ox + gc->ts_x_origin;
    t->tsy = t->oy + gc->ts_y_origin;
    cm = (TkpClipMask *) gc->clip_mask;
    if (cm != NULL) {
	if (cm->type == TKP_CLIP_REGION && cm->value.region != NULL) {
	    TkARegion *cr = (TkARegion *) cm->value.region;
	    TkARegion *tmp = TkARegionNew();
	    int i;

	    /* the GC's region, translated to storage coordinates */
	    for (i = 0; i < cr->n; i++) {
		TkARect r = cr->r[i];

		r.x1 += t->ox + gc->clip_x_origin;
		r.x2 += t->ox + gc->clip_x_origin;
		r.y1 += t->oy + gc->clip_y_origin;
		r.y2 += t->oy + gc->clip_y_origin;
		TkARegionAddRect(tmp, r);
	    }
	    {
		TkARegion *res = TkARegionNew();
		int a, b;

		for (a = 0; a < t->clip->n; a++) {
		    for (b = 0; b < tmp->n; b++) {
			TkARect x = t->clip->r[a];

			if (x.x1 < tmp->r[b].x1) x.x1 = tmp->r[b].x1;
			if (x.y1 < tmp->r[b].y1) x.y1 = tmp->r[b].y1;
			if (x.x2 > tmp->r[b].x2) x.x2 = tmp->r[b].x2;
			if (x.y2 > tmp->r[b].y2) x.y2 = tmp->r[b].y2;
			TkARegionAddRect(res, x);
		    }
		}
		TkARegionFree(t->clip);
		t->clip = res;
	    }
	    TkARegionFree(tmp);
	} else if (cm->type == TKP_CLIP_PIXMAP && cm->value.pixmap != None) {
	    t->maskPix = TKA_DRAWABLE(cm->value.pixmap);
	    t->mx = t->ox + gc->clip_x_origin;
	    t->my = t->oy + gc->clip_y_origin;
	}
    }
    return 1;
}

/* Colour to store for Tk pixel value v. */
static uint32_t
Store(Target *t, unsigned long v)
{
    return t->depth == 1 ? (uint32_t) (v & 1) : (TKA_OPAQUE | (uint32_t) (v & 0xFFFFFF));
}

static int
PatBit(TkADrawable *p, int x, int y)
{
    int px = x % p->w, py = y % p->h;

    if (px < 0) px += p->w;
    if (py < 0) py += p->h;
    return p->pix[py * p->w + px] != 0;
}

/* Colour for the pixel at storage coords (x, y), or 0 if it is not drawn. */
static int
ColorAt(Target *t, int x, int y, uint32_t *out)
{
    if (t->maskPix) {
	int mx = x - t->mx, my = y - t->my;

	if (mx < 0 || my < 0 || mx >= t->maskPix->w || my >= t->maskPix->h
		|| t->maskPix->pix[my * t->maskPix->w + mx] == 0) {
	    return 0;
	}
    }
    switch (t->fillStyle) {
    case FillStippled:
	if (!PatBit(t->pat, x - t->tsx, y - t->tsy)) return 0;
	*out = Store(t, t->fg);
	return 1;
    case FillOpaqueStippled:
	*out = Store(t, PatBit(t->pat, x - t->tsx, y - t->tsy) ? t->fg : t->bg);
	return 1;
    case FillTiled: {
	int px = (x - t->tsx) % t->pat->w, py = (y - t->tsy) % t->pat->h;

	if (px < 0) px += t->pat->w;
	if (py < 0) py += t->pat->h;
	*out = t->pat->pix[py * t->pat->w + px];
	return 1;
    }
    default:
	*out = Store(t, t->fg);
	return 1;
    }
}

static int
Simple(Target *t)
{
    return t->fillStyle == FillSolid && t->maskPix == NULL;
}

/* Fill the span [x1, x2) on row y (storage coordinates). */
static void
Span(Target *t, int y, int x1, int x2)
{
    int i;

    if (y < 0 || y >= t->h) {
	return;
    }
    for (i = 0; i < t->clip->n; i++) {
	const TkARect *r = &t->clip->r[i];
	int lo, hi, x;

	if (y < r->y1 || y >= r->y2) {
	    continue;
	}
	lo = x1 > r->x1 ? x1 : r->x1;
	hi = x2 < r->x2 ? x2 : r->x2;
	if (lo < 0) lo = 0;
	if (hi > t->w) hi = t->w;
	if (lo >= hi) {
	    continue;
	}
	if (Simple(t)) {
	    uint32_t c = Store(t, t->fg), *p = t->pix + (size_t) y * t->stride;

	    for (x = lo; x < hi; x++) {
		p[x] = c;
	    }
	} else {
	    uint32_t c;

	    for (x = lo; x < hi; x++) {
		if (ColorAt(t, x, y, &c)) {
		    t->pix[(size_t) y * t->stride + x] = c;
		}
	    }
	}
	if (t->isRoot) {
	    TkAMarkDirty((TkARect) {lo, y, hi, y + 1});
	}
    }
}

static void
Plot(Target *t, int x, int y)
{
    Span(t, y, x, x + 1);
}

static void
FillRectT(Target *t, int x, int y, int w, int h)
{
    int j;

    for (j = 0; j < h; j++) {
	Span(t, y + j, x, x + w);
    }
}

/* Fill an area of the root with a solid pixel, clipped to `clip` (root coords). */
void
TkAFillClipped(const TkARegion *clip, TkARect r, uint32_t pixel)
{
    Target t;

    memset(&t, 0, sizeof(t));
    t.pix = tkA.root->pix;
    t.stride = t.w = tkA.root->w;
    t.h = tkA.root->h;
    t.depth = 24;
    t.isRoot = 1;
    t.fg = pixel & 0xFFFFFF;
    t.clip = TkARegionNew();
    TkARegionCopy(t.clip, clip);
    TkARegionIntersectRect(t.clip, r);
    {
	int i;

	for (i = 0; i < t.clip->n; i++) {
	    FillRectT(&t, t.clip->r[i].x1, t.clip->r[i].y1,
		    t.clip->r[i].x2 - t.clip->r[i].x1,
		    t.clip->r[i].y2 - t.clip->r[i].y1);
	}
    }
    FreeTarget(&t);
}

/* ---- polygons ------------------------------------------------------------------------ */

typedef struct Pt { double x, y; } Pt;

static int
CmpD(const void *a, const void *b)
{
    double d = *(const double *) a - *(const double *) b;

    return d < 0 ? -1 : d > 0;
}

/* Even-odd fill of a polygon given in storage coordinates. */
static void
FillPoly(Target *t, const Pt *p, int n)
{
    double ymin = p[0].y, ymax = p[0].y, *xs;
    int i, y, y0, y1;

    if (n < 3) {
	return;
    }
    for (i = 1; i < n; i++) {
	if (p[i].y < ymin) ymin = p[i].y;
	if (p[i].y > ymax) ymax = p[i].y;
    }
    y0 = (int) ceil(ymin - 0.5);
    y1 = (int) ceil(ymax - 0.5);
    xs = (double *) malloc((size_t) n * sizeof(double));
    for (y = y0; y < y1; y++) {
	double cy = y + 0.5;
	int k = 0, j;

	for (i = 0; i < n; i++) {
	    const Pt *a = &p[i], *b = &p[(i + 1) % n];

	    if ((a->y <= cy && b->y > cy) || (b->y <= cy && a->y > cy)) {
		xs[k++] = a->x + (cy - a->y) * (b->x - a->x) / (b->y - a->y);
	    }
	}
	qsort(xs, (size_t) k, sizeof(double), CmpD);
	for (j = 0; j + 1 < k; j += 2) {
	    Span(t, y, (int) ceil(xs[j] - 0.5), (int) ceil(xs[j + 1] - 0.5));
	}
    }
    free(xs);
}

/* ---- lines ----------------------------------------------------------------------------- */

static void
Disc(Target *t, double cx, double cy, double r)
{
    Pt pts[40];
    int i, n = r < 2 ? 8 : r < 6 ? 16 : 32;

    for (i = 0; i < n; i++) {
	double a = 2 * M_PI * i / n;

	pts[i].x = cx + r * cos(a);
	pts[i].y = cy + r * sin(a);
    }
    FillPoly(t, pts, n);
}

/* One-pixel Bresenham line with optional dashes; dashPos carries across segments. */
static void
ThinLine(Target *t, int x0, int y0, int x1, int y1, int *dashPos)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    int dashed = t->lineStyle != LineSolid;
    int period = 2 * t->dashLen;

    for (;;) {
	int on = 1;

	if (dashed) {
	    int pos = (*dashPos) % period;

	    on = pos < t->dashLen;
	    if (!on && t->lineStyle == LineDoubleDash) {
		/* off dashes drawn in background: not supported; leave blank */
	    }
	    (*dashPos)++;
	}
	if (on) {
	    Plot(t, x0, y0);
	}
	if (x0 == x1 && y0 == y1) {
	    break;
	}
	e2 = 2 * err;
	if (e2 >= dy) { err += dy; x0 += sx; }
	if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void
ThickSegment(Target *t, double x0, double y0, double x1, double y1, double w)
{
    double dx = x1 - x0, dy = y1 - y0, len = sqrt(dx * dx + dy * dy), nx, ny, ex = 0, ey = 0;
    Pt q[4];

    if (len == 0) {
	Disc(t, x0, y0, w / 2);
	return;
    }
    nx = -dy / len * w / 2;
    ny = dx / len * w / 2;
    if (t->capStyle == CapProjecting) {
	ex = dx / len * w / 2;
	ey = dy / len * w / 2;
    }
    q[0].x = x0 - ex + nx; q[0].y = y0 - ey + ny;
    q[1].x = x1 + ex + nx; q[1].y = y1 + ey + ny;
    q[2].x = x1 + ex - nx; q[2].y = y1 + ey - ny;
    q[3].x = x0 - ex - nx; q[3].y = y0 - ey - ny;
    FillPoly(t, q, 4);
}

static void
PolyLine(Target *t, const Pt *p, int n)
{
    int i, dashPos = 0;

    if (n < 1) {
	return;
    }
    if (t->lineWidth <= 1) {
	if (n == 1) {
	    Plot(t, (int) p[0].x, (int) p[0].y);
	}
	for (i = 0; i + 1 < n; i++) {
	    ThinLine(t, (int) p[i].x, (int) p[i].y, (int) p[i + 1].x, (int) p[i + 1].y, &dashPos);
	}
	return;
    }
    for (i = 0; i + 1 < n; i++) {
	ThickSegment(t, p[i].x + 0.5, p[i].y + 0.5, p[i + 1].x + 0.5, p[i + 1].y + 0.5, t->lineWidth);
    }
    /* Joins and round caps: a disc at the vertices (a close approximation of miter/round). */
    for (i = 1; i + 1 < n; i++) {
	if (t->joinStyle != JoinBevel) {
	    Disc(t, p[i].x + 0.5, p[i].y + 0.5, t->lineWidth / 2.0);
	}
    }
    if (t->capStyle == CapRound) {
	Disc(t, p[0].x + 0.5, p[0].y + 0.5, t->lineWidth / 2.0);
	Disc(t, p[n - 1].x + 0.5, p[n - 1].y + 0.5, t->lineWidth / 2.0);
    }
}

/* ---- the X drawing API --------------------------------------------------------------------- */

int
XFillRectangles(Display *display, Drawable d, GC gc, XRectangle *r, int n)
{
    Target t;
    int i;

    (void) display;
    if (!Resolve(d, gc, &t)) {
	return 0;
    }
    for (i = 0; i < n; i++) {
	FillRectT(&t, t.ox + r[i].x, t.oy + r[i].y, r[i].width, r[i].height);
    }
    FreeTarget(&t);
    return 1;
}

int
XDrawRectangles(Display *display, Drawable d, GC gc, XRectangle *r, int n)
{
    Target t;
    int i;

    (void) display;
    if (!Resolve(d, gc, &t)) {
	return 0;
    }
    for (i = 0; i < n; i++) {
	int x = t.ox + r[i].x, y = t.oy + r[i].y, w = r[i].width, h = r[i].height;
	int lw = t.lineWidth > 1 ? t.lineWidth : 1, off = (lw - 1) / 2;

	/* Outline pixels span x..x+w and y..y+h, `lw` thick, centred on the edge. */
	FillRectT(&t, x - off, y - off, w + lw, lw);			/* top */
	FillRectT(&t, x - off, y + h - off, w + lw, lw);		/* bottom */
	FillRectT(&t, x - off, y - off + lw, lw, h - lw + 1);		/* left */
	FillRectT(&t, x + w - off, y - off + lw, lw, h - lw + 1);	/* right */
    }
    FreeTarget(&t);
    return 1;
}

int
XDrawRectangle(Display *display, Drawable d, GC gc, int x, int y,
	unsigned int w, unsigned int h)
{
    XRectangle r;

    r.x = (short) x;
    r.y = (short) y;
    r.width = (unsigned short) w;
    r.height = (unsigned short) h;
    return XDrawRectangles(display, d, gc, &r, 1);
}

int
XDrawLines(Display *display, Drawable d, GC gc, XPoint *pts, int n, int mode)
{
    Target t;
    Pt *p;
    int i;
    double cx = 0, cy = 0;

    (void) display;
    if (n < 1 || !Resolve(d, gc, &t)) {
	return 0;
    }
    p = (Pt *) malloc((size_t) n * sizeof(Pt));
    for (i = 0; i < n; i++) {
	if (mode == CoordModePrevious && i > 0) {
	    cx += pts[i].x;
	    cy += pts[i].y;
	} else {
	    cx = pts[i].x;
	    cy = pts[i].y;
	}
	p[i].x = t.ox + cx;
	p[i].y = t.oy + cy;
    }
    PolyLine(&t, p, n);
    free(p);
    FreeTarget(&t);
    return 1;
}

int
XFillPolygon(Display *display, Drawable d, GC gc, XPoint *pts, int n, int shape,
	int mode)
{
    Target t;
    Pt *p;
    int i;
    double cx = 0, cy = 0;

    (void) display;
    (void) shape;
    if (n < 3 || !Resolve(d, gc, &t)) {
	return 0;
    }
    p = (Pt *) malloc((size_t) n * sizeof(Pt));
    for (i = 0; i < n; i++) {
	if (mode == CoordModePrevious && i > 0) {
	    cx += pts[i].x;
	    cy += pts[i].y;
	} else {
	    cx = pts[i].x;
	    cy = pts[i].y;
	}
	p[i].x = t.ox + cx + 0.0;
	p[i].y = t.oy + cy + 0.0;
    }
    FillPoly(&t, p, n);
    free(p);
    FreeTarget(&t);
    return 1;
}

/* Sample an arc; angles are in 1/64 degrees, counter-clockwise from 3 o'clock. */
static int
ArcPoints(Target *t, const XArc *a, Pt **out)
{
    double rx = a->width / 2.0, ry = a->height / 2.0;
    double cx = t->ox + a->x + rx, cy = t->oy + a->y + ry;
    double a1 = a->angle1 / 64.0, sweep = a->angle2 / 64.0;
    int i, n;
    Pt *p;

    if (sweep > 360) sweep = 360;
    if (sweep < -360) sweep = -360;
    n = (int) (fabs(sweep) / 360.0 * (6.0 * sqrt(rx > ry ? rx : ry) + 8.0)) + 4;
    p = (Pt *) malloc((size_t) (n + 1) * sizeof(Pt));
    for (i = 0; i <= n; i++) {
	double ang = (a1 + sweep * i / n) * M_PI / 180.0;

	p[i].x = cx + rx * cos(ang) - 0.5;
	p[i].y = cy - ry * sin(ang) - 0.5;
    }
    *out = p;
    return n + 1;
}

int
XDrawArcs(Display *display, Drawable d, GC gc, XArc *arcs, int n)
{
    Target t;
    int i;

    (void) display;
    if (!Resolve(d, gc, &t)) {
	return 0;
    }
    for (i = 0; i < n; i++) {
	Pt *p;
	int k = ArcPoints(&t, &arcs[i], &p);

	PolyLine(&t, p, k);
	free(p);
    }
    FreeTarget(&t);
    return 1;
}

int
XDrawArc(Display *display, Drawable d, GC gc, int x, int y, unsigned int w,
	unsigned int h, int a1, int a2)
{
    XArc a;

    a.x = (short) x; a.y = (short) y; a.width = (unsigned short) w;
    a.height = (unsigned short) h; a.angle1 = (short) a1; a.angle2 = (short) a2;
    return XDrawArcs(display, d, gc, &a, 1);
}

int
XFillArcs(Display *display, Drawable d, GC gc, XArc *arcs, int n)
{
    Target t;
    int i;

    (void) display;
    if (!Resolve(d, gc, &t)) {
	return 0;
    }
    for (i = 0; i < n; i++) {
	Pt *p;
	int k = ArcPoints(&t, &arcs[i], &p);
	Pt *poly = (Pt *) malloc((size_t) (k + 1) * sizeof(Pt));
	int m;

	memcpy(poly, p, (size_t) k * sizeof(Pt));
	m = k;
	if (t.arcMode == ArcPieSlice && abs(arcs[i].angle2) < 360 * 64) {
	    poly[m].x = t.ox + arcs[i].x + arcs[i].width / 2.0 - 0.5;
	    poly[m].y = t.oy + arcs[i].y + arcs[i].height / 2.0 - 0.5;
	    m++;
	}
	FillPoly(&t, poly, m);
	free(poly);
	free(p);
    }
    FreeTarget(&t);
    return 1;
}

int
XFillArc(Display *display, Drawable d, GC gc, int x, int y, unsigned int w,
	unsigned int h, int a1, int a2)
{
    XArc a;

    a.x = (short) x; a.y = (short) y; a.width = (unsigned short) w;
    a.height = (unsigned short) h; a.angle1 = (short) a1; a.angle2 = (short) a2;
    return XFillArcs(display, d, gc, &a, 1);
}

/* ---- copying -------------------------------------------------------------------------------- */

static int
SourceBuffer(Drawable src, uint32_t **pix, int *stride, int *w, int *h, int *ox,
	int *oy, int *depth)
{
    TkADrawable *s = TKA_DRAWABLE(src);

    if (s == NULL) {
	return 0;
    }
    if (s->isWindow) {
	*pix = tkA.root->pix;
	*stride = tkA.root->w;
	*w = s->w;
	*h = s->h;
	TkAWindowOrigin(s, ox, oy);
	*depth = 24;
    } else {
	*pix = s->pix;
	*stride = s->w;
	*w = s->w;
	*h = s->h;
	*ox = *oy = 0;
	*depth = s->depth;
    }
    return 1;
}

/* Copy via a temporary so overlapping source/destination are safe. */
static int
CopyCommon(Drawable src, Drawable dst, GC gc, int sx, int sy, int w, int h,
	int dx, int dy, int plane)
{
    uint32_t *sp, *tmp;
    int sstride, sw, sh, sox, soy, sdepth, i, j;
    Target t;

    if (w <= 0 || h <= 0 || !SourceBuffer(src, &sp, &sstride, &sw, &sh, &sox, &soy, &sdepth)) {
	return 0;
    }
    if (!Resolve(dst, gc, &t)) {
	return 0;
    }
    tmp = (uint32_t *) malloc((size_t) w * (size_t) h * sizeof(uint32_t));
    for (j = 0; j < h; j++) {
	for (i = 0; i < w; i++) {
	    int x = sx + i, y = sy + j;

	    tmp[j * w + i] = (x < 0 || y < 0 || x >= sw || y >= sh) ? 0xFFFFFFFFu
		    : sp[(size_t) (soy + y) * sstride + sox + x];
	}
    }
    for (j = 0; j < h; j++) {
	for (i = 0; i < w; i++) {
	    uint32_t v = tmp[j * w + i];
	    int x = t.ox + dx + i, y = t.oy + dy + j, k;

	    if (v == 0xFFFFFFFFu && (sx + i < 0 || sy + j < 0 || sx + i >= sw || sy + j >= sh)) {
		continue;
	    }
	    if (x < 0 || y < 0 || x >= t.w || y >= t.h) {
		continue;
	    }
	    for (k = 0; k < t.clip->n; k++) {
		const TkARect *r = &t.clip->r[k];

		if (x >= r->x1 && x < r->x2 && y >= r->y1 && y < r->y2) {
		    uint32_t c;

		    if (t.maskPix) {
			uint32_t dummy;

			if (!ColorAt(&t, x, y, &dummy)) {
			    break;
			}
		    }
		    if (plane) {
			/* depth-1 source: bit set -> foreground, clear -> background */
			c = Store(&t, (v & 1) ? t.fg : t.bg);
		    } else if (sdepth == 1 && t.depth != 1) {
			c = Store(&t, (v & 1) ? t.fg : t.bg);
		    } else {
			c = v;
		    }
		    t.pix[(size_t) y * t.stride + x] = c;
		    if (t.isRoot) {
			TkAMarkDirty((TkARect) {x, y, x + 1, y + 1});
		    }
		    break;
		}
	    }
	}
    }
    free(tmp);
    FreeTarget(&t);
    return 1;
}

int
XCopyArea(Display *display, Drawable src, Drawable dst, GC gc, int sx, int sy,
	unsigned int w, unsigned int h, int dx, int dy)
{
    (void) display;
    return CopyCommon(src, dst, gc, sx, sy, (int) w, (int) h, dx, dy, 0);
}

int
XCopyPlane(Display *display, Drawable src, Drawable dst, GC gc, int sx, int sy,
	unsigned int w, unsigned int h, int dx, int dy, unsigned long plane)
{
    (void) display;
    (void) plane;
    return CopyCommon(src, dst, gc, sx, sy, (int) w, (int) h, dx, dy, 1);
}

/* ---- images -------------------------------------------------------------------------------------- */

static unsigned long
ImgGetPixel(XImage *im, int x, int y)
{
    const unsigned char *row = (const unsigned char *) im->data
	    + (size_t) y * (size_t) im->bytes_per_line;

    x += im->xoffset;
    if (im->format == XYBitmap || im->bits_per_pixel == 1) {
	unsigned char byte = row[x / 8];
	int bit = x % 8;

	if (im->bitmap_bit_order == MSBFirst) {
	    bit = 7 - bit;
	}
	return (byte >> bit) & 1;
    }
    switch (im->bits_per_pixel) {
    case 32: {
	const unsigned char *p = row + 4 * x;

	return im->byte_order == LSBFirst
		? (unsigned long) p[0] | (unsigned long) p[1] << 8 | (unsigned long) p[2] << 16 | (unsigned long) p[3] << 24
		: (unsigned long) p[3] | (unsigned long) p[2] << 8 | (unsigned long) p[1] << 16 | (unsigned long) p[0] << 24;
    }
    case 24: {
	const unsigned char *p = row + 3 * x;

	return im->byte_order == LSBFirst
		? (unsigned long) p[0] | (unsigned long) p[1] << 8 | (unsigned long) p[2] << 16
		: (unsigned long) p[2] | (unsigned long) p[1] << 8 | (unsigned long) p[0] << 16;
    }
    case 16: {
	const unsigned char *p = row + 2 * x;

	return im->byte_order == LSBFirst ? (unsigned long) p[0] | (unsigned long) p[1] << 8
		: (unsigned long) p[1] | (unsigned long) p[0] << 8;
    }
    default:
	return row[x];
    }
}

static int
ImgPutPixel(XImage *im, int x, int y, unsigned long v)
{
    unsigned char *row = (unsigned char *) im->data + (size_t) y * (size_t) im->bytes_per_line;

    x += im->xoffset;
    if (im->format == XYBitmap || im->bits_per_pixel == 1) {
	int bit = x % 8;

	if (im->bitmap_bit_order == MSBFirst) {
	    bit = 7 - bit;
	}
	if (v & 1) row[x / 8] |= (unsigned char) (1 << bit);
	else row[x / 8] &= (unsigned char) ~(1 << bit);
	return 0;
    }
    switch (im->bits_per_pixel) {
    case 32: {
	unsigned char *p = row + 4 * x;

	if (im->byte_order == LSBFirst) {
	    p[0] = (unsigned char) v; p[1] = (unsigned char) (v >> 8);
	    p[2] = (unsigned char) (v >> 16); p[3] = (unsigned char) (v >> 24);
	} else {
	    p[3] = (unsigned char) v; p[2] = (unsigned char) (v >> 8);
	    p[1] = (unsigned char) (v >> 16); p[0] = (unsigned char) (v >> 24);
	}
	break;
    }
    case 24: {
	unsigned char *p = row + 3 * x;

	if (im->byte_order == LSBFirst) {
	    p[0] = (unsigned char) v; p[1] = (unsigned char) (v >> 8); p[2] = (unsigned char) (v >> 16);
	} else {
	    p[2] = (unsigned char) v; p[1] = (unsigned char) (v >> 8); p[0] = (unsigned char) (v >> 16);
	}
	break;
    }
    default:
	row[x] = (unsigned char) v;
    }
    return 0;
}

static int
ImgDestroy(XImage *im)
{
    if (im->data != NULL) {
	free(im->data);
    }
    free(im);
    return 1;
}

static XImage *
ImgSub(XImage *im, int x, int y, unsigned int w, unsigned int h)
{
    XImage *sub = XCreateImage(NULL, NULL, (unsigned) im->depth, im->format, 0, NULL, w, h,
	    im->bitmap_pad, 0);
    unsigned int i, j;

    sub->data = (char *) calloc(1, (size_t) sub->bytes_per_line * h + 1);
    sub->byte_order = im->byte_order;
    sub->bitmap_bit_order = im->bitmap_bit_order;
    for (j = 0; j < h; j++) {
	for (i = 0; i < w; i++) {
	    ImgPutPixel(sub, (int) i, (int) j, ImgGetPixel(im, x + (int) i, y + (int) j));
	}
    }
    return sub;
}

static int
ImgAdd(XImage *im, long v)
{
    int x, y;

    for (y = 0; y < im->height; y++) {
	for (x = 0; x < im->width; x++) {
	    ImgPutPixel(im, x, y, (unsigned long) ((long) ImgGetPixel(im, x, y) + v));
	}
    }
    return 0;
}

int
_XInitImageFuncPtrs(XImage *im)
{
    im->f.create_image = XCreateImage;
    im->f.destroy_image = ImgDestroy;
    im->f.get_pixel = ImgGetPixel;
    im->f.put_pixel = ImgPutPixel;
    im->f.sub_image = ImgSub;
    im->f.add_pixel = ImgAdd;
    return 1;
}

XImage *
XCreateImage(Display *display, Visual *visual, unsigned int depth, int format,
	int offset, char *data, unsigned int width, unsigned int height,
	int pad, int bytesPerLine)
{
    XImage *im = (XImage *) calloc(1, sizeof(XImage));

    (void) display;
    im->width = (int) width;
    im->height = (int) height;
    im->xoffset = offset;
    im->format = format;
    im->data = data;
    im->byte_order = LSBFirst;
    im->bitmap_unit = 32;
    im->bitmap_bit_order = LSBFirst;
    im->bitmap_pad = pad ? pad : 32;
    im->depth = (int) depth;
    im->bits_per_pixel = (format == XYBitmap) ? 1 : (depth > 16 ? 32 : depth > 8 ? 16 : 8);
    if (visual != NULL) {
	im->red_mask = visual->red_mask;
	im->green_mask = visual->green_mask;
	im->blue_mask = visual->blue_mask;
    }
    if (bytesPerLine == 0) {
	int bits = im->bits_per_pixel * (int) width;
	int padbits = im->bitmap_pad;

	bytesPerLine = ((bits + padbits - 1) / padbits) * padbits / 8;
    }
    im->bytes_per_line = bytesPerLine;
    _XInitImageFuncPtrs(im);
    return im;
}

int
XPutImage(Display *display, Drawable d, GC gc, XImage *im, int sx, int sy,
	int dx, int dy, unsigned int w, unsigned int h)
{
    Target t;
    unsigned int i, j;

    (void) display;
    if (!Resolve(d, gc, &t)) {
	return 0;
    }
    for (j = 0; j < h; j++) {
	for (i = 0; i < w; i++) {
	    unsigned long v = im->f.get_pixel(im, sx + (int) i, sy + (int) j);
	    unsigned long saveFg = t.fg;
	    int saveFill = t.fillStyle;
	    uint32_t c;

	    if (im->format == XYBitmap || im->depth == 1) {
		/* a bitmap: set bits take the GC foreground, clear bits the background */
		t.fg = v ? gc->foreground : gc->background;
		t.fillStyle = FillSolid;
		if (t.depth == 1) {
		    t.fg = v;
		}
	    } else {
		t.fg = v & 0xFFFFFF;
		t.fillStyle = FillSolid;
	    }
	    (void) c;
	    Plot(&t, t.ox + dx + (int) i, t.oy + dy + (int) j);
	    t.fg = saveFg;
	    t.fillStyle = saveFill;
	}
    }
    FreeTarget(&t);
    return 1;
}

XImage *
XGetImage(Display *display, Drawable d, int x, int y, unsigned int w,
	unsigned int h, unsigned long planeMask, int format)
{
    uint32_t *sp;
    int stride, sw, sh, sox, soy, depth;
    XImage *im;
    unsigned int i, j;

    (void) planeMask;
    (void) format;
    if (!SourceBuffer(d, &sp, &stride, &sw, &sh, &sox, &soy, &depth)) {
	return NULL;
    }
    im = XCreateImage(display, DefaultVisual(display, 0), 24, ZPixmap, 0, NULL, w, h, 32, 0);
    im->data = (char *) calloc(1, (size_t) im->bytes_per_line * h + 1);
    for (j = 0; j < h; j++) {
	for (i = 0; i < w; i++) {
	    int px = x + (int) i, py = y + (int) j;
	    uint32_t v = (px < 0 || py < 0 || px >= sw || py >= sh) ? 0
		    : sp[(size_t) (soy + py) * stride + sox + px];

	    ImgPutPixel(im, (int) i, (int) j, v & 0xFFFFFF);
	}
    }
    return im;
}

/* ---- text ------------------------------------------------------------------------------------------- */

void
TkADrawText(Display *display, Drawable drawable, GC gc, void *font,
	const char *utf8, int nbytes, int x, int y)
{
    Target t;
    uint8_t *mask;
    int mw, mh, base, i, j;

    (void) display;
    if (nbytes <= 0 || tkAEngine == NULL) {
	return;
    }
    mask = tkAEngine->render(font, utf8, nbytes, &mw, &mh, &base);
    if (mask == NULL) {
	return;
    }
    if (Resolve(drawable, gc, &t)) {
	int top = t.oy + y - base, left = t.ox + x;
	uint32_t fg = Store(&t, t.fg);

	for (j = 0; j < mh; j++) {
	    for (i = 0; i < mw; i++) {
		int a = mask[j * mw + i], px = left + i, py = top + j, k;
		uint32_t cc;

		if (a == 0 || px < 0 || py < 0 || px >= t.w || py >= t.h) {
		    continue;
		}
		for (k = 0; k < t.clip->n; k++) {
		    const TkARect *r = &t.clip->r[k];

		    if (px >= r->x1 && px < r->x2 && py >= r->y1 && py < r->y2) {
			uint32_t *dst = &t.pix[(size_t) py * t.stride + px];

			if (!Simple(&t) && !ColorAt(&t, px, py, &cc)) {
			    break;
			}
			if (t.depth == 1) {
			    if (a > 127) *dst = 1;
			} else if (a >= 250) {
			    *dst = fg;
			} else {
			    uint32_t d0 = *dst;
			    int sr = (fg >> 16) & 0xFF, sg = (fg >> 8) & 0xFF, sb = fg & 0xFF;
			    int dr = (d0 >> 16) & 0xFF, dg = (d0 >> 8) & 0xFF, db = d0 & 0xFF;

			    *dst = TKA_OPAQUE | (uint32_t) (((dr * (255 - a) + sr * a) / 255) << 16)
				    | (uint32_t) (((dg * (255 - a) + sg * a) / 255) << 8)
				    | (uint32_t) ((db * (255 - a) + sb * a) / 255);
			}
			if (t.isRoot) {
			    TkAMarkDirty((TkARect) {px, py, px + 1, py + 1});
			}
			break;
		    }
		}
	    }
	}
	FreeTarget(&t);
    }
    free(mask);
}

/* ---- small Tk platform hooks (as in unix/tkUnixDraw.c) -------------------------------------------------- */

void
Tk_DrawHighlightBorder(Tk_Window tkwin, GC fgGC, GC bgGC, int highlightWidth,
	Drawable drawable)
{
    (void) bgGC;
    TkDrawInsetFocusHighlight(tkwin, fgGC, highlightWidth, drawable, 0);
}

void
TkpDrawFrameEx(Tk_Window tkwin, Drawable drawable, Tk_3DBorder border,
	int highlightWidth, int borderWidth, int relief)
{
    Tk_Fill3DRectangle(tkwin, drawable, border, highlightWidth, highlightWidth,
	    Tk_Width(tkwin) - 2 * highlightWidth,
	    Tk_Height(tkwin) - 2 * highlightWidth, borderWidth, relief);
}

GContext
XGContextFromGC(GC gc)
{
    return (GContext) (uintptr_t) gc;
}
