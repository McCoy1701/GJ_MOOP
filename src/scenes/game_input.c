#include <Archimedes.h>

#include "game_input.h"
#include "draw_utils.h"
#include "player.h"
#include "movement.h"
#include "inventory_ui.h"
#include "tile_actions.h"
#include "look_mode.h"
#include "target_mode.h"
#include "dialogue.h"
#include "dialogue_ui.h"
#include "shop_ui.h"
#include "combat.h"
#include "combat_vfx.h"
#include "game_events.h"
#include "game_turns.h"
#include "sound_manager.h"
#include "visibility.h"
#include "pause_menu.h"
#include "dungeon.h"
#include "interactive_tile.h"

extern Player_t player;

/* Viewport zoom limits */
#define ZOOM_MIN_H  20.0f
#define ZOOM_MAX_H 100.0f

static World_t*        gi_world;
static GameCamera_t*   gi_cam;
static Console_t*      gi_console;
static Enemy_t*        gi_enemies;
static int*            gi_num_enemies;
static NPC_t*          gi_npcs;
static int*            gi_num_npcs;

static int prev_mx = -1;
static int prev_my = -1;
static int mouse_moved = 0;

static int hover_row = -1, hover_col = -1;
static int turn_skipped = 0;

void GameInputInit( World_t* w, GameCamera_t* cam, Console_t* con,
                    Enemy_t* enemies, int* num_enemies,
                    NPC_t* npcs, int* num_npcs )
{
  gi_world       = w;
  gi_cam         = cam;
  gi_console     = con;
  gi_enemies     = enemies;
  gi_num_enemies = num_enemies;
  gi_npcs        = npcs;
  gi_num_npcs    = num_npcs;
  prev_mx = prev_my = -1;
  mouse_moved = 0;
  hover_row = hover_col = -1;
  turn_skipped = 0;
}

void GameInputFrameBegin( float dt )
{
  a_DoInput();
  SoundManagerUpdate( dt );
  mouse_moved = ( app.mouse.x != prev_mx || app.mouse.y != prev_my );
  prev_mx = app.mouse.x;
  prev_my = app.mouse.y;
}

int GameInputOverlays( void )
{
  if ( DialogueUILogic() ) return 1;
  if ( ShopUILogic() )     return 1;
  {
    int ta = TileActionsLogic( mouse_moved, gi_enemies, *gi_num_enemies );
    if ( ta == 2 ) { turn_skipped = 1; return 1; }
    if ( ta )      return 1;
  }
  return 0;
}

int GameInputEsc( void )
{
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] != 1 ) return 0;

  app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
  if ( TargetModeActive() )
    TargetModeExit();
  else if ( LookModeActive() )
    LookModeExit();
  else if ( !InventoryUICloseMenus() )
  {
    PauseMenuOpen();
  }
  return 0;
}

void GameInputInventory( void )
{
  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/game_scene.auf" );
  }

  InventoryUILogic( mouse_moved );

  if ( InventoryUIFocused() )
  {
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_A] = 0;
    app.keyboard[SDL_SCANCODE_D] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    app.keyboard[SDL_SCANCODE_LEFT] = 0;
    app.keyboard[SDL_SCANCODE_RIGHT] = 0;
  }
}

int GameInputTargetMode( void )
{
  int tr, tc, ci, si;
  if ( TargetModeConfirmed( &tr, &tc, &ci, &si ) )
  {
    if ( GameEventResolveTarget( ci, si, tr, tc, gi_enemies, *gi_num_enemies ) )
    {
      GameTurnsSetEnemyDelay( 0.2f );
      PlayerTickTurnsSinceHit();
      for ( int i = 0; i < *gi_num_enemies; i++ )
        if ( gi_enemies[i].alive ) gi_enemies[i].turns_since_hit++;
    }
  }

  if ( TargetModeLogic( gi_enemies, *gi_num_enemies ) )
  {
    hover_row = hover_col = -1;
    return 1;
  }
  return 0;
}

int GameInputLookMode( void )
{
  if ( LookModeLogic( mouse_moved ) )
  {
    hover_row = hover_col = -1;
    return 1;
  }

  if ( app.keyboard[SDL_SCANCODE_L] == 1 )
  {
    int pr, pc;
    GameTurnsGetPlayerTile( &pr, &pc );
    app.keyboard[SDL_SCANCODE_L] = 0;
    LookModeEnter( pr, pc );
  }
  return 0;
}

void GameInputMovement( void )
{
  if ( PlayerIsMoving() || EnemiesTurning() || InventoryUIFocused()
       || GameTurnsEnemyDelay() > 0 )
    return;

  /* Space = skip turn */
  if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    turn_skipped = 1;
    ConsolePush( gi_console, "You skip your turn.",
                 (aColor_t){ 0x81, 0x97, 0x96, 255 } );
    return;
  }

  int dr = 0, dc = 0;
  if ( app.keyboard[SDL_SCANCODE_UP] == 1 || app.keyboard[SDL_SCANCODE_W] == 1 )
  { app.keyboard[SDL_SCANCODE_UP] = 0; app.keyboard[SDL_SCANCODE_W] = 0; dc = -1; }
  else if ( app.keyboard[SDL_SCANCODE_DOWN] == 1 || app.keyboard[SDL_SCANCODE_S] == 1 )
  { app.keyboard[SDL_SCANCODE_DOWN] = 0; app.keyboard[SDL_SCANCODE_S] = 0; dc =  1; }
  else if ( app.keyboard[SDL_SCANCODE_LEFT] == 1 || app.keyboard[SDL_SCANCODE_A] == 1 )
  { app.keyboard[SDL_SCANCODE_LEFT] = 0; app.keyboard[SDL_SCANCODE_A] = 0; dr = -1; }
  else if ( app.keyboard[SDL_SCANCODE_RIGHT] == 1 || app.keyboard[SDL_SCANCODE_D] == 1 )
  { app.keyboard[SDL_SCANCODE_RIGHT] = 0; app.keyboard[SDL_SCANCODE_D] = 0; dr =  1; }

  if ( dr == 0 && dc == 0 ) return;

  int fpr, fpc;
  GameTurnsGetPlayerTile( &fpr, &fpc );
  int tr = fpr + dr;
  int tc = fpc + dc;

  Enemy_t* bump = EnemyAt( gi_enemies, *gi_num_enemies, tr, tc );
  NPC_t* bump_npc = NPCAt( gi_npcs, *gi_num_npcs, tr, tc );
  if ( bump )
  {
    PlayerLunge( dr, dc );
    CombatAttack( bump );
  }
  else if ( bump_npc )
  {
    PlayerSetFacing( bump_npc->world_x < player.world_x );
    if ( EnemiesInCombat( gi_enemies, *gi_num_enemies ) )
    {
      NPCType_t* nt = &g_npc_types[bump_npc->type_idx];
      CombatVFXSpawnText( bump_npc->world_x, bump_npc->world_y,
                          d_StringPeek( nt->combat_bark ), nt->color );
      ConsolePushF( gi_console, nt->color,
                    "%s yells \"%s\"", d_StringPeek( nt->name ),
                    d_StringPeek( nt->combat_bark ) );
    }
    else
    {
      DialogueStart( bump_npc->type_idx );
    }
  }
  else if ( TileWalkable( tr, tc ) )
    PlayerStartMove( tr, tc );
  else if ( TileHasDoor( tr, tc ) )
  {
    if ( TileActionsTryOpen( tr, tc ) )
      PlayerStartMove( tr, tc );
    else
      PlayerShake( dr, dc );
  }
  else if ( ITileIsRevealedHiddenWall( tr, tc ) )
  {
    ITileOpenHiddenWall( gi_world, tr, tc );
    PlayerStartMove( tr, tc );
  }
  else
    PlayerWallBump( dr, dc );
}

void GameInputMouse( void )
{
  aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
  int mx = app.mouse.x;
  int my = app.mouse.y;
  hover_row = -1;
  hover_col = -1;

  if ( !PointInRect( mx, my, vp->rect.x, vp->rect.y, vp->rect.w, vp->rect.h ) )
    return;

  float wx, wy;
  GV_ScreenToWorld( vp->rect, gi_cam, mx, my, &wx, &wy );
  int r = (int)( wx / gi_world->tile_w );
  int c = (int)( wy / gi_world->tile_h );
  if ( r >= 0 && r < gi_world->width && c >= 0 && c < gi_world->height
       && VisibilityGet( r, c ) > 0.01f )
  {
    hover_row = r;
    hover_col = c;
  }

  if ( app.mouse.pressed && hover_row >= 0
       && !PlayerIsMoving() && !EnemiesTurning()
       && GameTurnsEnemyDelay() <= 0 )
  {
    int fpr, fpc;
    GameTurnsGetPlayerTile( &fpr, &fpc );
    if ( RapidMoveActive()
         && TileAdjacent( hover_row, hover_col )
         && TileWalkable( hover_row, hover_col )
         && !EnemyAt( gi_enemies, *gi_num_enemies, hover_row, hover_col )
         && !NPCAt( gi_npcs, *gi_num_npcs, hover_row, hover_col ) )
    {
      app.mouse.pressed = 0;
      PlayerStartMove( hover_row, hover_col );
    }
    else
    {
      app.mouse.pressed = 0;
      int on_self = ( hover_row == fpr && hover_col == fpc );
      TileActionsOpen( hover_row, hover_col, on_self );
    }
  }
}

void GameInputZoom( void )
{
  if ( app.mouse.wheel == 0 ) return;

  aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
  if ( PointInRect( app.mouse.x, app.mouse.y,
                    cp->rect.x, cp->rect.y, cp->rect.w, cp->rect.h ) )
  {
    ConsoleScroll( gi_console, app.mouse.wheel );
    app.mouse.wheel = 0;
  }
  if ( app.mouse.wheel != 0 )
  {
    GV_Zoom( gi_cam, app.mouse.wheel, ZOOM_MIN_H, ZOOM_MAX_H );
    app.mouse.wheel = 0;
  }
}

int  GameInputHoverRow( void )        { return hover_row; }
int  GameInputHoverCol( void )        { return hover_col; }
int  GameInputTurnSkipped( void )     { return turn_skipped; }
void GameInputClearTurnSkipped( void ) { turn_skipped = 0; }
