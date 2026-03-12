/*
 * npc_editor.h:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#ifndef __NPC_EDITOR_H__
#define __NPC_EDITOR_H__

#include "ne_structs.h"

/* Currently loaded NPC graph */
extern NEGraph_t   g_ne_graph;
extern int         g_ne_graph_loaded;
extern int         g_ne_selected_node;

/* NPC file list */
extern NEFileEntry_t  g_ne_files[NE_MAX_NPC_FILES];
extern int            g_ne_num_files;
extern int            g_ne_selected_file;

/* Folder expand/collapse state */
extern NEFolderState_t g_ne_folders[NE_MAX_FOLDERS];
extern int             g_ne_num_folders;

/* Tab entry / exit */
void e_NPCEditorInit( void );
void e_NPCEditorDestroy( void );

/* Node / graph init-destroy helpers */
void ne_NodeInit( NENode_t* node );
void ne_NodeDestroy( NENode_t* node );
void ne_GraphInit( NEGraph_t* graph );
void ne_GraphDestroy( NEGraph_t* graph );
void ne_FileEntryInit( NEFileEntry_t* fe );
void ne_FileEntryDestroy( NEFileEntry_t* fe );

/* Browse panel */
void ne_ScanNPCFiles( const char* basedir );
int  ne_LoadNPCFile( const char* path, const char* stem );

/* Graph canvas (ne_Graph.c) */
void  ne_GraphClearInput( void );
void  ne_GraphReset( void );
float ne_GraphCamX( void );
float ne_GraphCamY( void );
void  ne_GraphLogic( aRectf_t panel );
void  ne_GraphDraw( aRectf_t panel );
int   ne_GraphCtxMenuOpen( void );
void  ne_GraphCtxMenuClose( void );
float ne_GraphCtxMenuWX( void );
float ne_GraphCtxMenuWY( void );
int   ne_GraphCtxMenuSX( void );
int   ne_GraphCtxMenuSY( void );
int   ne_GraphCtxMenuNode( void );

/* Auto-layout (ne_Layout.c) */
void ne_AutoLayout( void );

/* Undo/redo (ne_Undo.c) */
void ne_GraphDeepCopy( NEGraph_t* dst, NEGraph_t* src );
void ne_UndoInit( NEUndoStack_t* stack );
void ne_UndoDestroy( NEUndoStack_t* stack );
void ne_UndoPush( NEUndoStack_t* stack, NEGraph_t* graph );
int  ne_Undo( NEUndoStack_t* stack, NEGraph_t* graph );
int  ne_Redo( NEUndoStack_t* stack, NEGraph_t* graph );

/* Text field editing (ne_Edit.c) */
void ne_FieldClear( void );
int  ne_FieldIsActive( void );
int  ne_FieldIsTarget( dString_t* target );
int  ne_FieldIsIntTarget( int* target );
void ne_FieldActivate( dString_t* target );
void ne_FieldActivateInt( int* target );
int  ne_FieldCommit( void );
void ne_FieldCancel( void );
void ne_FieldLogic( float dt );

int  ne_DrawFieldRow( const char* label, const char* value,
                      int is_active, int x, int y, int w, int* clicked );
int  ne_DrawToggleRow( const char* label, int value,
                       int x, int y, int w, int* clicked );
int  ne_DrawSectionHeader( const char* text, int x, int y );
int  ne_DrawArrayField( const char* label, dString_t* items[], int count,
                        int max, int x, int y, int w,
                        int active_index, int* click_idx,
                        int* click_add, int* click_remove );

#endif
