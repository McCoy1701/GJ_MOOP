/*
 * world_editor/we_Load.c:
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

static void wel_LoadLogic( float dt );
static void wel_LoadDraw( float dt );

void we_Load( void )
{
  app.delegate.logic = wel_LoadLogic;
  app.delegate.draw  = wel_LoadDraw;
  
  app.active_widget = a_GetWidget( "load_menu" );
  aContainerWidget_t* container =
    a_GetContainerFromWidget( "load_menu" );

  app.active_widget->hidden = 0;

  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];
    current->hidden = 0;

    if ( strcmp( current->name, "load_yes" ) == 0 )
    {
      current->action = wel_LoadYes;
    }
    
    if ( strcmp( current->name, "Load_no" ) == 0 )
    {
      current->action = wel_LoadNo;
    }
  }
}

static void wel_LoadLogic( float dt )
{
  a_DoInput();

  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    e_WorldEditorInit();
  }
  
  a_DoWidget();
}

static void wel_LoadDraw( float dt )
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

  a_DrawText( "Load?", 635, 270, fps_style );

  a_DrawWidgets();
}

void wel_LoadYes( void )
{
  EditorInit();
}

void wel_LoadNo( void )
{
  EditorInit();
}

