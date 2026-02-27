
#include <stdio.h>
#include <stdlib.h>

#include <Archimedes.h>
#include <Daedalus.h>

#include "ed_defines.h"
#include "ed_structs.h"


void e_LoadColorPalette( aColor_t palette[MAX_COLOR_GROUPS][MAX_COLOR_PALETTE],
                       const char * filename )
{
  FILE* file;
  char line[8];
  unsigned int hex_value;
  uint8_t r, g, b;
  int i = 0;

  file = fopen( filename, "rb" );
  if ( file == NULL )
  {
    printf( "Failed to open file %s\n", filename );
    return;
  } 

  while( fgets( line, sizeof( line ), file ) != NULL )
  {
    hex_value = ( unsigned int )strtol( line, NULL, 16 );
    r = hex_value >> 16;
    g = hex_value >> 8;
    b = hex_value;
    
    if ( i >= 0 && i < MAX_COLOR_PALETTE )
    {
      palette[APOLLO_PALETE][i].r = r;
      palette[APOLLO_PALETE][i].g = g;
      palette[APOLLO_PALETE][i].b = b;
      palette[APOLLO_PALETE][i].a = 255;

    }

    i++;

  }

  fclose( file );

}
