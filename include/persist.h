#ifndef __PERSIST_H__
#define __PERSIST_H__

/*
 * PersistSave - store a null-terminated string under a key
 * Returns 0 on success, 1 on failure.
 *
 * Native:      writes to "saves/<key>.txt"
 * Emscripten:  writes to localStorage as "odd_<key>"
 */
int PersistSave( const char* key, const char* data );

/*
 * PersistLoad - load a string by key
 * Returns heap-allocated string (caller must free), or NULL if not found.
 *
 * Native:      reads from "saves/<key>.txt"
 * Emscripten:  reads from localStorage "odd_<key>"
 */
char* PersistLoad( const char* key );

/*
 * PersistDelete - remove saved data for a key
 * Returns 0 on success, 1 on failure.
 */
int PersistDelete( const char* key );

/*
 * PersistInit - call once at startup
 * Native:      creates saves/ directory if missing
 * Emscripten:  no-op
 */
void PersistInit( void );

#endif
