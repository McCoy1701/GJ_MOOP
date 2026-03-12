/*
 * Lightweight Tweening System
 * Pure C + SDL2 - No external dependencies
 * Designed for potential inclusion in Archimedes framework
 *
 * Features:
 * - Fixed-size tween pool (no dynamic allocation)
 * - Basic easing functions
 * - Float value interpolation
 * - Optional callbacks on completion
 */

#ifndef TWEEN_H
#define TWEEN_H

#include <stdbool.h>
#include <stddef.h>  // For size_t and offsetof

// =============
// CONFIGURATION
// =============

#define TWEEN_MAX_ACTIVE 32  // Maximum simultaneous tweens

// =============
// EASING TYPES
// =============

typedef enum TweenEasing {
    TWEEN_LINEAR,
    TWEEN_EASE_IN_QUAD,
    TWEEN_EASE_OUT_QUAD,
    TWEEN_EASE_IN_OUT_QUAD,
    TWEEN_EASE_IN_CUBIC,
    TWEEN_EASE_OUT_CUBIC,
    TWEEN_EASE_IN_OUT_CUBIC,
    TWEEN_EASE_OUT_BOUNCE,
    TWEEN_EASE_IN_ELASTIC,
    TWEEN_EASE_OUT_ELASTIC,
} TweenEasing_t;

// =============
// TWEEN STRUCTURE
// =============

typedef void (*TweenCallback_t)(void* user_data);

typedef enum {
    TWEEN_TARGET_DIRECT,      // Direct pointer (for stack/static variables)
    TWEEN_TARGET_ARRAY_ELEM   // Array element (needs recalculation to avoid dangling pointers)
} TweenTargetType_t;

typedef struct Tween {
    bool active;              // Is this slot in use?

    // Target type (determines how to resolve pointer)
    TweenTargetType_t target_type;

    // For DIRECT targets (stack variables, static variables, heap-allocated structs)
    float* direct_target;

    // For ARRAY_ELEM targets (elements inside dArray_t - safe from reallocation)
    void* array_ptr;          // Pointer to dArray_t* (e.g., &manager->active_effects)
    size_t element_index;     // Index in array
    size_t float_offset;      // Byte offset to the float field (use offsetof)

    // Tween parameters
    float start_value;        // Starting value
    float end_value;          // Target value
    float duration;           // Total duration (seconds)
    float elapsed;            // Time elapsed (seconds)
    TweenEasing_t easing;     // Easing function
    TweenCallback_t on_complete; // Optional callback (can be NULL)
    void* user_data;          // User data passed to callback
} Tween_t;

// =============
// TWEEN MANAGER (Fixed Pool)
// =============

typedef struct TweenManager {
    Tween_t tweens[TWEEN_MAX_ACTIVE];  // Fixed array of tweens
    int active_count;                   // Number of active tweens
    int highest_active_slot;            // Highest index with active tween (-1 if none active)
} TweenManager_t;

// =============
// LIFECYCLE
// =============

void InitTweenManager(TweenManager_t* manager);
void CleanupTweenManager(TweenManager_t* manager);

// =============
// TWEEN CREATION
// =============

bool TweenFloat(TweenManager_t* manager, float* target, float end_value,
                float duration, TweenEasing_t easing);

bool TweenFloatInArray(TweenManager_t* manager,
                       void* array_ptr,
                       size_t element_index,
                       size_t float_offset,
                       float end_value,
                       float duration,
                       TweenEasing_t easing);

bool TweenFloatWithCallback(TweenManager_t* manager, float* target, float end_value,
                             float duration, TweenEasing_t easing,
                             TweenCallback_t on_complete, void* user_data);

// =============
// UPDATE
// =============

void UpdateTweens(TweenManager_t* manager, float dt);

// =============
// CONTROL
// =============

int StopTweensForTarget(TweenManager_t* manager, float* target);
int StopAllTweens(TweenManager_t* manager);
int GetActiveTweenCount(const TweenManager_t* manager);

// =============
// UTILITY (Query & Debug)
// =============

bool IsTweenActive(const TweenManager_t* manager, const float* target);
float GetTweenProgress(const TweenManager_t* manager, const float* target);

#endif // TWEEN_H
