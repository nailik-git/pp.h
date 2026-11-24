#ifndef PP_H
#define PP_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef PP_NO_PREFIX
#define PP_FORMATTER_F formatter_f
#define PP_PRINT print
#define PP_FPRINT fprint
#define PP_DPRINT dprint
#define PP_SPRINT sprint
#define PP_VPRINT vprint
#define PP_VFPRINT vfprint
#define PP_VDPRINT vdprint
#define PP_VSPRINT vsprint
#define PP_REGISTER_FORMATTER register_formatter
#else
#define PP_FORMATTER_F pp_formatter_f
#define PP_PRINT pp_print
#define PP_FPRINT pp_fprint
#define PP_DPRINT pp_dprint
#define PP_SPRINT pp_sprint
#define PP_VPRINT pp_vprint
#define PP_VFPRINT pp_vfprint
#define PP_VDPRINT pp_vdprint
#define PP_VSPRINT pp_vsprint
#define PP_REGISTER_FORMATTER pp_register_formatter
#endif /* PP_NO_PREFIX */

// user formatter functions
typedef int(*PP_FORMATTER_F)(FILE* restrict f, const char* restrict fmt, va_list ap);

// prints to stdout based on fmt
// format of fmt:
// {<specifier>[:args]}
//
// supports most of printf (through forwarding to it)
// specifier of '%.2s' would be s
// args is everything before the specifier, except for '%' (args need to be in normal printf order)
//
// curly braces can be escaped using '\\'
// all '\\' not before a curly brace gets printed as a normal '\\'
//
// returns number of bytes printed on success
// returns negative value on failure, errno is set to indicate the error
int PP_PRINT(const char* restrict fmt, ...);

// same as pp_print, but specify output FILE*
int PP_FPRINT(FILE* restrict f, const char* restrict fmt, ...);

// same as pp_print, but print to a fd
// the fd will be opened using fdopen
int PP_DPRINT(int fd, const char* restrict fmt, ...);

// same as pp_print, but print to a string
// the string will be opened using fmemopen
int PP_SPRINT(char* restrict str, size_t size, const char* restrict fmt, ...);

// same as pp_print, but takes a va_list instead of variadic arguments
int PP_VPRINT(const char* restrict fmt, va_list ap);

// same as pp_fprint, but takes a va_list instead of variadic arguments
int PP_VFPRINT(FILE* restrict f, const char* restrict fmt, va_list ap);

// same as pp_dprint, but takes a va_list instead of variadic arguments
int PP_VDPRINT(int fd, const char* restrict fmt, va_list ap);

// same as pp_sprint, but takes a va_list instead of variadic arguments
int PP_VSPRINT(char* restrict str, size_t size, const char* restrict fmt, va_list ap);

// registers a new formatter
// returns 0 on success and -1 on failure, errno is set to indicate the error
//
// fails if the specifier is already defined by printf, errno is then set to EINVAL
//
// formatter_func shall pull the arguments it uses out of ap
// formatter_func shall return the number of bytes printed to stdout
int PP_REGISTER_FORMATTER(char specifier, PP_FORMATTER_F formatter_func);

#ifdef PP_IMPLEMENTATION
// ------------------------------------------------------------------------------------------------
// implementation

#include <errno.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdbool.h>

typedef struct {
  PP_FORMATTER_F function;
  char specifier;
} __pp_formatter_t;

struct {
  __pp_formatter_t* items;
  size_t count;
  size_t capacity;
} __pp = {0};

static ssize_t __pp_strtcpy(char *restrict dst, size_t dsize, const char *restrict src, size_t slen) {
  bool   trunc;
  size_t dlen;

  if (dsize == 0) {
    errno = ENOBUFS;
    return -1;
  }

  trunc = (slen == dsize);
  dlen = slen - trunc;

  stpcpy(mempcpy(dst, src, dlen), "");
  if (trunc)
    errno = E2BIG;
  return trunc ? -1 : (ssize_t)slen;
}

static char __pp_get_first_char_escaped(const char* restrict* fmt) {
  const char* p = *fmt;
  if(*p == '\\') {
    if(p[1] == '{')      {(*fmt)++; return '{';}
    else if(p[1] == '}') {(*fmt)++; return '}';}
    else                       return '\\';
  }

  return *p;
}

static const char* __pp_tmp_fmt_to_printf(const char* restrict fmt) {
  static char buf[0x1000] = "%";
  char* p = buf + 1;

  if(fmt[1] == ':') {
    int ret = __pp_strtcpy(p, 0x1000 - 2, &fmt[2], strlen(&fmt[2]));
    if(ret == -1 && errno == E2BIG) {
      p += 0x1000 - 3;
    } else {
      p += ret;
    }
  }
  *p++ = *fmt;
  *p = 0;

  return buf;
}

static int __pp_print_dispatcher(FILE* restrict f, const char* restrict fmt, va_list ap) {
  int num_printed = 0;

  switch(*fmt) {
    case 'd':
    case 'i':
    case 'u':
    case 'b':
    case 'o':
    case 'x':
    case 'X':
    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
    case 'c':
    case 's':
    case 'p':
      num_printed = vfprintf(f, __pp_tmp_fmt_to_printf(fmt), ap);
      break;
    case 'n':
      errno = 0;
      num_printed = -1;
      break;
    default:
      for(size_t i = 0; i < __pp.count; i++) {
        if(__pp.items[i].specifier == *fmt) {
          num_printed = __pp.items[i].function(f, fmt[1] == ':' ? fmt + 2 : fmt + 1, ap);
          break;
        }
      }
      break;
  }

  return num_printed;
}

// ------------------------------------------------------------------------------------------------
// public api implementation

int PP_PRINT(const char* restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = PP_VPRINT(fmt, ap);
  va_end(ap);
  return ret;
}

int PP_FPRINT(FILE* restrict f, const char* restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = PP_VFPRINT(f, fmt, ap);
  va_end(ap);
  return ret;
}

int PP_DPRINT(int fd, const char* restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = PP_VDPRINT(fd, fmt, ap);
  va_end(ap);
  return ret;
}

int PP_SPRINT(char* restrict str, size_t size, const char* restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = PP_VSPRINT(str, size, fmt, ap);
  va_end(ap);
  return ret;
}

int PP_VPRINT(const char* restrict fmt, va_list ap) {
  return PP_VFPRINT(stdout, fmt, ap);
}

int PP_VFPRINT(FILE* restrict f, const char* restrict fmt, va_list ap) {
  int num_printed = 0;

  for(; *fmt; fmt++) {
    const char* saved = fmt;
    char c = __pp_get_first_char_escaped(&fmt);
    bool escaped = saved != fmt;

    if(!escaped && c == '{') { // start of format parameter
      static char buf[0x1000] = "";
      char* p = buf;

      fmt++;
      while(*fmt) {
        saved = fmt;
        c = __pp_get_first_char_escaped(&fmt);
        escaped = saved != fmt;

        if(!escaped && c == '}') break;

        *p++ = c;
        fmt++;
      }
      *p = 0;

      int ret = __pp_print_dispatcher(f, buf, ap);
      if(ret == -1 && errno == 0) {
        int* n = va_arg(ap, int*);
        *n = num_printed;
      } else if(ret < 0) {
        return ret;
      } else num_printed += ret;
    } else {
      if(fputc(c, f) == EOF) {
        errno = E2BIG;
        return -1;
      }
      num_printed++;
    }
  }

  return num_printed;
}

int PP_VDPRINT(int fd, const char* restrict fmt, va_list ap) {
  FILE* f = NULL;
  switch(fd) {
    case 0:
      errno = EINVAL;
      break;
    case 1:
      f = stdout;
      break;
    case 2:
      f = stderr;
      break;
    default:
      f = fdopen(fd, "w");
      break;
  }
  if(f == NULL) return -1;
  int ret = PP_VFPRINT(f, fmt, ap);
  if(fd > 2) fclose(f);
  return ret;
}

int PP_VSPRINT(char* restrict str, size_t size, const char* restrict fmt, va_list ap) {
  FILE* f = fmemopen(str, size, "w");
  if(f == NULL) return -1;
  int ret = PP_VFPRINT(f, fmt, ap);
  if(fputc(0, f) == EOF) {
    errno = E2BIG;
    ret = -1;
  }
  fclose(f);
  str[size - 1] = 0;
  return ret;
}

int PP_REGISTER_FORMATTER(char specifier, PP_FORMATTER_F formatter_func) {
  switch(specifier) {
    case 'd':
    case 'i':
    case 'u':
    case 'b':
    case 'o':
    case 'x':
    case 'X':
    case 'e':
    case 'E':
    case 'f':
    case 'F':
    case 'g':
    case 'G':
    case 'a':
    case 'A':
    case 'c':
    case 's':
    case 'p':
    case 'n':
      errno = EINVAL;
      return -1;
  }

  __pp_formatter_t item = {.function = formatter_func, .specifier = specifier};

  if(__pp.capacity == 0) {
    __pp.items = malloc((__pp.capacity = 8) * sizeof(item));
    if(__pp.items == NULL) {
      __pp.capacity = 0;
      return -1;
    }
  } else if(__pp.count == __pp.capacity) {
    __pp_formatter_t* saved = __pp.items;
    __pp.items = realloc(__pp.items, (__pp.capacity *= 2) * sizeof(item));
    if(__pp.items == NULL) {
      __pp.items = saved;
      __pp.capacity /= 2;
      return -1;
    }
  }
  __pp.items[__pp.count++] = item;

  return 0;
}

#endif /* PP_IMPLEMENTATION */

#undef PP_FORMATTER_F
#undef PP_PRINT
#undef PP_VPRINT
#undef PP_REGISTER_FORMATTER

#endif /* PP_H */
