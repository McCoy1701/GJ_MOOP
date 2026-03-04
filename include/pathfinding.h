#ifndef __PATHFINDING_H__
#define __PATHFINDING_H__

#define PATH_MAX_LEN 256

typedef struct { int row, col; } PathNode_t;

/* Returns path length (0 = no path).  Path stored in out[] from start to goal.
   blocker_fn returns 1 if tile is blocked (walls + entities, caller decides).
   The goal tile is exempt from the blocker check. */
int PathfindAStar( int start_r, int start_c, int goal_r, int goal_c,
                   int grid_w, int grid_h,
                   int (*blocked)( int r, int c, void* ctx ), void* ctx,
                   PathNode_t out[PATH_MAX_LEN] );

#endif
