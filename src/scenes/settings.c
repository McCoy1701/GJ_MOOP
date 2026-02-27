#include <stdio.h>
#include <Archimedes.h>

#include "defines.h"
#include "main_menu.h"

static void st_Logic( float );
static void st_Draw( float );

static void back_button( void );

static void st_BindActions( void )
{
  aContainerWidget_t* container = a_GetContainerFromWidget( "settings_panel" );
  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];
    if ( strncmp( current->name, "back", MAX_NAME_LENGTH ) == 0 )
      current->action = back_button;
  }
}

void SettingsInit( void )
{
  app.delegate.logic = st_Logic;
  app.delegate.draw  = st_Draw;

  app.options.scale_factor = 1;

  a_WidgetsInit( "resources/widgets/settings.auf" );
  app.active_widget = a_GetWidget( "settings_panel" );
  st_BindActions();
}

static void st_Logic( float dt )
{
  a_DoInput();

  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    a_WidgetCacheFree();
    MainMenuInit();
    return;
  }

  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/settings.auf" );
    st_BindActions();
  }

  a_DoWidget();
}

static void st_Draw( float dt )
{
  a_DrawWidgets();
}

static void back_button( void )
{
  a_WidgetCacheFree();
  MainMenuInit();
}
