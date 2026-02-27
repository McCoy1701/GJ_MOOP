#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <Archimedes.h>

#define CONSOLE_MAX_LINES   64
#define CONSOLE_VISIBLE      6

typedef struct
{
  char text[256];
  aColor_t color;
} ConsoleLine_t;

typedef struct
{
  ConsoleLine_t lines[CONSOLE_MAX_LINES];
  int head;
  int count;
} Console_t;

void ConsoleInit( Console_t* c );
void ConsolePush( Console_t* c, const char* msg, aColor_t color );
void ConsolePushF( Console_t* c, aColor_t color, const char* fmt, ... );
void ConsoleDraw( Console_t* c, aRectf_t rect );
void ConsoleClear( Console_t* c );

#endif
