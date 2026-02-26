#include <stdio.h>
#include <Archimedes.h>

#include "main_menu.h"

static void s1_Logic( float );
static void s1_Draw( float );

void Scene1Init( void )
{
  app.delegate.logic = s1_Logic;
  app.delegate.draw  = s1_Draw;
}

static void s1_Logic( float dt )
{
  a_DoInput();
  
  if ( app.keyboard[ SDL_SCANCODE_ESCAPE ] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    MainMenuInit();
  }

  //a_DoWidget();
}

static void s1_Draw( float dt )
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
  
  a_DrawText( "New Scene", 500, 250, fps_style );

  //a_DrawWidgets();
}

