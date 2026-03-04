# ====================================================================
# PROJECT CONFIGURATION
# ====================================================================

# Compiler and Tools
CC = gcc
ECC = emcc

# Directories
SRC_DIR    = src
OBJ_DIR    = obj
INC_DIR    = include
BIN_DIR    = bin
LIB_DIR    = lib
INDEX_DIR  = index
SCENES_DIR = src/scenes
GAME_DIR   = src/game
UI_DIR     = src/game/ui
UTILS_DIR  = src/game/utils
PLAYER_DIR = src/game/player
SYS_DIR    = src/game/systems
WORLD_DIR    = src/game/world
ENEMIES_DIR  = src/game/entities/enemies
NPC_DIR      = src/game/entities/npc
GROUND_DIR   = src/game/entities/ground_items
DUNGEON_DIR  = src/game/dungeon

# Object Directories (Separated for different build types)
OBJ_DIR_NATIVE = obj/native
OBJ_DIR_SCENES = obj/native/scenes
OBJ_DIR_UI     = obj/native/ui
OBJ_DIR_UTILS  = obj/native/utils
OBJ_DIR_PLAYER = obj/native/player
OBJ_DIR_SYS    = obj/native/systems
OBJ_DIR_WORLD    = obj/native/world
OBJ_DIR_ENEMIES  = obj/native/enemies
OBJ_DIR_NPC      = obj/native/npc
OBJ_DIR_GROUND   = obj/native/ground_items
OBJ_DIR_DUNGEON  = obj/native/dungeon
OBJ_DIR_EM     = obj/em

#Flags
CINC = -I$(INC_DIR)/
LDLIBS = -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm
EFLAGS = -s USE_SDL=2 -s USE_SDL_IMAGE=2 -s SDL2_IMAGE_FORMATS='["png"]' -s USE_SDL_MIXER=2 -s USE_SDL_TTF=2 -sALLOW_MEMORY_GROWTH

ARCHIMEDES_INC = ../Archimedes/include
DAEDALUS_INC   = ../Daedalus/include

C_FLAGS = -std=c99 -Wall -Wextra $(CINC)
NATIVE_C_FLAGS = $(C_FLAGS) -ggdb -lArchimedes -lDaedalus
EMSCRIP_C_FLAGS = $(C_FLAGS) -I$(ARCHIMEDES_INC) -I$(DAEDALUS_INC) $(EFLAGS)

# ====================================================================
# GAME JAM MOOP LIBRARY OBJECTS (Core C Files)
# ====================================================================

SCENES_SRCS = main_menu.c \
							game_scene.c \
							game_camera.c \
							game_turns.c \
							game_input.c \
							class_select.c \
							settings.c \
							lore_scene.c

GJ_MOOP_SRCS = console.c\

UI_SRCS     = inventory_ui.c \
						tile_actions.c \
						look_mode.c \
						hud.c \
						quest_tracker.c \
						shop.c \
						shop_ui.c \
						target_mode.c \
						pause_menu.c \
						game_over.c
UTILS_SRCS  = draw_utils.c \
							context_menu.c
PLAYER_SRCS = items.c \
						maps.c \
						movement.c \
						rendering.c

SYS_SRCS = tween.c \
					 game_events.c \
					 transitions.c \
					 sound_manager.c \
					 combat.c \
					 combat_vfx.c \
					 spell_vfx.c \
					 lore.c \
					 persist.c \
					 poison_pool.c \
					 placed_traps.c \
					 floor_cutscene.c \
					 dev_mode.c \
					 bank.c \
					 npc_relocate.c \
					 pathfinding.c

WORLD_SRCS = world.c \
						 game_viewport.c \
						 visibility.c

ENEMIES_SRCS = enemies.c \
							 enemy_utils.c \
							 enemy_rat.c \
							 enemy_skeleton.c \
							 enemy_shaman.c \
							 rendering.c

NPC_SRCS = npc.c \
					 dialogue.c \
					 dialogue_ui.c

GROUND_SRCS = ground_items.c

DUNGEON_SRCS = dungeon_builder.c \
							 dungeon_spawner.c \
							 dungeon_handler.c \
							 doors.c \
							 objects.c \
							 room_enumerator.c \
							 interactive_tile.c

NATIVE_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_NATIVE)/%.o, $(GJ_MOOP_SRCS))
SCENES_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_SCENES)/%.o, $(SCENES_SRCS))
UI_LIB_OBJS     = $(patsubst %.c, $(OBJ_DIR_UI)/%.o, $(UI_SRCS))
UTILS_LIB_OBJS  = $(patsubst %.c, $(OBJ_DIR_UTILS)/%.o, $(UTILS_SRCS))
PLAYER_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_PLAYER)/%.o, $(PLAYER_SRCS))
SYS_LIB_OBJS    = $(patsubst %.c, $(OBJ_DIR_SYS)/%.o, $(SYS_SRCS))
WORLD_LIB_OBJS    = $(patsubst %.c, $(OBJ_DIR_WORLD)/%.o, $(WORLD_SRCS))
ENEMIES_LIB_OBJS  = $(patsubst %.c, $(OBJ_DIR_ENEMIES)/%.o, $(ENEMIES_SRCS))
NPC_LIB_OBJS      = $(patsubst %.c, $(OBJ_DIR_NPC)/%.o, $(NPC_SRCS))
GROUND_LIB_OBJS   = $(patsubst %.c, $(OBJ_DIR_GROUND)/%.o, $(GROUND_SRCS))
DUNGEON_LIB_OBJS  = $(patsubst %.c, $(OBJ_DIR_DUNGEON)/%.o, $(DUNGEON_SRCS))
EMCC_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_EM)/%.o, $(GJ_MOOP_SRCS))
EMCC_SCENES_OBJS = $(patsubst %.c, $(OBJ_DIR_EM)/%.o, $(SCENES_SRCS))
EMCC_UI_OBJS     = $(patsubst %.c, $(OBJ_DIR_EM)/%.o, $(UI_SRCS))
EMCC_UTILS_OBJS  = $(patsubst %.c, $(OBJ_DIR_EM)/%.o, $(UTILS_SRCS))
EMCC_PLAYER_OBJS = $(patsubst %.c, $(OBJ_DIR_EM)/%.o, $(PLAYER_SRCS))
EMCC_SYS_OBJS   = $(patsubst %.c, $(OBJ_DIR_EM)/%.o, $(SYS_SRCS))

MAIN_OBJ = $(OBJ_DIR_NATIVE)/main.o
EM_MAIN_OBJ = $(OBJ_DIR_EM)/main.o

NATIVE_EXE_OBJS = $(SCENES_LIB_OBJS) $(NATIVE_LIB_OBJS) $(UI_LIB_OBJS) $(UTILS_LIB_OBJS) $(PLAYER_LIB_OBJS) $(SYS_LIB_OBJS) $(WORLD_LIB_OBJS) $(ENEMIES_LIB_OBJS) $(NPC_LIB_OBJS) $(GROUND_LIB_OBJS) $(DUNGEON_LIB_OBJS) $(MAIN_OBJ)
EMCC_EXE_OBJS = $(EMCC_SCENES_OBJS) $(EMCC_LIB_OBJS) $(EMCC_UI_OBJS) $(EMCC_UTILS_OBJS) $(EMCC_PLAYER_OBJS) $(EMCC_SYS_OBJS) $(EM_MAIN_OBJ)

# ====================================================================
# PHONY TARGETS
# ====================================================================

.PHONY: all em clean bear bearclean
all: $(BIN_DIR)/native

# Emscripten Targets
em: $(INDEX_DIR)/index

# ====================================================================
# DIRECTORY & UTILITY RULES
# ====================================================================

# Ensure the directories exist before attempting to write files to them
$(BIN_DIR) $(OBJ_DIR_NATIVE) $(OBJ_DIR_EM) $(INDEX_DIR) $(OBJ_DIR_SCENES) $(OBJ_DIR_UI) $(OBJ_DIR_UTILS) $(OBJ_DIR_PLAYER) $(OBJ_DIR_SYS) $(OBJ_DIR_WORLD) $(OBJ_DIR_ENEMIES) $(OBJ_DIR_NPC) $(OBJ_DIR_GROUND) $(OBJ_DIR_DUNGEON):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) $(INDEX_DIR)
	@clear

bear:
	bear -- make

bearclean:
	rm compile_commands.json

# ====================================================================
# COMPILATION RULES
# ====================================================================

$(OBJ_DIR_SCENES)/%.o: $(SCENES_DIR)/%.c | $(OBJ_DIR_SCENES)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_NATIVE)/%.o: $(GAME_DIR)/%.c | $(OBJ_DIR_NATIVE)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_UI)/%.o: $(UI_DIR)/%.c | $(OBJ_DIR_UI)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_UTILS)/%.o: $(UTILS_DIR)/%.c | $(OBJ_DIR_UTILS)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_PLAYER)/%.o: $(PLAYER_DIR)/%.c | $(OBJ_DIR_PLAYER)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_SYS)/%.o: $(SYS_DIR)/%.c | $(OBJ_DIR_SYS)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_WORLD)/%.o: $(WORLD_DIR)/%.c | $(OBJ_DIR_WORLD)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_ENEMIES)/%.o: $(ENEMIES_DIR)/%.c | $(OBJ_DIR_ENEMIES)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_NPC)/%.o: $(NPC_DIR)/%.c | $(OBJ_DIR_NPC)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_GROUND)/%.o: $(GROUND_DIR)/%.c | $(OBJ_DIR_GROUND)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_DUNGEON)/%.o: $(DUNGEON_DIR)/%.c | $(OBJ_DIR_DUNGEON)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_NATIVE)/main.o: $(SRC_DIR)/main.c | $(OBJ_DIR_NATIVE)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)


# ====================================================================
# COMPILATION RULES (Emscripten - ECC)
# ====================================================================

$(OBJ_DIR_EM)/%.o: $(GAME_DIR)/%.c | $(OBJ_DIR_EM)
	$(ECC) -c $< -o $@ $(EMSCRIP_C_FLAGS)

$(OBJ_DIR_EM)/%.o: $(UI_DIR)/%.c | $(OBJ_DIR_EM)
	$(ECC) -c $< -o $@ $(EMSCRIP_C_FLAGS)

$(OBJ_DIR_EM)/%.o: $(UTILS_DIR)/%.c | $(OBJ_DIR_EM)
	$(ECC) -c $< -o $@ $(EMSCRIP_C_FLAGS)

$(OBJ_DIR_EM)/%.o: $(PLAYER_DIR)/%.c | $(OBJ_DIR_EM)
	$(ECC) -c $< -o $@ $(EMSCRIP_C_FLAGS)

$(OBJ_DIR_EM)/%.o: $(SYS_DIR)/%.c | $(OBJ_DIR_EM)
	$(ECC) -c $< -o $@ $(EMSCRIP_C_FLAGS)

$(OBJ_DIR_EM)/%.o: $(WORLD_DIR)/%.c | $(OBJ_DIR_EM)
	$(ECC) -c $< -o $@ $(EMSCRIP_C_FLAGS)

$(OBJ_DIR_EM)/%.o: $(ENEMIES_DIR)/%.c | $(OBJ_DIR_EM)
	$(ECC) -c $< -o $@ $(EMSCRIP_C_FLAGS)

$(OBJ_DIR_EM)/%.o: $(SCENES_DIR)/%.c | $(OBJ_DIR_EM)
	$(ECC) -c $< -o $@ $(EMSCRIP_C_FLAGS)

$(OBJ_DIR_EM)/main.o: $(SRC_DIR)/main.c | $(OBJ_DIR_EM)
	$(ECC) -c $< -o $@ $(EMSCRIP_C_FLAGS)

# ====================================================================
# LINKING RULES
# ====================================================================

# Target: Native Executable
$(BIN_DIR)/native: $(NATIVE_EXE_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(NATIVE_C_FLAGS) $(LDLIBS)

$(INDEX_DIR)/index: $(EMCC_EXE_OBJS) $(LIB_DIR)/libArchimedes.a $(LIB_DIR)/libDaedalus.a | $(INDEX_DIR)
	$(ECC) $^ -s WASM=1 $(EFLAGS) --shell-file htmlTemplate/template.html --preload-file resources/ -o $@.html

