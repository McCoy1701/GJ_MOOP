#include <Archimedes.h>
#include <string.h>

#include "dev_mode.h"
#include "dialogue.h"
#include "player.h"
#include "items.h"
#include "movement.h"
#include "visibility.h"
#include "context_menu.h"

extern Player_t player;

static int         dev_active = 0;
static Console_t*  dev_console = NULL;
static NPC_t*      dev_npcs = NULL;
static int*        dev_num_npcs = NULL;

#define DEV_COLOR (aColor_t){ 0x39, 0xff, 0x14, 255 }

/* ---- Teleport NPC selector ---- */
static int         tp_open   = 0;
static int         tp_cursor = 0;
static const char* tp_labels[MAX_NPCS];
static int         tp_npc_map[MAX_NPCS]; /* label index → dev_npcs[] index */
static int         tp_count  = 0;

void DevModeInit( Console_t* console )
{
  dev_console = console;
  dev_active  = 0;
}

void DevModeSetNPCs( NPC_t* list, int* count )
{
  dev_npcs     = list;
  dev_num_npcs = count;
}

int DevModeActive( void )
{
  return dev_active;
}

int DevModeInput( void )
{
  /* F1 toggles dev mode */
  if ( app.keyboard[SDL_SCANCODE_F1] == 1 )
  {
    app.keyboard[SDL_SCANCODE_F1] = 0;
    dev_active = !dev_active;
    if ( dev_active )
    {
      ConsolePush( dev_console, "DEV MODE ON", DEV_COLOR );
      ConsolePush( dev_console, "T - teleport to NPC", DEV_COLOR );
      ConsolePush( dev_console, "G - gear up (floor 01 drops)", DEV_COLOR );
    }
    else
    {
      ConsolePush( dev_console, "DEV MODE OFF", DEV_COLOR );
    }
    return 1;
  }

  if ( !dev_active ) return 0;

  /* Teleport menu active - handle navigation */
  if ( tp_open )
  {
    if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
    {
      app.keyboard[SDL_SCANCODE_W] = 0;
      app.keyboard[SDL_SCANCODE_UP] = 0;
      tp_cursor = ( tp_cursor - 1 + tp_count ) % tp_count;
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      tp_cursor = ( tp_cursor + 1 ) % tp_count;
    }

    /* Select */
    if ( app.keyboard[SDL_SCANCODE_RETURN] == 1
         || app.keyboard[SDL_SCANCODE_SPACE] == 1 )
    {
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      app.keyboard[SDL_SCANCODE_SPACE]  = 0;

      NPC_t* n = &dev_npcs[tp_npc_map[tp_cursor]];
      NPCType_t* nt = &g_npc_types[n->type_idx];

      /* Try adjacent tiles: below, above, right, left */
      static const int dr[] = { 0, 0, 1, -1 };
      static const int dc[] = { 1, -1, 0, 0 };
      int dest_r = n->row, dest_c = n->col;

      for ( int d = 0; d < 4; d++ )
      {
        int tr = n->row + dr[d];
        int tc = n->col + dc[d];
        if ( TileWalkable( tr, tc ) )
        {
          dest_r = tr;
          dest_c = tc;
          break;
        }
      }

      PlayerSetWorldPos( dest_r * 16 + 8.0f, dest_c * 16 + 8.0f );

      int pr, pc;
      PlayerGetTile( &pr, &pc );
      VisibilityUpdate( pr, pc );

      ConsolePushF( dev_console, DEV_COLOR, "TP: %s",
                    d_StringPeek( nt->name ) );
      tp_open = 0;
    }

    /* Cancel */
    if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
    {
      app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
      tp_open = 0;
    }

    /* Consume all movement keys while menu is open */
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_A] = 0;
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_D] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    app.keyboard[SDL_SCANCODE_LEFT] = 0;
    app.keyboard[SDL_SCANCODE_RIGHT] = 0;
    return 1;
  }

  /* T - open teleport NPC selector */
  if ( app.keyboard[SDL_SCANCODE_T] == 1 )
  {
    app.keyboard[SDL_SCANCODE_T] = 0;

    tp_count  = 0;
    tp_cursor = 0;
    for ( int i = 0; i < *dev_num_npcs; i++ )
    {
      if ( !dev_npcs[i].alive ) continue;
      NPCType_t* nt = &g_npc_types[dev_npcs[i].type_idx];
      tp_labels[tp_count]  = d_StringPeek( nt->name );
      tp_npc_map[tp_count] = i;
      tp_count++;
    }

    if ( tp_count > 0 ) tp_open = 1;
    return 1;
  }

  /* G - gear up (floor 01 drops: weapon + armor + trinket) */
  if ( app.keyboard[SDL_SCANCODE_G] == 1 )
  {
    app.keyboard[SDL_SCANCODE_G] = 0;

    const char* cls = PlayerClassKey();
    const char* weapon_key  = NULL;
    const char* armor_key   = NULL;
    const char* trinket_key = NULL;

    if ( strcmp( cls, "mercenary" ) == 0 )
    {
      weapon_key  = "heavy_mace";
      armor_key   = "chainmail";
      trinket_key = "blood_medal";
    }
    else if ( strcmp( cls, "rogue" ) == 0 )
    {
      weapon_key  = "serrated_blade";
      armor_key   = "shadow_vest";
      trinket_key = "viper_fang";
    }
    else if ( strcmp( cls, "mage" ) == 0 )
    {
      weapon_key  = "arcane_rod";
      armor_key   = "warded_robes";
      trinket_key = "surge_stone";
    }

    if ( weapon_key )
    {
      int wi = EquipmentByKey( weapon_key );
      if ( wi >= 0 ) PlayerEquip( EQUIP_WEAPON, wi );

      int ai = EquipmentByKey( armor_key );
      if ( ai >= 0 ) PlayerEquip( EQUIP_ARMOR, ai );

      int ti = EquipmentByKey( trinket_key );
      if ( ti >= 0 ) PlayerEquip( EQUIP_TRINKET1, ti );

      PlayerRecalcStats();
      ConsolePush( dev_console, "GEAR UP: floor 01 drops equipped", DEV_COLOR );
    }

    return 1;
  }

  return 0;
}

void DevModeDraw( void )
{
  if ( !tp_open || tp_count == 0 ) return;

  float menu_w = CTX_MENU_W;
  float menu_h = tp_count * ( CTX_MENU_ROW_H + CTX_MENU_PAD ) - CTX_MENU_PAD;
  float mx = ( SCREEN_WIDTH  - menu_w ) / 2.0f;
  float my = ( SCREEN_HEIGHT - menu_h ) / 2.0f;

  DrawContextMenu( mx, my, tp_labels, tp_count, tp_cursor );
}
