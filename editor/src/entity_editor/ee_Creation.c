/*
 * entity_editor/ee_Creation.c:
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

static void ee_CreationLogic( float );
static void ee_CreationDraw( float );

void ee_Creation( void )
{
  app.delegate.logic = ee_CreationLogic;
  app.delegate.draw  = ee_CreationDraw;

}

static void ee_CreationLogic( float dt )
{
  a_DoInput();

  if ( app.keyboard[ SDL_SCANCODE_ESCAPE ] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    
    e_EntityEditorInit();
  }
  
  a_DoWidget();
}

static void ee_CreationDraw( float dt )
{
  aRectf_t creation_menu_bg = (aRectf_t){ .x = 20, .y = 105, .w = 1052, .h = 537 };
  a_DrawFilledRect( creation_menu_bg, master_colors[APOLLO_PALETE][APOLLO_RED_2] );

  a_DrawWidgets();
}

