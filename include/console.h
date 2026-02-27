#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <Archimedes.h>
#include <Daedalus.h>

#define CONSOLE_MAX_LINES   64
#define CONSOLE_VISIBLE      8

typedef struct
{
  dString_t* text;
  aColor_t color;
} ConsoleLine_t;

typedef struct
{
  dArray_t* lines;
  int scroll_offset;
} Console_t;

void ConsoleInit( Console_t* c );
void ConsoleDestroy( Console_t* c );
void ConsolePush( Console_t* c, const char* msg, aColor_t color );
void ConsolePushF( Console_t* c, aColor_t color, const char* fmt, ... );
void ConsoleScroll( Console_t* c, int delta );
void ConsoleDraw( Console_t* c, aRectf_t rect );
void ConsoleClear( Console_t* c );

#endif
