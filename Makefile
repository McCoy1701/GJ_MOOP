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

# Object Directories (Separated for different build types)
OBJ_DIR_NATIVE = obj/native
OBJ_DIR_SCENES = obj/native/scenes
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
							class_select.c \
							settings.c

GJ_MOOP_SRCS = mechanics.c \
							items.c \
							draw_utils.c \
							console.c \
							game_events.c \

NATIVE_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_NATIVE)/%.o, $(GJ_MOOP_SRCS))
SCENES_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_SCENES)/%.o, $(SCENES_SRCS))
EMCC_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_EM)/%.o, $(GJ_MOOP_SRCS))
EMCC_SCENES_OBJS = $(patsubst %.c, $(OBJ_DIR_EM)/%.o, $(SCENES_SRCS))

MAIN_OBJ = $(OBJ_DIR_NATIVE)/main.o
EM_MAIN_OBJ = $(OBJ_DIR_EM)/main.o

NATIVE_EXE_OBJS = $(SCENES_LIB_OBJS) $(NATIVE_LIB_OBJS) $(MAIN_OBJ)
EMCC_EXE_OBJS = $(EMCC_SCENES_OBJS) $(EMCC_LIB_OBJS) $(EM_MAIN_OBJ)

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
$(BIN_DIR) $(OBJ_DIR_NATIVE) $(OBJ_DIR_EM) $(INDEX_DIR) $(OBJ_DIR_SCENES):
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

$(OBJ_DIR_NATIVE)/main.o: $(SRC_DIR)/main.c | $(OBJ_DIR_NATIVE)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)


# ====================================================================
# COMPILATION RULES (Emscripten - ECC)
# ====================================================================

$(OBJ_DIR_EM)/%.o: $(GAME_DIR)/%.c | $(OBJ_DIR_EM)
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

