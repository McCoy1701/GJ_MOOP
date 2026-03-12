/*
 * ne_structs.h:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#ifndef __NE_STRUCTS_H__
#define __NE_STRUCTS_H__

#include <Archimedes.h>
#include <Daedalus.h>

#define NE_MAX_NPC_FILES    64
#define NE_MAX_NODES       512
#define NE_MAX_OPTIONS       8
#define NE_MAX_CONDITIONS    4
#define NE_UNDO_MAX         64

/* A single dialogue node in the editor graph */
typedef struct
{
  dString_t* key;

  /* Speech node fields */
  dString_t* text;
  dString_t* option_keys[NE_MAX_OPTIONS];
  int  num_options;

  /* Option node fields */
  dString_t* label;
  dString_t* goto_key;
  dString_t* color_name;

  /* Start / priority */
  int  is_start;
  int  priority;

  /* Conditions */
  dString_t* require_class;
  dString_t* require_flag[NE_MAX_CONDITIONS];
  int  num_require_flag;
  dString_t* require_flag_min;
  dString_t* require_not_flag[NE_MAX_CONDITIONS];
  int  num_require_not_flag;
  dString_t* require_item;
  dString_t* require_lore[NE_MAX_CONDITIONS];
  int  num_require_lore;
  dString_t* require_not_lore[NE_MAX_CONDITIONS];
  int  num_require_not_lore;
  int  require_gold_min;

  /* Actions */
  dString_t* set_flag;
  dString_t* incr_flag;
  dString_t* clear_flag;
  dString_t* give_item;
  dString_t* take_item;
  dString_t* set_lore;
  int  give_gold;
  dString_t* action;

  /* Graph position (Phase 2) */
  float gx, gy;
} NENode_t;

/* Full NPC dialogue graph in editor */
typedef struct
{
  dString_t* npc_key;
  dString_t* display_name;
  dString_t* glyph;
  dString_t* description;
  dString_t* image_path;
  dString_t* combat_bark;
  dString_t* idle_bark;
  dString_t* idle_log;
  dString_t* action_label;
  uint8_t color[4];
  int  idle_cooldown;
  int  combat;
  int  damage;
  int  no_face;
  int  no_shadow;

  NENode_t nodes[NE_MAX_NODES];
  int num_nodes;
  int dirty;
} NEGraph_t;

/* Undo/redo: deep-copied graph snapshots in a circular buffer */
typedef struct
{
  NEGraph_t snapshots[NE_UNDO_MAX];
  int head;
  int count;
  int redo_count;
} NEUndoStack_t;

/* Text field for inline editing */
#define NE_FIELD_BUF 512

typedef struct
{
  char buf[NE_FIELD_BUF];
  int  len;
  int  cursor;
  int  active;
} NETextField_t;

/* Entry in the NPC file browser list */
typedef struct
{
  dString_t* filename;
  dString_t* stem;
  dString_t* display_name;
  dString_t* folder;         /* relative folder name, e.g. "floor01" */
} NEFileEntry_t;

/* Folder expand/collapse state for browser */
#define NE_MAX_FOLDERS  32

typedef struct
{
  char name[128];
  int  expanded;
} NEFolderState_t;

#endif
