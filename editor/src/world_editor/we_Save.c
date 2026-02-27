/*
 * world_editor/we_Save.c:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "ed_defines.h"
#include "ed_structs.h"

#include "editor.h"
#include "world_editor.h"

static void wes_SaveLogic( float dt );
static void wes_SaveDraw( float dt );

void we_Save( void )
{
  app.delegate.logic = wes_SaveLogic;
  app.delegate.draw  = wes_SaveDraw;
  
  app.active_widget = a_GetWidget( "save_menu" );
  aContainerWidget_t* container =
    a_GetContainerFromWidget( "save_menu" );

  app.active_widget->hidden = 0;

  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];
    current->hidden = 0;

    if ( strcmp( current->name, "save_yes" ) == 0 )
    {
      current->action = wes_SaveYes;
    }
    
    if ( strcmp( current->name, "save_no" ) == 0 )
    {
      current->action = wes_SaveNo;
    }
  }
}

static void wes_SaveLogic( float dt )
{
  a_DoInput();

  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    e_WorldEditorInit();
  }
  
  a_DoWidget();
}

static void wes_SaveDraw( float dt )
{
  aTextStyle_t fps_style = {
    .type = FONT_CODE_PAGE_437,
    .fg = white,
    .bg = black,
    .align = TEXT_ALIGN_CENTER,
    .wrap_width = 0,
    .scale = 1.0f,
    .padding = 0
  };

  a_DrawText( "Save?", 635, 270, fps_style );

  a_DrawWidgets();
}

void wes_SaveYes( void )
{
  EditorInit();
}

void wes_SaveNo( void )
{
  EditorInit();
}

