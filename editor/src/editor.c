/*
 * editor.c:
 *
 * Copyright (c) 2025 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "ed_defines.h"
#include "ed_structs.h"

#include "entity_editor.h"
#include "items_editor.h"
#include "tile.h"
#include "utils.h"
#include "world.h"
#include "world_editor.h"
#include "npc_editor.h"
#include "item_catalog.h"

static void ed_Logic( float );
static void ed_Draw( float );

static void pending_world_action( void );
static void pending_item_action( void );
static void pending_entity_action( void );
static void pending_npc_action( void );

aColor_t master_colors[MAX_COLOR_GROUPS][48] = {0};
Tileset_t* g_tile_sets[MAX_TILESETS] = {NULL};
dArray_t* g_map_filenames  = NULL;

static void (*pending_init)(void) = NULL;

void EditorInit( void )
{
  app.delegate.logic = ed_Logic;
  app.delegate.draw  = ed_Draw;

  app.g_viewport = (aRectf_t){0};
  pending_init = NULL;

  if ( g_map_filenames == NULL )
  {
    char temp_filename[1024];
    g_map_filenames = d_ArrayInit( 10, sizeof(temp_filename) );
    FindMapFiles( "..", g_map_filenames );
  }

  e_LoadColorPalette( master_colors, "resources/colorpalette/colors.hex" );
  EdItemCatalogLoad();

  g_tile_sets[LVL1_TILESET] = e_TilesetCreate(
    "../resources/assets/tiles/level01tilemap.png", 16, 16 );
  g_current_tileset = LVL1_TILESET;

  a_WidgetsInit( "resources/widgets/editor/editor.auf" );

  app.active_widget = a_GetWidget( "tab_bar" );

  aContainerWidget_t* container = a_GetContainerFromWidget( "tab_bar" );

  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];

    if ( strcmp( current->name, "world" ) == 0 )
      current->action = pending_world_action;

    if ( strcmp( current->name, "item" ) == 0 )
      current->action = pending_item_action;

    if ( strcmp( current->name, "entity" ) == 0 )
      current->action = pending_entity_action;

    if ( strcmp( current->name, "npc" ) == 0 )
      current->action = pending_npc_action;
  }
}

static void pending_world_action( void )  { pending_init = e_WorldEditorInit; }
static void pending_item_action( void )   { pending_init = e_ItemEditorInit; }
static void pending_entity_action( void ) { pending_init = e_EntityEditorInit; }
static void pending_npc_action( void )    { pending_init = e_NPCEditorInit; }

static void ed_Logic( float dt )
{
  a_DoInput();

  if ( app.keyboard[ SDL_SCANCODE_ESCAPE ] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    app.running = 0;
  }

  if ( app.keyboard[A_F1] == 1 )
  {
    app.keyboard[A_F1] = 0;
    a_WidgetsInit( "resources/widgets/editor/editor.auf" );
  }

  a_DoWidget();

  if ( pending_init )
  {
    void (*fn)(void) = pending_init;
    pending_init = NULL;
    fn();
  }
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
  
  a_DrawWidgets();
}

void EditorDestroy( void )
{
  for ( int i = 0; i < MAX_TILESETS; i++ )
  {
    if ( g_tile_sets[i] != NULL )
    {
      e_TilesetDestroy( g_tile_sets[i] );
    }
  }

  d_ArrayDestroy( g_map_filenames );
}

