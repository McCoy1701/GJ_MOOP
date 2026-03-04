#include <string.h>
#include <ctype.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "draw_utils.h"
#include "context_menu.h"
#include "movement.h"
#include "combat.h"
#include "tile_actions.h"
#include "npc.h"
#include "dialogue.h"
#include "combat_vfx.h"
#include "ground_items.h"
#include "items.h"
#include "maps.h"
#include "doors.h"
#include "objects.h"
#include "shop.h"
#include "poison_pool.h"
#include "interactive_tile.h"
#include "game_input.h"

extern Player_t player;

#define TILE_ACTION_COUNT 4

static World_t*       world;
static GameCamera_t*  camera;
static Console_t*     console;
static aSoundEffect_t* sfx_move;
static aSoundEffect_t* sfx_click;

static int tile_action_open    = 0;
static int tile_action_cursor  = 0;
static int tile_action_row, tile_action_col;
static int tile_action_on_self = 0;

/* Check if midground has a door at (r,c) - delegates to doors.c */
static int tile_has_door( int r, int c )
{
  int idx = c * world->width + r;
  return DoorIsDoor( world->midground[idx].tile );
}

/* Try to open a door at (r,c). Returns 1 if opened, 0 if locked. */
int TileActionsTryOpen( int r, int c )
{
  return DoorTryOpen( world, r, c );
}

/* Build the label list for the current tile action target */
static NPC_t* ta_npcs      = NULL;
static int*   ta_num_npcs  = NULL;

static GroundItem_t* ta_ground_items     = NULL;
static int*          ta_num_ground_items = NULL;

static int build_labels( const char** labels, Enemy_t* enemies, int num_enemies )
{
  int adjacent = TileAdjacent( tile_action_row, tile_action_col );

  Enemy_t* e = EnemyAt( enemies, num_enemies,
                         tile_action_row, tile_action_col );

  if ( e && adjacent )
  {
    labels[0] = "Attack";
    labels[1] = "Look";
    return 2;
  }

  /* NPC: configurable action (Talk, Walk, Open, Read...) */
  NPC_t* n = NPCAt( ta_npcs, *ta_num_npcs,
                     tile_action_row, tile_action_col );
  if ( n && adjacent )
  {
    NPCType_t* nt = &g_npc_types[n->type_idx];
    labels[0] = d_StringPeek( nt->action_label );
    labels[1] = "Look";
    return 2;
  }

  if ( tile_action_on_self )
  {
    labels[0] = "Skip Turn";
    labels[1] = "Look";
    return 2;
  }

  if ( !adjacent )
  {
    int in_combat = EnemiesInCombat( enemies, num_enemies );

    /* Distant door - offer Open (auto-walk then open) */
    if ( !in_combat
         && tile_has_door( tile_action_row, tile_action_col ) )
    {
      labels[0] = "Open";
      labels[1] = "Look";
      return 2;
    }

    /* Distant walkable tile - offer Move (auto-walk) */
    if ( !in_combat
         && TileWalkable( tile_action_row, tile_action_col )
         && !EnemyAt( enemies, num_enemies,
                       tile_action_row, tile_action_col )
         && !NPCAt( ta_npcs, *ta_num_npcs,
                     tile_action_row, tile_action_col ) )
    {
      labels[0] = "Move";
      labels[1] = "Look";
      return 2;
    }

    labels[0] = "Look";
    return 1;
  }

  /* Adjacent door */
  if ( tile_has_door( tile_action_row, tile_action_col ) )
  {
    labels[0] = "Open";
    labels[1] = "Look";
    return 2;
  }

  labels[0] = "Move";
  labels[1] = "Look";
  return 2;
}

void TileActionsInit( World_t* w, GameCamera_t* cam, Console_t* con,
                      aSoundEffect_t* move, aSoundEffect_t* click )
{
  world     = w;
  camera    = cam;
  console   = con;
  sfx_move  = move;
  sfx_click = click;
  tile_action_open = 0;

  DoorsInit( con );
  /* ObjectsInit called before DungeonBuild in GameSceneInit */
}

void TileActionsSetNPCs( NPC_t* npcs, int* num_npcs )
{
  ta_npcs     = npcs;
  ta_num_npcs = num_npcs;
}

void TileActionsSetGroundItems( GroundItem_t* items, int* count )
{
  ta_ground_items     = items;
  ta_num_ground_items = count;
}

void TileActionsOpen( int row, int col, int on_self )
{
  tile_action_row     = row;
  tile_action_col     = col;
  tile_action_cursor  = 0;
  tile_action_on_self = on_self;
  tile_action_open    = 1;
}

int  TileActionsIsOpen( void )  { return tile_action_open; }
void TileActionsClose( void )   { tile_action_open = 0; }

int TileActionsLogic( int mouse_moved, Enemy_t* enemies, int num_enemies )
{
  if ( !tile_action_open ) return 0;

  const char* ta_labels[TILE_ACTION_COUNT];
  int ta_count = build_labels( ta_labels, enemies, num_enemies );

  /* ESC closes menu */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    tile_action_open = 0;
    return 1;
  }

  /* W/S or Up/Down - navigate */
  if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
  {
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    tile_action_cursor--;
    if ( tile_action_cursor < 0 ) tile_action_cursor = ta_count - 1;
    a_AudioPlaySound( sfx_move, NULL );
  }
  if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
  {
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    tile_action_cursor++;
    if ( tile_action_cursor >= ta_count ) tile_action_cursor = 0;
    a_AudioPlaySound( sfx_move, NULL );
  }

  /* Scroll wheel */
  if ( app.mouse.wheel != 0 )
  {
    tile_action_cursor += ( app.mouse.wheel < 0 ) ? 1 : -1;
    if ( tile_action_cursor < 0 ) tile_action_cursor = ta_count - 1;
    if ( tile_action_cursor >= ta_count ) tile_action_cursor = 0;
    a_AudioPlaySound( sfx_move, NULL );
    app.mouse.wheel = 0;
  }

  /* Mouse hover on action menu rows */
  if ( mouse_moved )
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    float sx, sy, cl, ct;
    float aspect = vp->rect.w / vp->rect.h;
    float cam_w = camera->half_h * aspect;
    sx = vp->rect.w / ( cam_w * 2.0f );
    sy = vp->rect.h / ( camera->half_h * 2.0f );
    cl = camera->x - cam_w;
    ct = camera->y - camera->half_h;
    float twx = tile_action_row * world->tile_w;
    float twy = tile_action_col * world->tile_h;
    float menu_sx = ( twx - cl ) * sx + vp->rect.x + world->tile_w * sx + 4;
    float menu_sy = ( twy - ct ) * sy + vp->rect.y;

    for ( int i = 0; i < ta_count; i++ )
    {
      float ry = menu_sy + i * ( CTX_MENU_ROW_H + CTX_MENU_PAD );
      if ( PointInRect( app.mouse.x, app.mouse.y, menu_sx, ry,
                        CTX_MENU_W, CTX_MENU_ROW_H ) )
      {
        if ( i != tile_action_cursor )
          a_AudioPlaySound( sfx_move, NULL );
        tile_action_cursor = i;
        break;
      }
    }
  }

  /* Space/Enter or click - execute action */
  int exec = 0;
  if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
  {
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    exec = 1;
  }
  if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
  {
    app.mouse.pressed = 0;
    /* Only execute if click is on the menu, otherwise close */
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    float sx, sy, cl, ct;
    float aspect = vp->rect.w / vp->rect.h;
    float cam_w = camera->half_h * aspect;
    sx = vp->rect.w / ( cam_w * 2.0f );
    sy = vp->rect.h / ( camera->half_h * 2.0f );
    cl = camera->x - cam_w;
    ct = camera->y - camera->half_h;
    float twx = tile_action_row * world->tile_w;
    float twy = tile_action_col * world->tile_h;
    float mx = ( twx - cl ) * sx + vp->rect.x + world->tile_w * sx + 4;
    float my = ( twy - ct ) * sy + vp->rect.y;
    float mh = ta_count * ( CTX_MENU_ROW_H + CTX_MENU_PAD ) - CTX_MENU_PAD;

    if ( PointInRect( app.mouse.x, app.mouse.y, mx, my, CTX_MENU_W, mh ) )
    {
      exec = 1;
    }
    else
    {
      /* Click on the same tile = execute first action (double-click shortcut) */
      float cwx, cwy;
      GV_ScreenToWorld( vp->rect, camera, app.mouse.x, app.mouse.y, &cwx, &cwy );
      int cr = (int)( cwx / world->tile_w );
      int cc = (int)( cwy / world->tile_h );
      if ( cr == tile_action_row && cc == tile_action_col )
      {
        tile_action_cursor = 0;
        exec = 1;
      }
      else
        tile_action_open = 0;
    }
  }

  if ( exec )
  {
    a_AudioPlaySound( sfx_click, NULL );
    const char* action = ta_labels[tile_action_cursor];

    if ( strcmp( action, "Skip Turn" ) == 0 )
    {
      ConsolePush( console, "You skip your turn.",
                   (aColor_t){ 0x81, 0x97, 0x96, 255 } );
      tile_action_open = 0;
      return 2;
    }
    else if ( strcmp( action, "Move" ) == 0 )
    {
      if ( TileAdjacent( tile_action_row, tile_action_col ) )
      {
        if ( TileWalkable( tile_action_row, tile_action_col ) )
          PlayerStartMove( tile_action_row, tile_action_col );
        else
        {
          int pr, pc;
          PlayerGetTile( &pr, &pc );
          PlayerWallBump( tile_action_row - pr, tile_action_col - pc );
        }
      }
      else
      {
        GameInputStartAutoPath( tile_action_row, tile_action_col );
      }
    }
    else if ( strcmp( action, "Open" ) == 0 )
    {
      if ( TileAdjacent( tile_action_row, tile_action_col ) )
      {
        if ( TileActionsTryOpen( tile_action_row, tile_action_col ) )
          PlayerStartMove( tile_action_row, tile_action_col );
        else
        {
          int pr, pc;
          PlayerGetTile( &pr, &pc );
          PlayerShake( tile_action_row - pr, tile_action_col - pc );
        }
      }
      else
      {
        /* Auto-walk to tile adjacent to the door, then open on arrival */
        GameInputStartAutoPath( tile_action_row, tile_action_col );
      }
    }
    else if ( strcmp( action, "Attack" ) == 0 )
    {
      Enemy_t* ae = EnemyAt( enemies, num_enemies,
                              tile_action_row, tile_action_col );
      if ( ae )
      {
        int pr, pc;
        PlayerGetTile( &pr, &pc );
        PlayerLunge( tile_action_row - pr, tile_action_col - pc );
        CombatAttack( ae );
      }
    }
    else if ( strcmp( action, "Look" ) != 0 )
    {
      NPC_t* tn = NPCAt( ta_npcs, *ta_num_npcs,
                           tile_action_row, tile_action_col );
      if ( tn )
      {
        PlayerSetFacing( tn->world_x < player.world_x );
        if ( EnemiesInCombat( enemies, num_enemies ) )
        {
          NPCType_t* nt = &g_npc_types[tn->type_idx];
          CombatVFXSpawnText( tn->world_x, tn->world_y,
                              d_StringPeek( nt->combat_bark ), nt->color );
          ConsolePushF( console, nt->color,
                        "%s yells \"%s\"", d_StringPeek( nt->name ),
                        d_StringPeek( nt->combat_bark ) );
        }
        else
        {
          DialogueStart( tn->type_idx );
        }
      }
    }
    else /* Look */
    {
      if ( tile_action_on_self )
      {
        ConsolePush( console, "You see yourself.",
                     (aColor_t){ 0x81, 0x97, 0x96, 255 } );
      }
      else
      {
        Enemy_t* le = EnemyAt( enemies, num_enemies,
                                tile_action_row, tile_action_col );
        NPC_t* ln = NPCAt( ta_npcs, *ta_num_npcs,
                             tile_action_row, tile_action_col );
        if ( le )
        {
          EnemyType_t* lt = &g_enemy_types[le->type_idx];
          char desc[256];
          strncpy( desc, lt->description, 255 ); desc[255] = '\0';
          desc[0] = (char)tolower( (unsigned char)desc[0] );
          int elen = (int)strlen( desc );
          int eperiod = ( elen > 0 && desc[elen - 1] == '.' );
          ConsolePushF( console, lt->color,
                        eperiod ? "You see %s" : "You see %s.", desc );
          ConsolePushF( console, lt->color,
                        "  HP: %d/%d  DMG: %d  DEF: %d",
                        le->hp, lt->hp, lt->damage, lt->defense );
          if ( lt->range > 0 )
            ConsolePushF( console, lt->color,
                          "  Ranged (range %d)", lt->range );
        }
        else if ( ln )
        {
          NPCType_t* lnt = &g_npc_types[ln->type_idx];
          char desc[256];
          strncpy( desc, d_StringPeek( lnt->description ), 255 ); desc[255] = '\0';
          desc[0] = (char)tolower( (unsigned char)desc[0] );
          int dlen = (int)strlen( desc );
          int has_period = ( dlen > 0 && desc[dlen - 1] == '.' );
          ConsolePushF( console, lnt->color,
                        has_period ? "You see %s" : "You see %s.", desc );
        }
        else
        {
          /* Check for shop items on rug */
          ShopItem_t* shop_i = ShopItemAt( tile_action_row, tile_action_col );
          if ( shop_i )
          {
            if ( shop_i->item_type == INV_CONSUMABLE )
            {
              ConsumableInfo_t* ci = &g_consumables[shop_i->item_index];
              ConsolePushF( console, ci->color,
                            "%s -- %dg", ci->name, shop_i->cost );
              ConsolePushF( console, (aColor_t){ 0xa8, 0xb5, 0xb2, 255 },
                            "  %s", ci->description );
            }
            else
            {
              EquipmentInfo_t* ei = &g_equipment[shop_i->item_index];
              ConsolePushF( console, ei->color,
                            "%s -- %dg", ei->name, shop_i->cost );
              ConsolePushF( console, (aColor_t){ 0xa8, 0xb5, 0xb2, 255 },
                            "  %s", ei->description );
            }
          }
          /* Check for poison pool */
          else if ( PoisonPoolAt( tile_action_row, tile_action_col ) )
          {
            PoisonPool_t* pp = PoisonPoolAt( tile_action_row, tile_action_col );
            ConsolePushF( console, (aColor_t){ 50, 220, 50, 255 },
                          "A bubbling pool of poison. %d turn%s remaining.",
                          pp->turns_remaining,
                          pp->turns_remaining == 1 ? "" : "s" );
          }
          /* Check for ground items */
          else
          {
            GroundItem_t* gi = ( ta_ground_items && ta_num_ground_items )
              ? GroundItemAt( ta_ground_items, *ta_num_ground_items,
                              tile_action_row, tile_action_col )
              : NULL;
            if ( gi )
            {
              const char* iname;
              aColor_t    icolor;
              const char* idesc;
              if ( gi->item_type == GROUND_MAP )
              {
                iname  = g_maps[gi->item_idx].name;
                icolor = g_maps[gi->item_idx].color;
                idesc  = g_maps[gi->item_idx].description;
              }
              else if ( gi->item_type == GROUND_EQUIPMENT )
              {
                iname  = g_equipment[gi->item_idx].name;
                icolor = g_equipment[gi->item_idx].color;
                idesc  = g_equipment[gi->item_idx].description;
              }
              else
              {
                iname  = g_consumables[gi->item_idx].name;
                icolor = g_consumables[gi->item_idx].color;
                idesc  = g_consumables[gi->item_idx].description;
              }
              ConsolePushF( console, icolor,
                            "You see %s on the ground.", iname );
              ConsolePushF( console, (aColor_t){ 0xa8, 0xb5, 0xb2, 255 },
                            "  %s", idesc );
            }
            else if ( tile_has_door( tile_action_row, tile_action_col ) )
            {
              DoorDescribe( world, tile_action_row, tile_action_col );
            }
            else
            {
              ITile_t* itile = ITileAt( tile_action_row, tile_action_col );
              int idx = tile_action_col * world->width + tile_action_row;
              Tile_t* t = &world->background[idx];
              if ( itile )
              {
                if ( itile->type == ITILE_HIDDEN_WALL && !itile->revealed )
                  ConsolePushF( console, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                                "You see a stone wall." );
                else if ( itile->type == ITILE_HIDDEN_WALL && itile->revealed )
                  ConsolePushF( console, (aColor_t){ 0xc0, 0x94, 0x73, 255 },
                                "A cracked wall. It looks like you could push through." );
                else
                  ConsolePushF( console, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                                "%s", ITileDescription( itile->type ) );
              }
              else if ( ObjectIsObject( tile_action_row, tile_action_col ) )
                ObjectDescribe( tile_action_row, tile_action_col );
              else if ( t->solid )
                ConsolePushF( console, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                              "You see a stone wall." );
              else
                ConsolePushF( console, (aColor_t){ 0x81, 0x97, 0x96, 255 },
                              "You see stone floor." );
            }
          }
        }
      }
    }
    tile_action_open = 0;
  }

  /* Consume remaining input */
  app.keyboard[SDL_SCANCODE_W] = 0;
  app.keyboard[SDL_SCANCODE_S] = 0;
  app.keyboard[SDL_SCANCODE_A] = 0;
  app.keyboard[SDL_SCANCODE_D] = 0;
  app.keyboard[SDL_SCANCODE_UP] = 0;
  app.keyboard[SDL_SCANCODE_DOWN] = 0;
  app.keyboard[SDL_SCANCODE_LEFT] = 0;
  app.keyboard[SDL_SCANCODE_RIGHT] = 0;
  app.keyboard[SDL_SCANCODE_SPACE] = 0;
  app.keyboard[SDL_SCANCODE_RETURN] = 0;
  return 1;
}

void TileActionsDraw( Enemy_t* enemies, int num_enemies )
{
  if ( !tile_action_open || !world ) return;

  aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
  float sx, sy, cl, ct;
  float aspect = vp->rect.w / vp->rect.h;
  float cam_w = camera->half_h * aspect;
  sx = vp->rect.w / ( cam_w * 2.0f );
  sy = vp->rect.h / ( camera->half_h * 2.0f );
  cl = camera->x - cam_w;
  ct = camera->y - camera->half_h;

  float twx = tile_action_row * world->tile_w;
  float twy = tile_action_col * world->tile_h;
  float menu_x = ( twx - cl ) * sx + vp->rect.x + world->tile_w * sx + 4;
  float menu_y = ( twy - ct ) * sy + vp->rect.y;

  const char* ta_labels[TILE_ACTION_COUNT];
  int ta_count = build_labels( ta_labels, enemies, num_enemies );

  DrawContextMenu( menu_x, menu_y, ta_labels, ta_count, tile_action_cursor );
}

int TileActionsGetRow( void ) { return tile_action_row; }
int TileActionsGetCol( void ) { return tile_action_col; }
