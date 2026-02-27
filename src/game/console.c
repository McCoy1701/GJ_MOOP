#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <Archimedes.h>

#include "console.h"

#define CON_PAD_X    8.0f
#define CON_PAD_Y    4.0f
#define CON_SCALE    1.0f
#define CON_LINE_H  16.0f

void ConsoleInit( Console_t* c )
{
  memset( c, 0, sizeof( *c ) );
}

void ConsolePush( Console_t* c, const char* msg, aColor_t color )
{
  int idx;

  if ( c->count < CONSOLE_MAX_LINES )
  {
    idx = ( c->head + c->count ) % CONSOLE_MAX_LINES;
    c->count++;
  }
  else
  {
    idx = c->head;
    c->head = ( c->head + 1 ) % CONSOLE_MAX_LINES;
  }

  snprintf( c->lines[idx].text, sizeof( c->lines[idx].text ), "%s", msg );
  c->lines[idx].color = color;
}

void ConsolePushF( Console_t* c, aColor_t color, const char* fmt, ... )
{
  char buf[256];
  va_list args;

  va_start( args, fmt );
  vsnprintf( buf, sizeof( buf ), fmt, args );
  va_end( args );

  ConsolePush( c, buf, color );
}

void ConsoleDraw( Console_t* c, aRectf_t rect )
{
  if ( c->count == 0 )
    return;

  aTextStyle_t ts = a_default_text_style;
  ts.bg    = (aColor_t){ 0, 0, 0, 0 };
  ts.scale = CON_SCALE;
  ts.align = TEXT_ALIGN_LEFT;

  /* How many lines can we actually show */
  int visible = CONSOLE_VISIBLE;
  if ( c->count < visible )
    visible = c->count;

  /* Draw bottom-aligned: newest line at bottom of rect */
  float base_y = rect.y + rect.h - CON_PAD_Y - CON_LINE_H;
  float x      = rect.x + CON_PAD_X;

  for ( int i = 0; i < visible; i++ )
  {
    /* Walk backwards from newest */
    int line_idx = ( c->head + c->count - 1 - i ) % CONSOLE_MAX_LINES;
    float y = base_y - ( i * CON_LINE_H );

    /* Prefix */
    ts.fg = (aColor_t){ 120, 120, 120, 255 };
    a_DrawText( "> ", (int)x, (int)y, ts );

    /* Message */
    ts.fg = c->lines[line_idx].color;
    a_DrawText( c->lines[line_idx].text,
                (int)( x + 16.0f ), (int)y, ts );
  }
}

void ConsoleClear( Console_t* c )
{
  c->head  = 0;
  c->count = 0;
}
