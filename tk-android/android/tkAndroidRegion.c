/*
 * tkAndroidRegion.c --
 *
 *	Regions as lists of disjoint rectangles. Used both for Tk's X-shaped
 *	Region API (XCreateRegion, XUnionRectWithRegion, ...) and internally for
 *	clipping. Pure geometry: no platform dependencies.
 */

#include "tkAndroidPriv.h"

TkARegion *
TkARegionNew(void)
{
    return (TkARegion *) calloc(1, sizeof(TkARegion));
}

void
TkARegionFree(TkARegion *r)
{
    if (r != NULL) {
	free(r->r);
	free(r);
    }
}

void
TkARegionClear(TkARegion *r)
{
    r->n = 0;
}

static void
Append(TkARegion *r, int x1, int y1, int x2, int y2)
{
    if (x1 >= x2 || y1 >= y2) {
	return;
    }
    if (r->n == r->cap) {
	r->cap = r->cap ? r->cap * 2 : 8;
	r->r = (TkARect *) realloc(r->r, (size_t) r->cap * sizeof(TkARect));
    }
    r->r[r->n].x1 = x1;
    r->r[r->n].y1 = y1;
    r->r[r->n].x2 = x2;
    r->r[r->n].y2 = y2;
    r->n++;
}

void
TkARegionCopy(TkARegion *dst, const TkARegion *src)
{
    int i;

    if (dst == src) {
	return;
    }
    dst->n = 0;
    for (i = 0; i < src->n; i++) {
	Append(dst, src->r[i].x1, src->r[i].y1, src->r[i].x2, src->r[i].y2);
    }
}

void
TkARegionSubtractRect(TkARegion *r, TkARect s)
{
    int i, n = r->n;
    TkARect *old = r->r;
    TkARegion out = {0, 0, NULL};

    /* Split every rectangle around s (up to four pieces each). */
    for (i = 0; i < n; i++) {
	TkARect a = old[i];

	if (s.x2 <= a.x1 || s.x1 >= a.x2 || s.y2 <= a.y1 || s.y1 >= a.y2) {
	    Append(&out, a.x1, a.y1, a.x2, a.y2);
	    continue;
	}
	Append(&out, a.x1, a.y1, a.x2, s.y1 > a.y1 ? s.y1 : a.y1);	/* top */
	Append(&out, a.x1, s.y2 < a.y2 ? s.y2 : a.y2, a.x2, a.y2);	/* bottom */
	{
	    int y1 = s.y1 > a.y1 ? s.y1 : a.y1;
	    int y2 = s.y2 < a.y2 ? s.y2 : a.y2;

	    Append(&out, a.x1, y1, s.x1 > a.x1 ? s.x1 : a.x1, y2);	/* left */
	    Append(&out, s.x2 < a.x2 ? s.x2 : a.x2, y1, a.x2, y2);	/* right */
	}
    }
    free(old);
    *r = out;
}

void
TkARegionIntersectRect(TkARegion *r, TkARect s)
{
    int i, j = 0;

    for (i = 0; i < r->n; i++) {
	TkARect a = r->r[i];

	if (a.x1 < s.x1) a.x1 = s.x1;
	if (a.y1 < s.y1) a.y1 = s.y1;
	if (a.x2 > s.x2) a.x2 = s.x2;
	if (a.y2 > s.y2) a.y2 = s.y2;
	if (a.x1 < a.x2 && a.y1 < a.y2) {
	    r->r[j++] = a;
	}
    }
    r->n = j;
}

void
TkARegionAddRect(TkARegion *r, TkARect s)
{
    if (s.x1 >= s.x2 || s.y1 >= s.y2) {
	return;
    }
    TkARegionSubtractRect(r, s);	/* keep rectangles disjoint */
    Append(r, s.x1, s.y1, s.x2, s.y2);
}

/*
 * ---------------------------------------------------------------------------
 * Tk's X-shaped Region API. Region is an opaque pointer type in Tk's Xlib
 * header; it points at a TkARegion here.
 * ---------------------------------------------------------------------------
 */

#define RGN(x) ((TkARegion *) (x))

Region
XCreateRegion(void)
{
    return (Region) TkARegionNew();
}

int
XDestroyRegion(Region rgn)
{
    TkARegionFree(RGN(rgn));
    return 1;
}

void
TkpCopyRegion(TkRegion dst, TkRegion src)
{
    TkARegionCopy(RGN(dst), RGN(src));
}

int
XUnionRectWithRegion(XRectangle *rect, Region src, Region dst)
{
    TkARect s;

    s.x1 = rect->x;
    s.y1 = rect->y;
    s.x2 = rect->x + rect->width;
    s.y2 = rect->y + rect->height;
    if (RGN(dst) != RGN(src)) {
	TkARegionCopy(RGN(dst), RGN(src));
    }
    TkARegionAddRect(RGN(dst), s);
    return 1;
}

int
XIntersectRegion(Region a, Region b, Region dst)
{
    TkARegion *out = TkARegionNew(), *tmp = TkARegionNew();
    int i;

    for (i = 0; i < RGN(b)->n; i++) {
	TkARegionCopy(tmp, RGN(a));
	TkARegionIntersectRect(tmp, RGN(b)->r[i]);
	{
	    int j;

	    for (j = 0; j < tmp->n; j++) {
		TkARegionAddRect(out, tmp->r[j]);
	    }
	}
    }
    TkARegionCopy(RGN(dst), out);
    TkARegionFree(out);
    TkARegionFree(tmp);
    return 1;
}

int
XSubtractRegion(Region a, Region b, Region dst)
{
    int i;

    if (RGN(dst) != RGN(a)) {
	TkARegionCopy(RGN(dst), RGN(a));
    }
    for (i = 0; i < RGN(b)->n; i++) {
	TkARegionSubtractRect(RGN(dst), RGN(b)->r[i]);
    }
    return 1;
}

int
XOffsetRegion(Region rgn, int dx, int dy)
{
    int i;

    for (i = 0; i < RGN(rgn)->n; i++) {
	RGN(rgn)->r[i].x1 += dx;
	RGN(rgn)->r[i].x2 += dx;
	RGN(rgn)->r[i].y1 += dy;
	RGN(rgn)->r[i].y2 += dy;
    }
    return 1;
}

int
XClipBox(Region rgn, XRectangle *box)
{
    TkARegion *r = RGN(rgn);
    int i, x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    for (i = 0; i < r->n; i++) {
	if (i == 0 || r->r[i].x1 < x1) x1 = r->r[i].x1;
	if (i == 0 || r->r[i].y1 < y1) y1 = r->r[i].y1;
	if (i == 0 || r->r[i].x2 > x2) x2 = r->r[i].x2;
	if (i == 0 || r->r[i].y2 > y2) y2 = r->r[i].y2;
    }
    box->x = (short) x1;
    box->y = (short) y1;
    box->width = (unsigned short) (x2 - x1);
    box->height = (unsigned short) (y2 - y1);
    return 1;
}

int
XRectInRegion(Region rgn, int x, int y, unsigned int width, unsigned int height)
{
    TkARegion *r = RGN(rgn);
    long covered = 0, area = (long) width * (long) height;
    int i;

    for (i = 0; i < r->n; i++) {
	int x1 = r->r[i].x1 > x ? r->r[i].x1 : x;
	int y1 = r->r[i].y1 > y ? r->r[i].y1 : y;
	int x2 = r->r[i].x2 < (int) (x + width) ? r->r[i].x2 : (int) (x + width);
	int y2 = r->r[i].y2 < (int) (y + height) ? r->r[i].y2 : (int) (y + height);

	if (x1 < x2 && y1 < y2) {
	    covered += (long) (x2 - x1) * (y2 - y1);
	}
    }
    if (covered == 0) {
	return RectangleOut;
    }
    return covered >= area ? RectangleIn : RectanglePart;
}

/* Build a region from the opaque pixels of an alpha mask (shaped windows, -bitmap). */
void
TkpBuildRegionFromAlphaData(Region region, unsigned x, unsigned y,
	unsigned width, unsigned height, unsigned char *data,
	unsigned pixelStride, unsigned lineStride)
{
    unsigned i, j;

    for (i = 0; i < height; i++) {
	unsigned char *row = data + i * lineStride;

	for (j = 0; j < width; ) {
	    TkARect run;

	    if (row[j * pixelStride] == 0) {
		j++;
		continue;
	    }
	    run.x1 = (int) (x + j);
	    while (j < width && row[j * pixelStride] != 0) {
		j++;
	    }
	    run.x2 = (int) (x + j);
	    run.y1 = (int) (y + i);
	    run.y2 = (int) (y + i + 1);
	    TkARegionAddRect(RGN(region), run);
	}
    }
}
