#include <stdio.h>

#include <Archimedes.h>
#include <Daedalus.h>

#include "editor.h"
#include "world_editor.h"
#include "entity_editor.h"

static void e_ItemEditorLogic( float );
static void e_ItemEditorDraw( float );

void e_ItemEditorInit( void )
{
  app.delegate.logic = e_ItemEditorLogic;
  app.delegate.draw  = e_ItemEditorDraw;
  
  a_WidgetsInit( "resources/widgets/editor/items.json" );
  
  app.active_widget = a_GetWidget( "tab_bar" );

  aContainerWidget_t* container = a_GetContainerFromWidget( "tab_bar" );
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
  
}

static void e_ItemEditorLogic( float dt )
{
  a_DoInput();
  
  if ( app.keyboard[ SDL_SCANCODE_ESCAPE ] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    EditorInit();
  }

  a_DoWidget();
}

static void e_ItemEditorDraw( float dt )
{
  aRectf_t test_rect_0 = (aRectf_t){ .x = 100, .y = 100, .w = 32, .h = 32 };
  a_DrawFilledRect( test_rect_0, blue );
  aRectf_t test_rect_1 = (aRectf_t){ .x = 300, .y = 300, .w = 32, .h = 32 };
  a_DrawFilledRect( test_rect_1, green );

  a_DrawWidgets();
}

void e_DestroyItemEditor( void )
{
}

