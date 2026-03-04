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
#include "world.h"
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
  int width  = 0, height = 0;
  int tile_w = 0, tile_h = 0;

  aContainerWidget_t* container =
    a_GetContainerFromWidget( "generation_menu" );
  
  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];

    if ( strcmp( current->name, "width" ) == 0 )
    {
      aInputWidget_t* inp = (aInputWidget_t*)current->data;
      width = atoi( inp->text );
    }
    
    if ( strcmp( current->name, "height" ) == 0 )
    {
      aInputWidget_t* inp = (aInputWidget_t*)current->data;
      height = atoi( inp->text );
    }
    
    if ( strcmp( current->name, "tile_w" ) == 0 )
    {
      aInputWidget_t* inp = (aInputWidget_t*)current->data;
      tile_w = atoi( inp->text );
    }
    
    if ( strcmp( current->name, "tile_h" ) == 0 )
    {
      aInputWidget_t* inp = (aInputWidget_t*)current->data;
      tile_h = atoi( inp->text );
    }
  }

  g_map = WorldCreate( width, height, 16, 16 );
}

