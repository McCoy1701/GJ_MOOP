/*
 * world_editor/we_Creation.c:
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

static void we_CreationLogic( float );
static void we_CreationDraw( float );

void we_Creation( void )
{
  app.delegate.logic = we_CreationLogic;
  app.delegate.draw  = we_CreationDraw;


  app.active_widget = a_GetWidget( "generation_menu" );
  aContainerWidget_t* container =
    a_GetContainerFromWidget( "generation_menu" );

  app.active_widget->hidden = 0;

  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];
    current->hidden = 0;

    if ( strcmp( current->name, "generate" ) == 0 )
    {
      current->action = wec_GenerateWorld;
    }
  }

}

static void we_CreationLogic( float dt )
{
  a_DoInput();

  if ( app.keyboard[ SDL_SCANCODE_ESCAPE ] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    
    e_WorldEditorInit();
  }

  a_DoWidget();

}

static void we_CreationDraw( float dt )
{
  a_DrawWidgets();

}

void wec_GenerateWorld( void )
{
}

