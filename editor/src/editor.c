/*
 * editor.c:
 *
 * Copyright (c) 2025 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <Archimedes.h>

#include "ed_defines.h"
#include "ed_structs.h"

#include "entity_editor.h"
#include "items_editor.h"
#include "world_editor.h"

static void ed_Logic( float );
static void ed_Draw( float );

GlyphArray_t* game_glyphs = NULL;
aColor_t master_colors[MAX_COLOR_GROUPS][48] = {0};

void EditorInit( void )
{
  app.delegate.logic = ed_Logic;
  app.delegate.draw  = ed_Draw;
}

static void ed_Logic( float dt )
{
  a_DoInput();
  
  if ( app.keyboard[ SDL_SCANCODE_ESCAPE ] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    app.running = 0;
  }
  
  a_WidgetsInit( "resources/widgets/editor/editor.json" );

  app.active_widget = a_GetWidget( "tab_bar" );

  aContainerWidget_t* container = 
    ( aContainerWidget_t* )app.active_widget->data;

  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];

    if ( strcmp( current->name, "world" ) == 0 )
    {
      current->action = e_WorldEditorInit;
    }

    if ( strcmp( current->name, "item" ) == 0 )
    {
      current->action = e_ItemEditorInit;
    }

    if ( strcmp( current->name, "entity" ) == 0 )
    {
      current->action = e_EntityEditorInit;
    }
  }


  //a_DoWidget();
}

static void ed_Draw( float dt )
{
  aColor_t color_something = { .r = 0, .g = 0, .b = 255, .a = 255 };
  aRectf_t rect_something = { .x = 100, .y = 100, .w = 32, .h = 32 };
  a_DrawFilledRect( rect_something, color_something );
  
  char fps_text[MAX_NAME_LENGTH];
  snprintf(fps_text, MAX_NAME_LENGTH, "%f", app.time.avg_FPS );
  
  aTextStyle_t fps_style = {
    .type = FONT_CODE_PAGE_437,
    .fg = white,
    .bg = black,
    .align = TEXT_ALIGN_CENTER,
    .wrap_width = 0,
    .scale = 1.0f,
    .padding = 0
  };

  a_DrawText( fps_text, 600, 100, fps_style );

  //a_DrawWidgets();
}

