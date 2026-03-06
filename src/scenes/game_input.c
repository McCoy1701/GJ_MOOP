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
#include "dev_mode.h"
#include "interactive_tile.h"
#include "pathfinding.h"

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

/* Click-to-move auto-path */
static PathNode_t auto_path[PATH_MAX_LEN];
static int auto_path_len  = 0;
static int auto_path_step = 0;

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
  auto_path_len = 0;
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
  if ( auto_path_len > 0 )
  {
    auto_path_len = 0;
    return 0;
  }
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

  if ( !InventoryUIFocused() )
    InventoryUIHotkey();

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
    int res = GameEventResolveTarget( ci, si, tr, tc, gi_enemies, *gi_num_enemies );
    if ( res == 1 )
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

/* ---- Click-to-move blocker ---- */

static int player_path_blocked( int r, int c, void* ctx )
{
  (void)ctx;
  if ( !TileWalkable( r, c ) )                                   return 1;
  if ( EnemyAt( gi_enemies, *gi_num_enemies, r, c ) )            return 1;
  if ( NPCAt( gi_npcs, *gi_num_npcs, r, c ) )                    return 1;
  return 0;
}

void GameInputAutoMove( void )
{
  if ( auto_path_len == 0 ) return;

  /* Cancel if rooted */
  if ( player.root_turns > 0 )
  {
    auto_path_len = 0;
    return;
  }

  /* Cancel if combat starts */
  if ( EnemiesInCombat( gi_enemies, *gi_num_enemies ) )
  {
    auto_path_len = 0;
    return;
  }

  /* Cancel if any movement key pressed */
  if ( app.keyboard[SDL_SCANCODE_W] || app.keyboard[SDL_SCANCODE_S]
       || app.keyboard[SDL_SCANCODE_A] || app.keyboard[SDL_SCANCODE_D]
       || app.keyboard[SDL_SCANCODE_UP] || app.keyboard[SDL_SCANCODE_DOWN]
       || app.keyboard[SDL_SCANCODE_LEFT] || app.keyboard[SDL_SCANCODE_RIGHT]
       || app.keyboard[SDL_SCANCODE_SPACE] )
  {
    auto_path_len = 0;
    return;
  }

  /* Wait for player to be idle */
  if ( PlayerIsMoving() || EnemiesTurning() || InventoryUIFocused()
       || GameTurnsEnemyDelay() > 0 )
    return;

  /* Check next step is still walkable and unoccupied */
  int r = auto_path[auto_path_step].row;
  int c = auto_path[auto_path_step].col;

  if ( EnemyAt( gi_enemies, *gi_num_enemies, r, c )
       || NPCAt( gi_npcs, *gi_num_npcs, r, c ) )
  {
    auto_path_len = 0;
    return;
  }

  if ( !TileWalkable( r, c ) )
  {
    /* Try opening a door on the path */
    if ( TileHasDoor( r, c ) && TileActionsTryOpen( r, c ) )
      ; /* door opened, proceed */
    else
    {
      auto_path_len = 0;
      return;
    }
  }

  PlayerStartMove( r, c );
  auto_path_step++;
  if ( auto_path_step >= auto_path_len )
    auto_path_len = 0;
}

void GameInputMovement( void )
{
  if ( PlayerIsMoving() || EnemiesTurning() || InventoryUIFocused()
       || GameTurnsEnemyDelay() > 0 )
    return;

  /* Rooted — can attack adjacent enemies but can't move */
  if ( player.root_turns > 0 )
  {
    /* Space = skip turn, consumes root */
    if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 )
    {
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      player.root_turns--;
      turn_skipped = 1;
      ConsolePushF( gi_console, (aColor_t){ 0x9a, 0x8c, 0x7a, 255 },
                    "You struggle against the web! (%d)", player.root_turns );
      return;
    }

    /* Direction keys — allow bump-attacks, block movement */
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
    if ( bump )
    {
      PlayerLunge( dr, dc );
      CombatAttack( bump );
    }
    else
    {
      player.root_turns--;
      turn_skipped = 1;
      ConsolePushF( gi_console, (aColor_t){ 0x9a, 0x8c, 0x7a, 255 },
                    "You struggle against the web! (%d)", player.root_turns );
    }
    return;
  }

  /* Space = skip turn */
  if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    turn_skipped = 1;
    ConsolePush( gi_console, "You skip your turn.",
                 (aColor_t){ 0x81, 0x97, 0x96, 255 } );
    return;
  }

  int in_combat = EnemiesInCombat( gi_enemies, *gi_num_enemies );

  int dr = 0, dc = 0;
  if ( app.keyboard[SDL_SCANCODE_UP] == 1 || app.keyboard[SDL_SCANCODE_W] == 1 )
  { if ( in_combat ) { app.keyboard[SDL_SCANCODE_UP] = 0; app.keyboard[SDL_SCANCODE_W] = 0; } dc = -1; }
  else if ( app.keyboard[SDL_SCANCODE_DOWN] == 1 || app.keyboard[SDL_SCANCODE_S] == 1 )
  { if ( in_combat ) { app.keyboard[SDL_SCANCODE_DOWN] = 0; app.keyboard[SDL_SCANCODE_S] = 0; } dc =  1; }
  else if ( app.keyboard[SDL_SCANCODE_LEFT] == 1 || app.keyboard[SDL_SCANCODE_A] == 1 )
  { if ( in_combat ) { app.keyboard[SDL_SCANCODE_LEFT] = 0; app.keyboard[SDL_SCANCODE_A] = 0; } dr = -1; }
  else if ( app.keyboard[SDL_SCANCODE_RIGHT] == 1 || app.keyboard[SDL_SCANCODE_D] == 1 )
  { if ( in_combat ) { app.keyboard[SDL_SCANCODE_RIGHT] = 0; app.keyboard[SDL_SCANCODE_D] = 0; } dr =  1; }

  if ( dr == 0 && dc == 0 ) return;

  int fpr, fpc;
  GameTurnsGetPlayerTile( &fpr, &fpc );
  int tr = fpr + dr;
  int tc = fpc + dc;

  Enemy_t* bump = EnemyAt( gi_enemies, *gi_num_enemies, tr, tc );
  NPC_t* bump_npc = NPCAt( gi_npcs, *gi_num_npcs, tr, tc );
  if ( !bump && !bump_npc )
  {
    /* Debug: dump all alive NPCs near this tile */
    for ( int dbg = 0; dbg < *gi_num_npcs; dbg++ )
    {
      if ( !gi_npcs[dbg].alive ) continue;
      int dr2 = abs( gi_npcs[dbg].row - tr );
      int dc2 = abs( gi_npcs[dbg].col - tc );
      if ( dr2 + dc2 <= 2 )
        printf( "DEBUG NEAR (%d,%d): npc[%d] type=%d at (%d,%d) wx=%.1f wy=%.1f\n",
                tr, tc, dbg, gi_npcs[dbg].type_idx,
                gi_npcs[dbg].row, gi_npcs[dbg].col,
                gi_npcs[dbg].world_x, gi_npcs[dbg].world_y );
    }
  }
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
  else if ( ITileAt( tr, tc ) && ( ITileAt( tr, tc )->type == ITILE_OLD_CRATE
            || ITileAt( tr, tc )->type == ITILE_URN ) )
  {
    ITile_t* it = ITileAt( tr, tc );
    int is_urn = ( it->type == ITILE_URN );
    int gold = 0;
    if ( is_urn )
      ITileUrnCheck( gi_world, tr, tc, &gold );
    else
      ITileCrateCheck( gi_world, tr, tc, &gold );
    PlayerWallBump( dr, dc );
    const char* name = is_urn ? "urn" : "old crate";
    if ( gold > 0 )
    {
      PlayerAddGold( gold );
      ConsolePushF( gi_console, (aColor_t){ 0xda, 0xaf, 0x20, 255 },
                    "You smash the %s. Found %d gold inside!", name, gold );
      char vfx[8];
      snprintf( vfx, sizeof( vfx ), "+%d", gold );
      CombatVFXSpawnText( tr * gi_world->tile_w + gi_world->tile_w / 2.0f,
                          tc * gi_world->tile_h + gi_world->tile_h / 2.0f,
                          vfx, (aColor_t){ 0xda, 0xaf, 0x20, 255 } );
    }
    else
    {
      aColor_t c = is_urn ? (aColor_t){ 0x8a, 0x5c, 0x3e, 255 }
                          : (aColor_t){ 0xa0, 0x78, 0x46, 255 };
      ConsolePushF( gi_console, c, "You smash the %s. Empty.", name );
    }
    turn_skipped = 1;
  }
  else if ( TileWalkable( tr, tc ) || DevModeNoclip() )
    PlayerStartMove( tr, tc );
  else if ( TileHasDoor( tr, tc ) )
  {
    if ( TileActionsTryOpen( tr, tc ) )
      PlayerStartMove( tr, tc );
    else
      PlayerShake( dr, dc );
  }
  else if ( ITileIsHiddenWall( tr, tc ) )
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
         && !EnemyAt( gi_enemies, *gi_num_enemies, hover_row, hover_col )
         && !NPCAt( gi_npcs, *gi_num_npcs, hover_row, hover_col ) )
    {
      app.mouse.pressed = 0;
      if ( ITileIsHiddenWall( hover_row, hover_col ) )
        ITileOpenHiddenWall( gi_world, hover_row, hover_col );
      if ( TileWalkable( hover_row, hover_col ) )
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

void GameInputStartAutoPath( int goal_r, int goal_c )
{
  if ( player.root_turns > 0 ) return;
  int fpr, fpc;
  GameTurnsGetPlayerTile( &fpr, &fpc );
  int len = PathfindAStar( fpr, fpc, goal_r, goal_c,
                           gi_world->width, gi_world->height,
                           player_path_blocked, NULL, auto_path );
  if ( len >= 2 )
  {
    auto_path_len  = len;
    auto_path_step = 1;
  }
}

int  GameInputHoverRow( void )        { return hover_row; }
int  GameInputHoverCol( void )        { return hover_col; }
int  GameInputTurnSkipped( void )     { return turn_skipped; }
void GameInputClearTurnSkipped( void ) { turn_skipped = 0; }
