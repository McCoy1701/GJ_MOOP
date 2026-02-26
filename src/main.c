#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <Archimedes.h>
#include "defines.h"
#include "main_menu.h"

aSoundEffect_t sfx_click;
aSoundEffect_t sfx_move;

void aMainloop( void )
{
  float dt = a_GetDeltaTime();
  a_TimerStart( app.time.FPS_cap_timer );
  a_GetFPS();
  a_PrepareScene();
  
  app.delegate.logic( dt );
  app.delegate.draw( dt );
  
  a_PresentScene();
  app.time.frames++;
  
  if ( app.options.frame_cap )
  {
    int frame_tick = a_TimerGetTicks( app.time.FPS_cap_timer );
    if ( frame_tick < LOGIC_RATE )
    {
      SDL_Delay( LOGIC_RATE - frame_tick );
    }
  }
}

int main( void )
{
  a_Init( SCREEN_WIDTH, SCREEN_HEIGHT, "Archimedes" );

  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );
  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_move );

  MainMenuInit();

  #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop( aMainloop, 0, 1 );
  #endif

  #ifndef __EMSCRIPTEN__
    while( app.running ) {
      aMainloop();
    }
  #endif
  
  a_Quit();

  return 0;
}

