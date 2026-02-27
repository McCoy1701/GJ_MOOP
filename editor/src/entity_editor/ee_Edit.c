/*
 * entity_editor/ee_Edit.c:
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

static void ee_EditLogic( float dt );
static void ee_EditDraw( float dt );

void ee_Edit( void )
{
  app.delegate.logic = ee_EditLogic;
  app.delegate.draw  = ee_EditDraw;

}

static void ee_EditLogic( float dt )
{
  a_DoInput();
  
  if ( app.keyboard[ SDL_SCANCODE_ESCAPE ] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    
    e_EntityEditorInit();
  }

  a_DoWidget();
}

static void ee_EditDraw( float dt )
{
  aRectf_t background_rect = (aRectf_t){ .x = 20, .y = 105,
                                       .w = 1052, .h = 537 };
  a_DrawFilledRect( background_rect,
                    master_colors[APOLLO_PALETE][APOLLO_RED_2] ); //Background
  
  a_DrawWidgets();
}

