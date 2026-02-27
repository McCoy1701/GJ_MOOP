#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "settings.h"
#include "main_menu.h"

static void st_Logic( float );
static void st_Draw( float );

static int st_leaving = 0;

static void st_ApplySettings( void )
{
  aContainerWidget_t* c = a_GetContainerFromWidget( "settings_panel" );

  for ( int i = 0; i < c->num_components; i++ )
  {
    aWidget_t* w = &c->components[i];

    if ( strncmp( w->name, "gfx_mode", MAX_NAME_LENGTH ) == 0 )
    {
      aSelectWidget_t* sel = (aSelectWidget_t*)w->data;
      settings.gfx_mode = sel->value;
    }
  }
}

static void st_Leave( void )
{
  st_ApplySettings();
  st_leaving = 1;
  a_WidgetCacheFree();
  MainMenuInit();
}

static void back_button( void )
{
  st_Leave();
}

static void st_SyncWidgets( void )
{
  aContainerWidget_t* c = a_GetContainerFromWidget( "settings_panel" );

  for ( int i = 0; i < c->num_components; i++ )
  {
    aWidget_t* w = &c->components[i];

    if ( strncmp( w->name, "back", MAX_NAME_LENGTH ) == 0 )
      w->action = back_button;

    if ( strncmp( w->name, "gfx_mode", MAX_NAME_LENGTH ) == 0 )
    {
      aSelectWidget_t* sel = (aSelectWidget_t*)w->data;
      sel->value = settings.gfx_mode;
    }
  }
}

void SettingsInit( void )
{
  app.delegate.logic = st_Logic;
  app.delegate.draw  = st_Draw;

  app.options.scale_factor = 1;

  st_leaving = 0;
  a_WidgetsInit( "resources/widgets/settings.auf" );
  app.active_widget = a_GetWidget( "settings_panel" );
  st_SyncWidgets();
}

static void st_Logic( float dt )
{
  a_DoInput();

  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    st_Leave();
    return;
  }

  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/settings.auf" );
    st_SyncWidgets();
  }

  a_DoWidget();
  if ( !st_leaving )
    st_ApplySettings();
}

#define HIGHLIGHT_COLOR  (aColor_t){ 218, 175, 32, 255 }

static void st_Draw( float dt )
{
  a_DrawWidgets();

  /* Draw highlight around focused widget */
  aContainerWidget_t* c = a_GetContainerFromWidget( "settings_panel" );
  if ( c->focus_index >= 0 && c->focus_index < c->num_components )
  {
    aWidget_t* focused = &c->components[c->focus_index];
    a_DrawRect( focused->rect, HIGHLIGHT_COLOR );
  }
}
