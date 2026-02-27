/*
 * world_editor/we_Edit.c:
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

static void we_EditLogic( float dt );
static void we_EditDraw( float dt );

char* wem_strings[WEM_MAX+1] =
{
  "WEM_NONE",
  "WEM_BRUSH",
  "WEM_COPY",
  "WEM_PASTE",
  "WEM_MASS_CHANGE",
  "WEM_SELECT",
  "WEM_MAX"
};

void we_Edit( void )
{
  app.delegate.logic = we_EditLogic;
  app.delegate.draw  = we_EditDraw;

  app.active_widget = a_GetWidget( "generation_menu" );
  aContainerWidget_t* container = a_GetContainerFromWidget( "generation_menu" );
  app.active_widget->hidden = 1;

  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];
    current->hidden = 1;

  }
}

static void we_EditLogic( float dt )
{
  a_DoInput();
  
  a_DoWidget();
}

static void we_EditDraw( float dt )
{

  a_DrawWidgets();
}

