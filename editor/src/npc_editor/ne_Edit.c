/*
 * npc_editor/ne_Edit.c:
 *
 * Custom text field system and editable modal for the NPC dialogue editor.
 * No Archimedes WT_INPUT — built from scratch using app.input_text and
 * app.keyboard[].
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "npc_editor.h"

/* ---- Text field state ---- */

static NETextField_t g_field;
static dString_t*    g_field_target  = NULL;  /* dString to write back to */
static int*          g_field_int_target = NULL; /* int field to write back to */
static int           g_field_is_int  = 0;
static float         g_cursor_blink  = 0.0f;

/* ---- Field helpers ---- */

void ne_FieldClear( void )
{
  memset( &g_field, 0, sizeof( g_field ) );
  g_field_target     = NULL;
  g_field_int_target = NULL;
  g_field_is_int     = 0;
}

int ne_FieldIsActive( void )
{
  return g_field.active;
}

int ne_FieldIsTarget( dString_t* target )
{
  return g_field.active && !g_field_is_int && g_field_target == target;
}

int ne_FieldIsIntTarget( int* target )
{
  return g_field.active && g_field_is_int && g_field_int_target == target;
}

void ne_FieldActivate( dString_t* target )
{
  ne_FieldClear();
  g_field.active = 1;
  g_field_target = target;

  const char* s = d_StringPeek( target );
  if ( s )
  {
    int len = (int)strlen( s );
    if ( len >= NE_FIELD_BUF ) len = NE_FIELD_BUF - 1;
    memcpy( g_field.buf, s, len );
    g_field.buf[len] = '\0';
    g_field.len    = len;
    g_field.cursor = len;
  }

  SDL_StartTextInput();
}

void ne_FieldActivateInt( int* target )
{
  ne_FieldClear();
  g_field.active     = 1;
  g_field_int_target = target;
  g_field_is_int     = 1;

  snprintf( g_field.buf, NE_FIELD_BUF, "%d", *target );
  g_field.len    = (int)strlen( g_field.buf );
  g_field.cursor = g_field.len;

  SDL_StartTextInput();
}

/* Returns 1 if the field was committed (value changed) */
int ne_FieldCommit( void )
{
  if ( !g_field.active ) return 0;

  g_field.active = 0;
  SDL_StopTextInput();

  if ( g_field_is_int && g_field_int_target )
  {
    *g_field_int_target = atoi( g_field.buf );
    g_field_int_target  = NULL;
    return 1;
  }

  if ( g_field_target )
  {
    d_StringSet( g_field_target, g_field.buf );
    g_field_target = NULL;
    return 1;
  }

  return 0;
}

void ne_FieldCancel( void )
{
  g_field.active     = 0;
  g_field_target     = NULL;
  g_field_int_target = NULL;
  SDL_StopTextInput();
}

/* ---- Field logic: call each frame while active ---- */

void ne_FieldLogic( float dt )
{
  if ( !g_field.active ) return;

  g_cursor_blink += dt;
  if ( g_cursor_blink > 1.0f ) g_cursor_blink -= 1.0f;

  /* Text input from SDL */
  if ( app.input_text[0] != '\0' )
  {
    int ch_len = (int)strlen( app.input_text );
    if ( g_field.len + ch_len < NE_FIELD_BUF )
    {
      /* Insert at cursor */
      memmove( g_field.buf + g_field.cursor + ch_len,
               g_field.buf + g_field.cursor,
               g_field.len - g_field.cursor + 1 );
      memcpy( g_field.buf + g_field.cursor, app.input_text, ch_len );
      g_field.cursor += ch_len;
      g_field.len    += ch_len;
    }
    memset( app.input_text, 0, sizeof( app.input_text ) );
  }

  /* Backspace */
  if ( app.keyboard[SDL_SCANCODE_BACKSPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_BACKSPACE] = 0;
    if ( g_field.cursor > 0 )
    {
      memmove( g_field.buf + g_field.cursor - 1,
               g_field.buf + g_field.cursor,
               g_field.len - g_field.cursor + 1 );
      g_field.cursor--;
      g_field.len--;
    }
  }

  /* Delete */
  if ( app.keyboard[SDL_SCANCODE_DELETE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_DELETE] = 0;
    if ( g_field.cursor < g_field.len )
    {
      memmove( g_field.buf + g_field.cursor,
               g_field.buf + g_field.cursor + 1,
               g_field.len - g_field.cursor );
      g_field.len--;
    }
  }

  /* Left / Right arrow */
  if ( app.keyboard[SDL_SCANCODE_LEFT] == 1 )
  {
    app.keyboard[SDL_SCANCODE_LEFT] = 0;
    if ( g_field.cursor > 0 ) g_field.cursor--;
  }
  if ( app.keyboard[SDL_SCANCODE_RIGHT] == 1 )
  {
    app.keyboard[SDL_SCANCODE_RIGHT] = 0;
    if ( g_field.cursor < g_field.len ) g_field.cursor++;
  }

  /* Home / End */
  if ( app.keyboard[SDL_SCANCODE_HOME] == 1 )
  {
    app.keyboard[SDL_SCANCODE_HOME] = 0;
    g_field.cursor = 0;
  }
  if ( app.keyboard[SDL_SCANCODE_END] == 1 )
  {
    app.keyboard[SDL_SCANCODE_END] = 0;
    g_field.cursor = g_field.len;
  }
}

/* ---- Draw a text field row ---- */

#define FIELD_H     18
#define LABEL_W    175
#define GLYPH_W      9

static aTextStyle_t field_style( void )
{
  return (aTextStyle_t){
    .type       = FONT_CODE_PAGE_437,
    .fg         = white,
    .bg         = black,
    .align      = TEXT_ALIGN_LEFT,
    .wrap_width = 0,
    .scale      = 1.0f,
    .padding    = 0
  };
}

/*
 * Draw one labeled field row. Returns the y for the next row.
 * If the field is active (being edited), draw the edit buffer with cursor.
 * If clicked, returns 1 via *clicked.
 */
int ne_DrawFieldRow( const char* label, const char* value,
                     int is_active, int x, int y, int w, int* clicked )
{
  aTextStyle_t ts = field_style();

  /* Value area dimensions */
  int vx = x + LABEL_W;
  int vw = w - LABEL_W;
  int chars_per_line = vw / GLYPH_W;
  if ( chars_per_line < 1 ) chars_per_line = 1;

  /* Calculate row height based on content */
  int num_lines = 1;
  {
    const char* measure = is_active ? g_field.buf : value;
    if ( measure && measure[0] )
    {
      int text_len = (int)strlen( measure );
      num_lines = ( text_len + chars_per_line - 1 ) / chars_per_line;
      if ( num_lines < 1 ) num_lines = 1;
    }
  }
  int row_h = FIELD_H * num_lines;

  aRectf_t val_rect = { (float)vx - 2, (float)y - 1,
                         (float)vw + 4, (float)row_h + 2 };

  /* Label (vertically aligned to top of field) */
  ts.fg = (aColor_t){ 160, 160, 160, 255 };
  a_DrawText( label, x, y + 1, ts );

  if ( is_active )
  {
    /* Active field: wrapped, with cursor tracking across lines */
    a_DrawFilledRect( val_rect, (aColor_t){ 40, 50, 70, 255 } );
    a_DrawRect( val_rect, (aColor_t){ 100, 140, 200, 255 } );

    ts.fg = white;
    ts.wrap_width = vw;
    a_DrawText( g_field.buf, vx, y + 1, ts );

    /* Blinking cursor — calculate position with wrapping */
    if ( g_cursor_blink < 0.5f )
    {
      int cur_line = g_field.cursor / chars_per_line;
      int cur_col  = g_field.cursor % chars_per_line;
      int cx = vx + cur_col * GLYPH_W;
      int cy = y + cur_line * FIELD_H;
      a_DrawFilledRect( (aRectf_t){ (float)cx, (float)cy,
                                     2.0f, (float)FIELD_H },
                        (aColor_t){ 255, 255, 255, 220 } );
    }

    return y + row_h + 4;
  }
  else
  {
    /* Hover highlight */
    if ( app.mouse.x >= val_rect.x && app.mouse.x < val_rect.x + val_rect.w
         && app.mouse.y >= val_rect.y && app.mouse.y < val_rect.y + val_rect.h )
    {
      a_DrawFilledRect( val_rect, (aColor_t){ 35, 40, 55, 200 } );

      if ( clicked && app.mouse.button == 1 && app.mouse.state == 1 )
        *clicked = 1;
    }

    ts.fg = (aColor_t){ 220, 220, 220, 255 };
    if ( !value || !value[0] )
    {
      ts.fg = (aColor_t){ 80, 80, 80, 255 };
      a_DrawText( "(empty)", vx, y + 1, ts );
    }
    else
    {
      /* Draw wrapped text — set wrap_width so a_DrawText wraps */
      ts.wrap_width = vw;
      a_DrawText( value, vx, y + 1, ts );
    }
  }

  return y + row_h + 4;
}

/* Draw a toggle row (is_start). Returns next y. */
int ne_DrawToggleRow( const char* label, int value,
                      int x, int y, int w __attribute__((unused)),
                      int* clicked )
{
  aTextStyle_t ts = field_style();
  ts.fg = (aColor_t){ 160, 160, 160, 255 };
  a_DrawText( label, x, y + 1, ts );

  int vx = x + LABEL_W;
  aRectf_t box = { (float)vx - 2, (float)y - 1, 60.0f, (float)FIELD_H + 2 };

  /* Hover */
  if ( app.mouse.x >= box.x && app.mouse.x < box.x + box.w
       && app.mouse.y >= box.y && app.mouse.y < box.y + box.h )
  {
    a_DrawFilledRect( box, (aColor_t){ 35, 40, 55, 200 } );
    if ( clicked && app.mouse.button == 1 && app.mouse.state == 1 )
      *clicked = 1;
  }

  ts.fg = value ? (aColor_t){ 100, 220, 100, 255 }
                : (aColor_t){ 220, 100, 100, 255 };
  a_DrawText( value ? "yes" : "no", vx, y + 1, ts );

  return y + FIELD_H + 4;
}

/* Draw a section header. Returns next y. */
int ne_DrawSectionHeader( const char* text, int x, int y )
{
  aTextStyle_t ts = field_style();
  ts.fg = (aColor_t){ 0xde, 0x9e, 0x41, 255 };
  a_DrawText( text, x, y, ts );
  return y + 20;
}

/* Draw array field with [+] and [-] buttons. Returns next y. */
int ne_DrawArrayField( const char* label, dString_t* items[], int count,
                       int max, int x, int y, int w,
                       int active_index, int* click_idx,
                       int* click_add, int* click_remove )
{
  aTextStyle_t ts = field_style();

  /* Section label with [+] */
  ts.fg = (aColor_t){ 140, 180, 220, 255 };
  a_DrawText( label, x, y + 1, ts );

  if ( count < max )
  {
    int bx = x + (int)strlen( label ) * GLYPH_W + 8;
    aRectf_t add_btn = { (float)bx, (float)y - 1, 22.0f, (float)FIELD_H + 2 };
    a_DrawFilledRect( add_btn, (aColor_t){ 40, 80, 40, 200 } );
    a_DrawRect( add_btn, (aColor_t){ 80, 160, 80, 200 } );
    ts.fg = (aColor_t){ 140, 255, 140, 255 };
    a_DrawText( "+", bx + 7, y + 1, ts );

    if ( app.mouse.x >= add_btn.x && app.mouse.x < add_btn.x + add_btn.w
         && app.mouse.y >= add_btn.y && app.mouse.y < add_btn.y + add_btn.h
         && app.mouse.button == 1 && app.mouse.state == 1 )
    {
      if ( click_add ) *click_add = 1;
    }
  }

  y += FIELD_H + 2;

  for ( int i = 0; i < count; i++ )
  {
    int is_active = ( active_index == i ) || ne_FieldIsTarget( items[i] );
    int clicked = 0;

    const char* val = d_StringPeek( items[i] );
    y = ne_DrawFieldRow( "  ", val, is_active, x, y, w, &clicked );

    if ( clicked && click_idx ) *click_idx = i;

    /* [-] button */
    int rx = x + w - 22;
    aRectf_t rm_btn = { (float)rx, (float)( y - FIELD_H - 5 ),
                         20.0f, (float)FIELD_H };
    a_DrawFilledRect( rm_btn, (aColor_t){ 80, 30, 30, 200 } );
    a_DrawRect( rm_btn, (aColor_t){ 160, 60, 60, 200 } );
    ts.fg = (aColor_t){ 255, 100, 100, 255 };
    a_DrawText( "-", rx + 6, (int)rm_btn.y + 1, ts );

    if ( app.mouse.x >= rm_btn.x && app.mouse.x < rm_btn.x + rm_btn.w
         && app.mouse.y >= rm_btn.y && app.mouse.y < rm_btn.y + rm_btn.h
         && app.mouse.button == 1 && app.mouse.state == 1 )
    {
      if ( click_remove ) *click_remove = i;
    }
  }

  return y;
}
