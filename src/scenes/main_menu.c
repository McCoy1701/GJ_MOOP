#include <stdio.h>
#include <Archimedes.h>

#include "scene_1.h"

static void mm_Logic( float );
static void mm_Draw( float );

static void play_button( void );
static void load_button( void );
static void settings_button( void );
static void quit_button( void );

void MainMenuInit( void )
{
  app.delegate.logic = mm_Logic;
  app.delegate.draw  = mm_Draw;
  
  app.options.scale_factor = 1;
  
  a_WidgetsInit( "resources/widgets/main_menu.auf" );
  app.active_widget = a_GetWidget( "main_menu" );

  aContainerWidget_t* container = a_GetContainerFromWidget( "main_menu" );
  for ( int i = 0; i < container->num_components; i++ )
  {
    aWidget_t* current = &container->components[i];
    if ( strncmp( current->name, "play", MAX_NAME_LENGTH ) == 0 )
    {
      current->action = play_button;
    }
    
    if ( strncmp( current->name, "load", MAX_NAME_LENGTH ) == 0 )
    {
      current->action = load_button;
    }
    
    if ( strncmp( current->name, "settings", MAX_NAME_LENGTH ) == 0 )
    {
      current->action = settings_button;
    }
    
    if ( strncmp( current->name, "quit", MAX_NAME_LENGTH ) == 0 )
    {
      current->action = quit_button;
    }
  }
}

static void mm_Logic( float dt )
{
  a_DoInput();
  
  if ( app.keyboard[ SDL_SCANCODE_ESCAPE ] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    app.running = 0;
  }

  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/main_menu.auf" );
  }

  a_DoWidget();
}

static void mm_Draw( float dt )
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

static void play_button( void )
{
  Scene1Init();
  a_WidgetCacheFree();
}

static void load_button( void )
{
}

static void settings_button( void )
{
}

static void quit_button( void )
{
}

