/*
 * embed_test.c -- drives tcldroid_core.c (the JNI-free half of the Android host)
 * on the build machine. See tests/host-embed.sh.
 *
 *   usage: embed_test <libdir> <filesdir> <cachedir> <tz> <script.tcl>
 *
 * Exit 0 only if the script finished OK and printed "TCLDROID_SMOKE: PASS".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tcldroid_core.h"

static char captured[1 << 16];
static size_t capturedLen;

static void
OnOutput(void *ctx, const char *buf, int len)
{
    (void) ctx;
    if (capturedLen + (size_t) len < sizeof(captured) - 1) {
	memcpy(captured + capturedLen, buf, (size_t) len);
	capturedLen += (size_t) len;
	captured[capturedLen] = '\0';
    }
    fwrite(buf, 1, (size_t) len, stdout);
}

static char *
ReadFile(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *s;

    if (f == NULL) {
	perror(path);
	exit(2);
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    rewind(f);
    s = malloc((size_t) n + 1);
    if (fread(s, 1, (size_t) n, f) != (size_t) n) {
	exit(2);
    }
    s[n] = '\0';
    fclose(f);
    return s;
}

int
main(int argc, char **argv)
{
    TclDroidConfig cfg;
    char *result;
    int resultLen, ok;

    if (argc != 6) {
	fprintf(stderr, "usage: %s libdir filesdir cachedir tz script.tcl\n",
		argv[0]);
	return 2;
    }
    cfg.libDir = argv[1];
    cfg.filesDir = argv[2];
    cfg.cacheDir = argv[3];
    cfg.tz = argv[4];
    cfg.script = ReadFile(argv[5]);
    cfg.onOutput = OnOutput;
    cfg.onInit = NULL;
    cfg.ctx = NULL;

    ok = tcldroid_run(&cfg, &result, &resultLen);
    printf("\n[tcldroid_run] ok=%d result=%.*s\n", ok, resultLen, result);
    free(result);
    return (ok && strstr(captured, "TCLDROID_SMOKE: PASS") != NULL) ? 0 : 1;
}
