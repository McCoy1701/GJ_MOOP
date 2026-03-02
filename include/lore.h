#ifndef __LORE_H__
#define __LORE_H__

#define MAX_LORE_ENTRIES   64
#define MAX_LORE_FLOORS     4

typedef struct
{
  char key[64];
  char title[128];
  char category[32];
  char description[512];
  int  discovered;
  int  floor;
} LoreEntry_t;

void LoreLoadDefinitions( void );
void LoreLoadSave( void );
void LoreSave( void );

/* Returns 1 if newly unlocked, 0 if already known or not found */
int  LoreUnlock( const char* key );
int  LoreIsDiscovered( const char* key );
void LoreResetAll( void );

int          LoreGetCount( void );
LoreEntry_t* LoreGetEntry( int index );
int          LoreCountDiscovered( void );
int          LoreCountInCategory( const char* category );

/* Fill out[] with unique category names. Returns count. */
int LoreGetCategories( char out[][32], int max );
int LoreGetFloorCategories( int floor, char out[][32], int max );
int LoreGetFloors( int out[], int max );
int LoreCountInFloorCategory( int floor, const char* category );

#endif
