#include <stdlib.h>
#include <Archimedes.h>
#include <Daedalus.h>

#include "sound_manager.h"
#include "tween.h"

static aMusic_t music_menu;
static aMusic_t music_game;
static int game_music_active = 0;

/* Dungeon ambience — loops on AUDIO_CHANNEL_WEATHER with random volume drift */
static aSoundEffect_t ambience_dungeon;
static TweenManager_t amb_tweens;
static float amb_volume;

static void amb_fade_out( void* user_data );
static void amb_wait( void* user_data );

/* Phase 1: silent wait — sit at 0 for a random stretch */
static void amb_start_cycle( void* user_data )
{
  (void)user_data;
  amb_volume = 0;
  float wait = RANDF( 4.0f, 15.0f );
  d_LogInfoF( "[ambience] silent wait %.1fs", wait );
  TweenFloatWithCallback( &amb_tweens, &amb_volume, 0,
                          wait, TWEEN_LINEAR,
                          amb_wait, NULL );
}

static const TweenEasing_t amb_easings[] = {
  TWEEN_EASE_IN_QUAD, TWEEN_EASE_OUT_QUAD, TWEEN_EASE_IN_OUT_QUAD,
  TWEEN_EASE_IN_CUBIC, TWEEN_EASE_OUT_CUBIC, TWEEN_EASE_IN_OUT_CUBIC,
  TWEEN_LINEAR
};
#define AMB_NUM_EASINGS (int)( sizeof( amb_easings ) / sizeof( amb_easings[0] ) )

/* Phase 2: swell up to a random volume (40-80%) */
static void amb_wait( void* user_data )
{
  (void)user_data;
  float peak     = RANDF( 50, 102 );   /* ~40% to ~80% of 128 */
  float duration = RANDF( 2.0f, 12.0f );
  TweenEasing_t ease = amb_easings[ rand() % AMB_NUM_EASINGS ];
  d_LogInfoF( "[ambience] swell to %d/%d over %.1fs", (int)peak, AUDIO_MAX_VOLUME, duration );
  TweenFloatWithCallback( &amb_tweens, &amb_volume, peak,
                          duration, ease,
                          amb_fade_out, NULL );
}

/* Phase 3: fade back to silence, then restart the cycle */
static void amb_fade_out( void* user_data )
{
  (void)user_data;
  float duration = RANDF( 3.0f, 15.0f );
  TweenEasing_t ease = amb_easings[ rand() % AMB_NUM_EASINGS ];
  d_LogInfoF( "[ambience] fade out over %.1fs", duration );
  TweenFloatWithCallback( &amb_tweens, &amb_volume, 0,
                          duration, ease,
                          amb_start_cycle, NULL );
}

void SoundManagerInit( void )
{
  a_AudioLoadMusic( "resources/music/Soliloquy.ogg", &music_menu );
  a_AudioLoadMusic( "resources/music/Desolate.ogg", &music_game );
  a_AudioLoadSound( "resources/ambience/Forgoten_tombs.ogg", &ambience_dungeon );
}

void SoundManagerUpdate( float dt )
{
  UpdateTweens( &amb_tweens, dt );
  a_AudioSetChannelVolume( AUDIO_CHANNEL_WEATHER, (int)amb_volume );
}

void SoundManagerCrossfadeToGame( void )
{
  if ( game_music_active ) return;
  d_LogInfo( "[sound] crossfade to game music" );
  a_AudioSetMusicVolume( 80 );
  a_AudioPlayMusic( &music_game, -1, 500 );
  game_music_active = 1;
}

void SoundManagerPlayMenu( void )
{
  a_AudioStopMusic( MUSIC_FADE_MS );
  a_AudioSetMusicVolume( AUDIO_MAX_VOLUME );
  a_AudioPlayMusic( &music_menu, -1, MUSIC_FADE_MS );
  game_music_active = 0;

  /* Stop dungeon ambience */
  a_AudioHaltChannel( AUDIO_CHANNEL_WEATHER );
  StopAllTweens( &amb_tweens );
}

void SoundManagerPlayGame( void )
{
  /* Music crossfade may have already started during outro */
  if ( !game_music_active )
  {
    a_AudioStopMusic( MUSIC_FADE_MS );
    a_AudioSetMusicVolume( 80 );
    a_AudioPlayMusic( &music_game, -1, MUSIC_FADE_MS );
    game_music_active = 1;
  }

  /* Start dungeon ambience on weather channel, looping forever.
     Chunk volume stays at max — we control loudness via channel volume. */
  aAudioOptions_t opts = a_AudioDefaultOptions();
  opts.channel = AUDIO_CHANNEL_WEATHER;
  opts.loops   = -1;
  a_AudioPlaySound( &ambience_dungeon, &opts );
  a_AudioSetChannelVolume( AUDIO_CHANNEL_WEATHER, 0 );

  /* Kick off the random volume drift */
  StopAllTweens( &amb_tweens );
  amb_volume = 0;
  amb_start_cycle( NULL );
}

void SoundManagerStop( void )
{
  a_AudioStopMusic( MUSIC_FADE_MS );
  a_AudioHaltChannel( AUDIO_CHANNEL_WEATHER );
  StopAllTweens( &amb_tweens );
}
