/* SDL 1.2 configuration for Linux (written for this project).
   Only used to compile against SDL's type definitions; SDL itself is loaded
   at runtime from the device's own libSDL-1.2.so.0. */
#ifndef _SDL_config_h
#define _SDL_config_h
#include "SDL_platform.h"
#define SDL_HAS_64BIT_TYPE 1
#define HAVE_LIBC 1
#define HAVE_STDIO_H 1
#define STDC_HEADERS 1
#define HAVE_STDLIB_H 1
#define HAVE_STDARG_H 1
#define HAVE_MALLOC_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_CTYPE_H 1
#define HAVE_MATH_H 1
#define HAVE_SIGNAL_H 1
#endif
