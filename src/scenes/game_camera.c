#include <Archimedes.h>

#include "game_camera.h"
#include "player.h"
#include "movement.h"
#include "transitions.h"
#include "visibility.h"

extern Player_t player;

static GameCamera_t* cam;

void GameCameraInit( GameCamera_t* c )
{
  cam = c;
}

void GameCameraFollow( void )
{
  cam->x = player.world_x + PlayerShakeOX();
  cam->y = player.world_y + PlayerShakeOY();
}

int GameCameraIntro( float dt )
{
  if ( !TransitionIntroActive() ) return 0;

  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    TransitionIntroSkip();
  }
  else
  {
    TransitionIntroUpdate( dt );
  }

  cam->half_h = TransitionGetViewportH();
  cam->x = player.world_x;
  cam->y = player.world_y;

  {
    int vr, vc;
    PlayerGetTile( &vr, &vc );
    VisibilityUpdate( vr, vc );
  }

  return 1;
}
