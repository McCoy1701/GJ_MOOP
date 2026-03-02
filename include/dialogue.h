#ifndef __DIALOGUE_H__
#define __DIALOGUE_H__

#include <Archimedes.h>
#include <Daedalus.h>

#define MAX_NPC_TYPES      16
#define MAX_DIALOGUE_NODES 512
#define MAX_NODE_OPTIONS    8
#define MAX_CONDITIONS      4
#define MAX_FLAGS          128

/* A single dialogue entry - speech node OR player option */
typedef struct
{
  dString_t* key;

  /* Speech node fields (NPC talks) */
  dString_t* text;
  dString_t* option_keys[MAX_NODE_OPTIONS];
  int  num_options;

  /* Option fields (player chooses) */
  dString_t* label;
  dString_t* goto_key;
  aColor_t label_color;       /* manual color override (alpha 0 = not set) */

  /* Start node */
  int  is_start;
  int  priority;

  /* Conditions (multiple require_flag / require_not_flag supported) */
  dString_t* require_class;
  dString_t* require_flag[MAX_CONDITIONS];
  int  num_require_flag;
  dString_t* require_flag_min;
  dString_t* require_not_flag[MAX_CONDITIONS];
  int  num_require_not_flag;
  dString_t* require_item;
  dString_t* require_lore[MAX_CONDITIONS];
  int  num_require_lore;

  /* Conditions (gold) */
  int  require_gold_min;

  /* Actions */
  dString_t* set_flag;
  dString_t* incr_flag;
  dString_t* clear_flag;
  dString_t* give_item;
  dString_t* take_item;
  dString_t* set_lore;
  int  give_gold;
} DialogueEntry_t;

/* NPC type - loaded from one DUF file */
typedef struct
{
  dString_t*  key;              /* filename stem: "guard" */
  dString_t*  name;
  dString_t*  glyph;
  aColor_t    color;
  dString_t*  description;
  dString_t*  combat_bark;
  dString_t*  idle_bark;
  dString_t*  idle_log;         /* console message when idle bark fires */
  int         idle_cooldown;    /* turns between idle barks (0 = disabled) */
  int         combat;           /* 1 = fights enemies in room */
  int         damage;           /* melee damage dealt to enemies */
  aImage_t*   image;
  DialogueEntry_t entries[MAX_DIALOGUE_NODES];
  int         num_entries;
} NPCType_t;

extern NPCType_t g_npc_types[MAX_NPC_TYPES];
extern int        g_num_npc_types;

/* Flag system - global key-value table on the player */
int  FlagGet( const char* name );
void FlagSet( const char* name, int value );
void FlagIncr( const char* name );
void FlagClear( const char* name );
void FlagsInit( void );

/* Init / destroy helpers */
void DialogueEntryInit( DialogueEntry_t* de );
void DialogueEntryDestroy( DialogueEntry_t* de );
void NPCTypeInit( NPCType_t* npc );
void NPCTypeDestroy( NPCType_t* npc );
void DialogueDestroyAll( void );

/* Loading */
void DialogueLoadAll( void );
int  NPCTypeByKey( const char* key );

/* Runtime */
void DialogueStart( int npc_type_idx );
void DialogueSelectOption( int index );
void DialogueEnd( void );
int  DialogueActive( void );

/* Current dialogue state (for UI) */
const char* DialogueGetNPCName( void );
aColor_t    DialogueGetNPCColor( void );
const char* DialogueGetText( void );
int         DialogueGetOptionCount( void );
const char* DialogueGetOptionLabel( int index );
aColor_t    DialogueGetOptionColor( int index );
aImage_t*   DialogueGetNPCImage( void );
const char* DialogueGetNPCGlyph( void );

#endif
