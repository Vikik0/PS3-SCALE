#ifndef CRASH_BASH_PS3_SDL3_SHIM_H
#define CRASH_BASH_PS3_SDL3_SHIM_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <wchar.h>

typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int64_t Sint64;
typedef FILE SDL_IOStream;

Uint64 SDL_GetTicks(void);
Uint64 SDL_GetTicksNS(void);
Uint64 SDL_GetPerformanceCounter(void);
void SDL_Delay(Uint32 milliseconds);
size_t SDL_strlcpy(char *destination, const char *source, size_t size);
int SDL_snprintf(char *buffer, size_t size, const char *format, ...);
char *SDL_iconv_string(const char *to, const char *from, const char *input, size_t length);
size_t SDL_wcslen(const wchar_t *string);
void SDL_free(void *memory);
SDL_IOStream *SDL_IOFromFile(const char *file, const char *mode);
Sint64 SDL_GetIOSize(SDL_IOStream *stream);
size_t SDL_ReadIO(SDL_IOStream *stream, void *destination, size_t size);
int SDL_CloseIO(SDL_IOStream *stream);

#endif
