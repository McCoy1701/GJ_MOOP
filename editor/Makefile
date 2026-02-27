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
EE_DIR     = src/entity_editor
WE_DIR     = src/world_editor

# Object Directories (Separated for different build types)
OBJ_DIR_NATIVE = obj/native
OBJ_DIR_EE     = obj/native/entity_editor
OBJ_DIR_WE     = obj/native/world_editor

#Flags
CINC = -I$(INC_DIR)/
LDLIBS = -lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm

C_FLAGS = -std=c99 -Wall -Wextra $(CINC)
NATIVE_C_FLAGS = $(C_FLAGS) -ggdb -lArchimedes -lDaedalus


# ====================================================================
# EDITOR LIBRARY OBJECTS (Core C Files)
# ====================================================================

EDITOR_SRCS = editor.c \
							entity_editor.c\
							items_editor.c\
							utils.c\
							world_editor.c

ENTITY_EDITOR_SRCS = ee_Creation.c\
										 ee_Edit.c

WORLD_EDITOR_SRCS = we_Creation.c\
										we_Edit.c\
										we_Load.c\
										we_Save.c

NATIVE_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_NATIVE)/%.o, $(EDITOR_SRCS))
ENTITY_EDITOR_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_EE)/%.o, $(ENTITY_EDITOR_SRCS))
WORLD_EDITOR_LIB_OBJS = $(patsubst %.c, $(OBJ_DIR_WE)/%.o, $(WORLD_EDITOR_SRCS))

MAIN_OBJ = $(OBJ_DIR_NATIVE)/main.o

NATIVE_EXE_OBJS = $(ENTITY_EDITOR_LIB_OBJS) $(WORLD_EDITOR_LIB_OBJS) $(NATIVE_LIB_OBJS) $(MAIN_OBJ)


# ====================================================================
# PHONY TARGETS
# ====================================================================

.PHONY: all clean bear bearclean
all: $(BIN_DIR)/native


# ====================================================================
# DIRECTORY & UTILITY RULES
# ====================================================================

# Ensure the directories exist before attempting to write files to them
$(BIN_DIR) $(OBJ_DIR_NATIVE) $(OBJ_DIR_EE) $(OBJ_DIR_WE) $(OBJ_DIR_EM) $(INDEX_DIR) $(OBJ_DIR_SCENES):
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

$(OBJ_DIR_NATIVE)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR_NATIVE)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_EE)/%.o: $(EE_DIR)/%.c | $(OBJ_DIR_EE)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_WE)/%.o: $(WE_DIR)/%.c | $(OBJ_DIR_WE)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)

$(OBJ_DIR_NATIVE)/main.o: $(SRC_DIR)/main.c | $(OBJ_DIR_NATIVE)
	$(CC) -c $< -o $@ $(NATIVE_C_FLAGS)


# ====================================================================
# LINKING RULES
# ====================================================================

# Target: Native Executable
$(BIN_DIR)/native: $(NATIVE_EXE_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(NATIVE_C_FLAGS) $(LDLIBS)

