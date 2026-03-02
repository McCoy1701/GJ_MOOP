#include <string.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "floor_cutscene.h"
#include "dialogue.h"
#include "combat_vfx.h"
#include "tween.h"
#include "dungeon.h"

/* ---- Cutscene phases ---- */
enum {
  CS_NONE = 0,
  CS_DROP,
  CS_LAURA,
  CS_GOBLIN,
  CS_GLORBNAX,
};

/* ---- Cached entity pointers + positions ---- */
static NPC_t*   cs_laura    = NULL;
static Enemy_t* cs_grunt    = NULL;
static NPC_t*   cs_glorbnax = NULL;

/* ---- State ---- */
static int   cs_phase   = CS_NONE;
static float cs_timer   = 0.0f;
static int   cs_started = 0;
static float cs_player_oy = 0.0f;

static TweenManager_t cs_tweens;

/* ---- Phase timing (seconds from start) ---- */
#define T_LAURA    0.8f
#define T_GOBLIN   1.8f
#define T_GLORBNAX 2.8f
#define T_END      4.0f

/* ---- Text lines ---- */
#define LINE_LAURA    "Watch your step."
#define LINE_GOBLIN   "Heh. Fresh meat."
#define LINE_GLORBNAX "...welcome."

/* ---- Colors ---- */
#define COL_WHITE  (aColor_t){ 0xeb, 0xed, 0xe9, 255 }

/* ------------------------------------------------------------------ */

void FloorCutsceneRegister( NPC_t* npcs, int num_npcs,
                            Enemy_t* enemies, int num_enemies )
{
  cs_phase   = CS_NONE;
  cs_timer   = 0.0f;
  cs_player_oy = 0.0f;
  cs_laura    = NULL;
  cs_grunt    = NULL;
  cs_glorbnax = NULL;

  if ( g_current_floor < 2 ) { cs_started = 0; return; }
  if ( cs_started )          return;

  /* Scan NPCs for laura and glorbnax */
  int laura_idx    = NPCTypeByKey( "laura" );
  int glorbnax_idx = NPCTypeByKey( "glorbnax_the_serene" );

  for ( int i = 0; i < num_npcs; i++ )
  {
    if ( !npcs[i].alive ) continue;
    if ( laura_idx >= 0 && npcs[i].type_idx == laura_idx )
      cs_laura = &npcs[i];
    if ( glorbnax_idx >= 0 && npcs[i].type_idx == glorbnax_idx )
      cs_glorbnax = &npcs[i];
  }

  /* Scan enemies for goblin_grunt */
  int grunt_idx = EnemyTypeByKey( "goblin_grunt" );
  for ( int i = 0; i < num_enemies; i++ )
  {
    if ( !enemies[i].alive ) continue;
    if ( grunt_idx >= 0 && enemies[i].type_idx == grunt_idx )
    { cs_grunt = &enemies[i]; break; }
  }

  /* Start the cutscene */
  cs_started = 1;
  cs_phase   = CS_DROP;
  cs_timer   = 0.0f;

  /* Player bounce tween: start 40px above, ease-out-bounce to 0 */
  InitTweenManager( &cs_tweens );
  cs_player_oy = -40.0f;
  TweenFloat( &cs_tweens, &cs_player_oy, 0.0f, 0.7f, TWEEN_EASE_OUT_BOUNCE );
}

int FloorCutsceneUpdate( float dt )
{
  if ( cs_phase == CS_NONE ) return 0;

  /* ESC skips the whole cutscene */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    cs_player_oy = 0.0f;
    StopAllTweens( &cs_tweens );
    cs_phase = CS_NONE;
    return 0;
  }

  cs_timer += dt;
  UpdateTweens( &cs_tweens, dt );

  /* Spawn text bubbles at the right moments */
  if ( cs_phase == CS_DROP && cs_timer >= T_LAURA )
  {
    cs_phase = CS_LAURA;
    if ( cs_laura )
    {
      NPCType_t* nt = &g_npc_types[cs_laura->type_idx];
      CombatVFXSpawnText( cs_laura->world_x, cs_laura->world_y,
                          LINE_LAURA, nt->color );
    }
  }

  if ( cs_phase == CS_LAURA && cs_timer >= T_GOBLIN )
  {
    cs_phase = CS_GOBLIN;
    if ( cs_grunt )
    {
      EnemyType_t* et = &g_enemy_types[cs_grunt->type_idx];
      CombatVFXSpawnText( cs_grunt->world_x, cs_grunt->world_y,
                          LINE_GOBLIN, et->color );
    }
  }

  if ( cs_phase == CS_GOBLIN && cs_timer >= T_GLORBNAX )
  {
    cs_phase = CS_GLORBNAX;
    if ( cs_glorbnax )
    {
      NPCType_t* nt = &g_npc_types[cs_glorbnax->type_idx];
      CombatVFXSpawnText( cs_glorbnax->world_x, cs_glorbnax->world_y,
                          LINE_GLORBNAX, nt->color );
    }
  }

  if ( cs_timer >= T_END )
  {
    cs_phase = CS_NONE;
    cs_player_oy = 0.0f;
    return 0;
  }

  return 1;
}

float FloorCutscenePlayerOY( void )
{
  return cs_player_oy;
}
