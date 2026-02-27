/*
 * world_editor.c:
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
#include "entity_editor.h"
#include "items_editor.h"
#include "world_editor.h"

static void e_WorldEditorLogic( float );
static void e_WorldEditorDraw( float );

aWorld_t* map = NULL;

static aPoint2f_t selected_pos;
static aPoint2f_t highlighted_pos;
char* pos_text;

static int originx = 0;
static int originy = 0;

void e_WorldEditorInit( void )
{
  aWidget_t* w;
  app.delegate.logic = e_WorldEditorLogic;
  app.delegate.draw  = e_WorldEditorDraw;

  pos_text = malloc( sizeof(char) * 50 );

  selected_pos = ( aPoint2f_t ){ .x = 0, .y = 0 };

  snprintf( pos_text, 50, "%f,%f\n", selected_pos.x, selected_pos.y );

  a_WidgetsInit( "resources/widgets/editor/world.json" );
  app.active_widget = a_GetWidget( "tab_bar" );

  aContainerWidget_t* tab_container = a_GetContainerFromWidget( "tab_bar" );
  for ( int i = 0; i < tab_container->num_components; i++ )
  {
    aWidget_t* current = &tab_container->components[i];

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

  aContainerWidget_t* world_menu_container = a_GetContainerFromWidget("world_menu_bar");
  for ( int i = 0; i < world_menu_container->num_components; i++ )
  {
    aWidget_t* current = &world_menu_container->components[i];

    if ( strcmp( current->name, "creation" ) == 0 )
    {
      current->action = we_Creation;
    }

    if ( strcmp( current->name, "edit" ) == 0 )
    {
      current->action = we_Edit;
    }

    if ( strcmp( current->name, "save" ) == 0 )
    {
      current->action = we_Save;
    }

    if ( strcmp( current->name, "load" ) == 0 )
    {
      current->action = we_Load;
    }
  }

}

static void e_WorldEditorLogic( float dt )
{
  a_DoInput();
  
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    EditorInit();
  }

  a_DoWidget();
}

static void e_WorldEditorDraw( float dt )
{

  a_DrawWidgets();
}

