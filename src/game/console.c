#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "console.h"

#define CON_PAD_X    8.0f
#define CON_PAD_Y    4.0f
#define CON_SCALE    1.0f
#define CON_LINE_H  16.0f
#define CON_SB_W     8.0f

void ConsoleInit( Console_t* c )
{
  c->lines = d_ArrayInit( CONSOLE_MAX_LINES, sizeof( ConsoleLine_t ) );
  c->scroll_offset = 0;
}

void ConsoleDestroy( Console_t* c )
{
  for ( size_t i = 0; i < c->lines->count; i++ )
  {
    ConsoleLine_t* line = (ConsoleLine_t*)d_ArrayGet( c->lines, i );
    d_StringDestroy( line->text );
  }
  d_ArrayDestroy( c->lines );
  c->lines = NULL;
}

void ConsolePush( Console_t* c, const char* msg, aColor_t color )
{
  /* If at capacity, destroy the oldest line and remove it */
  if ( c->lines->count >= CONSOLE_MAX_LINES )
  {
    ConsoleLine_t* oldest = (ConsoleLine_t*)d_ArrayGet( c->lines, 0 );
    d_StringDestroy( oldest->text );
    d_ArrayRemove( c->lines, 0 );

    /* Adjust scroll so it doesn't point past removed line */
    if ( c->scroll_offset > 0 )
      c->scroll_offset--;
  }

  ConsoleLine_t line;
  line.text  = d_StringInit();
  line.color = color;
  d_StringAppend( line.text, msg, 0 );

  d_ArrayAppend( c->lines, &line );

  /* If scrolled up, bump offset so the view stays pinned */
  if ( c->scroll_offset > 0 )
    c->scroll_offset++;
}

void ConsolePushF( Console_t* c, aColor_t color, const char* fmt, ... )
{
  dString_t* buf = d_StringInit();
  va_list args;

  va_start( args, fmt );
  char tmp[512];
  vsnprintf( tmp, sizeof( tmp ), fmt, args );
  va_end( args );

  d_StringAppend( buf, tmp, 0 );
  ConsolePush( c, d_StringPeek( buf ), color );
  d_StringDestroy( buf );
}

void ConsoleScroll( Console_t* c, int delta )
{
  int max_offset = (int)c->lines->count - CONSOLE_VISIBLE;
  if ( max_offset < 0 ) max_offset = 0;

  c->scroll_offset += delta;

  if ( c->scroll_offset < 0 ) c->scroll_offset = 0;
  if ( c->scroll_offset > max_offset ) c->scroll_offset = max_offset;
}

void ConsoleDraw( Console_t* c, aRectf_t rect )
{
  if ( c->lines->count == 0 )
    return;

  int total = (int)c->lines->count;
  int has_scrollbar = ( total > CONSOLE_VISIBLE );

  /* Text area (shrink if scrollbar present) */
  float text_w = has_scrollbar ? rect.w - CON_SB_W - 4 : rect.w;

  aTextStyle_t ts = a_default_text_style;
  ts.bg    = (aColor_t){ 0, 0, 0, 0 };
  ts.scale = CON_SCALE;
  ts.align = TEXT_ALIGN_LEFT;

  /* How many lines can we actually show */
  int visible = CONSOLE_VISIBLE;
  if ( total < visible )
    visible = total;

  /* Draw bottom-aligned: newest line at bottom of rect */
  float base_y = rect.y + rect.h - CON_PAD_Y - CON_LINE_H;
  float x      = rect.x + CON_PAD_X;

  for ( int i = 0; i < visible; i++ )
  {
    /* Walk backwards from newest, offset by scroll */
    int idx = total - 1 - c->scroll_offset - i;
    if ( idx < 0 ) break;

    ConsoleLine_t* line = (ConsoleLine_t*)d_ArrayGet( c->lines, (size_t)idx );
    float y = base_y - ( i * CON_LINE_H );

    /* Prefix */
    ts.fg = (aColor_t){ 120, 120, 120, 255 };
    a_DrawText( "> ", (int)x, (int)y, ts );

    /* Message */
    ts.fg = line->color;
    a_DrawText( d_StringPeek( line->text ),
                (int)( x + 16.0f ), (int)y, ts );
  }

  /* Scrollbar */
  if ( has_scrollbar )
  {
    int max_offset = total - CONSOLE_VISIBLE;
    float track_x = rect.x + rect.w - CON_SB_W - 2;
    float track_y = rect.y + CON_PAD_Y;
    float track_h = rect.h - CON_PAD_Y * 2;

    /* Track */
    a_DrawFilledRect( (aRectf_t){ track_x, track_y, CON_SB_W, track_h },
                      (aColor_t){ 20, 20, 20, 200 } );

    /* Thumb — size proportional to visible/total, position from scroll */
    float thumb_h = ( (float)CONSOLE_VISIBLE / total ) * track_h;
    if ( thumb_h < 12.0f ) thumb_h = 12.0f;

    /* scroll_offset 0 = bottom, max_offset = top */
    float scroll_pct = ( max_offset > 0 )
                       ? (float)c->scroll_offset / max_offset
                       : 0;
    float thumb_y = track_y + ( 1.0f - scroll_pct ) * ( track_h - thumb_h );

    a_DrawFilledRect( (aRectf_t){ track_x, thumb_y, CON_SB_W, thumb_h },
                      (aColor_t){ 80, 80, 80, 255 } );
    a_DrawRect( (aRectf_t){ track_x, thumb_y, CON_SB_W, thumb_h },
                (aColor_t){ 140, 140, 140, 255 } );
  }
}

void ConsoleClear( Console_t* c )
{
  for ( size_t i = 0; i < c->lines->count; i++ )
  {
    ConsoleLine_t* line = (ConsoleLine_t*)d_ArrayGet( c->lines, i );
    d_StringDestroy( line->text );
  }
  d_ArrayClear( c->lines );
  c->scroll_offset = 0;
}
