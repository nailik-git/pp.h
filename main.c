#include <stdio.h>

#define PP_IMPLEMENTATION
#define PP_NO_PREFIX
#include "pp.h"

typedef struct {
  int x;
  int y;
} Vec2;

typedef union {
  struct {int x; int y; int z;};
  struct {Vec2 xy; int __pad0;};
  struct {int __pad1; Vec2 yz;};
} Vec3;

int print_vec2(FILE* f, Vec2 v) {
  return fprint(f, "\\{{d}, {d}\\}", v.x, v.y);
}
int print_vec3(FILE* f, Vec3 v) {
  return fprint(f, "\\{{d}, {d}, {d}\\}", v.x, v.y, v.z);
}

int print_vec(FILE* f, const char* fmt, va_list ap) {
  if     (*fmt == '2') return print_vec2(f, va_arg(ap, Vec2));
  else if(*fmt == '3') return print_vec3(f, va_arg(ap, Vec3));

  return -1;
}

int main() {
  print("{s:.2} {s} \\{asdads\\}\n", "hi(test)", "a");

  register_formatter('v', print_vec);

  Vec2 a = {.x = 1, .y = 2};
  Vec3 b = {.xy = a, .__pad0 = 3};
  print("print:\n");
  print("\t{v2}, {v3}\n", a, b);
  print("fprint:\n");
  fprint(stdout, "\t{v2}, {v3}\n", a, b);
  print("dprint:\n");
  dprint(1, "\t{v2}, {v3}\n", a, b);

  print("sprint:\n");
  char buf[0x100];
  sprint(buf, 0x100, "\t{v2}, {v3}\n", a, b);
  print("{s}", buf);

  return 0;
}
