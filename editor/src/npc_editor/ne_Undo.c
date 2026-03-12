/*
 * npc_editor/ne_Undo.c:
 *
 * Deep-copy undo/redo system for the NPC dialogue editor.
 * NEGraph_t uses dString_t* (heap pointers), so snapshots require
 * explicit string copies — memcpy is not safe.
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "npc_editor.h"

/* ---- Helpers ---- */

static void copy_str( dString_t* dst, dString_t* src )
{
  if ( !dst || !src ) return;
  const char* s = d_StringPeek( src );
  if ( s && s[0] )
    d_StringSet( dst, s );
}

/* ---- Deep copy a single node ---- */

static void node_deep_copy( NENode_t* dst, NENode_t* src )
{
  ne_NodeInit( dst );

  copy_str( dst->key,        src->key );
  copy_str( dst->text,       src->text );
  copy_str( dst->label,      src->label );
  copy_str( dst->goto_key,   src->goto_key );
  copy_str( dst->color_name, src->color_name );

  dst->is_start = src->is_start;
  dst->priority = src->priority;
  dst->gx       = src->gx;
  dst->gy       = src->gy;

  /* Options */
  dst->num_options = src->num_options;
  for ( int i = 0; i < src->num_options; i++ )
    copy_str( dst->option_keys[i], src->option_keys[i] );

  /* Conditions */
  copy_str( dst->require_class,    src->require_class );
  copy_str( dst->require_flag_min, src->require_flag_min );
  copy_str( dst->require_item,     src->require_item );

  dst->num_require_flag = src->num_require_flag;
  for ( int i = 0; i < src->num_require_flag; i++ )
    copy_str( dst->require_flag[i], src->require_flag[i] );

  dst->num_require_not_flag = src->num_require_not_flag;
  for ( int i = 0; i < src->num_require_not_flag; i++ )
    copy_str( dst->require_not_flag[i], src->require_not_flag[i] );

  dst->num_require_lore = src->num_require_lore;
  for ( int i = 0; i < src->num_require_lore; i++ )
    copy_str( dst->require_lore[i], src->require_lore[i] );

  dst->num_require_not_lore = src->num_require_not_lore;
  for ( int i = 0; i < src->num_require_not_lore; i++ )
    copy_str( dst->require_not_lore[i], src->require_not_lore[i] );

  dst->require_gold_min = src->require_gold_min;

  /* Actions */
  copy_str( dst->set_flag,   src->set_flag );
  copy_str( dst->incr_flag,  src->incr_flag );
  copy_str( dst->clear_flag, src->clear_flag );
  copy_str( dst->give_item,  src->give_item );
  copy_str( dst->take_item,  src->take_item );
  copy_str( dst->set_lore,   src->set_lore );
  copy_str( dst->action,     src->action );
  dst->give_gold = src->give_gold;
}

/* ---- Deep copy an entire graph ---- */

void ne_GraphDeepCopy( NEGraph_t* dst, NEGraph_t* src )
{
  ne_GraphInit( dst );

  copy_str( dst->npc_key,      src->npc_key );
  copy_str( dst->display_name, src->display_name );
  copy_str( dst->glyph,        src->glyph );
  copy_str( dst->description,  src->description );
  copy_str( dst->image_path,   src->image_path );
  copy_str( dst->combat_bark,  src->combat_bark );
  copy_str( dst->idle_bark,    src->idle_bark );
  copy_str( dst->idle_log,     src->idle_log );
  copy_str( dst->action_label, src->action_label );

  memcpy( dst->color, src->color, sizeof( dst->color ) );
  dst->idle_cooldown = src->idle_cooldown;
  dst->combat        = src->combat;
  dst->damage        = src->damage;
  dst->no_face       = src->no_face;
  dst->no_shadow     = src->no_shadow;
  dst->dirty         = src->dirty;

  dst->num_nodes = src->num_nodes;
  for ( int i = 0; i < src->num_nodes; i++ )
    node_deep_copy( &dst->nodes[i], &src->nodes[i] );
}

/* ---- Undo stack ---- */

/* initialized = tracks whether snapshot slots have been inited */
static int slots_inited[NE_UNDO_MAX];

void ne_UndoInit( NEUndoStack_t* stack )
{
  stack->head       = 0;
  stack->count      = 0;
  stack->redo_count = 0;
  memset( slots_inited, 0, sizeof( slots_inited ) );
}

void ne_UndoDestroy( NEUndoStack_t* stack )
{
  for ( int i = 0; i < NE_UNDO_MAX; i++ )
  {
    if ( slots_inited[i] )
    {
      ne_GraphDestroy( &stack->snapshots[i] );
      slots_inited[i] = 0;
    }
  }
  stack->head       = 0;
  stack->count      = 0;
  stack->redo_count = 0;
}

void ne_UndoPush( NEUndoStack_t* stack, NEGraph_t* graph )
{
  int slot = stack->head;

  /* Free old snapshot in this slot if it was inited */
  if ( slots_inited[slot] )
    ne_GraphDestroy( &stack->snapshots[slot] );

  ne_GraphDeepCopy( &stack->snapshots[slot], graph );
  slots_inited[slot] = 1;

  stack->head = ( slot + 1 ) % NE_UNDO_MAX;

  if ( stack->count < NE_UNDO_MAX )
    stack->count++;

  /* Any new push kills the redo chain */
  stack->redo_count = 0;
}

int ne_Undo( NEUndoStack_t* stack, NEGraph_t* graph )
{
  if ( stack->count == 0 ) return 0;

  /* Save current state as redo before overwriting */
  int redo_slot = ( stack->head + stack->redo_count ) % NE_UNDO_MAX;
  if ( slots_inited[redo_slot] )
    ne_GraphDestroy( &stack->snapshots[redo_slot] );
  ne_GraphDeepCopy( &stack->snapshots[redo_slot], graph );
  slots_inited[redo_slot] = 1;
  stack->redo_count++;

  /* Move head back one */
  stack->head = ( stack->head - 1 + NE_UNDO_MAX ) % NE_UNDO_MAX;
  stack->count--;

  /* Restore snapshot at new head position */
  int src_slot = stack->head;

  /* Destroy current graph, deep copy from snapshot */
  ne_GraphDestroy( graph );
  ne_GraphDeepCopy( graph, &stack->snapshots[src_slot] );

  return 1;
}

int ne_Redo( NEUndoStack_t* stack, NEGraph_t* graph )
{
  if ( stack->redo_count == 0 ) return 0;

  /* Push current state as an undo point */
  int slot = stack->head;
  if ( slots_inited[slot] )
    ne_GraphDestroy( &stack->snapshots[slot] );
  ne_GraphDeepCopy( &stack->snapshots[slot], graph );
  slots_inited[slot] = 1;

  stack->head = ( slot + 1 ) % NE_UNDO_MAX;
  stack->count++;
  stack->redo_count--;

  /* Restore from the redo slot */
  int redo_slot = ( stack->head + stack->redo_count ) % NE_UNDO_MAX;

  ne_GraphDestroy( graph );
  ne_GraphDeepCopy( graph, &stack->snapshots[redo_slot] );

  return 1;
}
