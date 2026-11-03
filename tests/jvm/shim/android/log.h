/* Host stand-in for <android/log.h>, so the real JNI sources build on the build machine. */
#ifndef SHIM_ANDROID_LOG_H
#define SHIM_ANDROID_LOG_H
#include <stdio.h>
#include <stdarg.h>
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_ERROR 6
static inline int __android_log_print(int prio, const char *tag, const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); fprintf(stderr, "[%s/%d] ", tag, prio); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap); return 0; }
static inline int __android_log_write(int prio, const char *tag, const char *text)
{ fprintf(stderr, "[%s/%d] %s\n", tag, prio, text); return 0; }
#endif
