CC = gcc-14

IFLAGS = -I$(INCLUDE_DIR) -I$(LIBRARY_DIR)
LFLAGS = -L$(LIBRARY_DIR) -Llib/tree_sitter
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra
CFLAGS += -fopenmp
LDFLAGS += -fopenmp
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer

DEBUG_FLAGS = -O0 -DDEBUG=1 -g

SRC_DIR = src
INCLUDE_DIR = include
BIN_DIR = bin
BUILD_DIR = build
BUILD_RELEASE_DIR = release
BUILD_DEBUG_DIR = debug

LIBRARY_DIR = lib
LIBS = tree-sitter
LLIBS := $(patsubst %,-l%,$(LIBS))

TARGET = analyzer
NO_LOG_TARGET = analyzer_no_log
DEBUG_TARGET = analyzer_debug

LOCAL_DEPS := $(wildcard $(INCLUDE_DIR)/*.h)
LIB_DEPS := $(wildcard $(LIBRARY_DIR)/*/*.h)
DEPS := $(LOCAL_DEPS) $(LIB_DEPS)

LOCAL_SOURCES := $(wildcard $(SRC_DIR)/*.c)
LIB_SOURCES := $(wildcard $(LIBRARY_DIR)/*/*.c)
SOURCES := $(LOCAL_SOURCES) $(LIB_SOURCES)

OBJ = $(patsubst %.c,$(BUILD_DIR)/$(BUILD_RELEASE_DIR)/%.o, $(SOURCES))
DEBUG_OBJ = $(patsubst %.c,$(BUILD_DIR)/$(BUILD_DEBUG_DIR)/%.o, $(SOURCES))

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^ $(LFLAGS) $(LDFLAGS) $(LLIBS)

$(BUILD_DIR)/$(BUILD_RELEASE_DIR)/%.o: %.c $(DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

debug: $(DEBUG_TARGET)

$(DEBUG_TARGET): $(DEBUG_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/$@ $^ $(LFLAGS) $(LDFLAGS) $(LLIBS)

$(BUILD_DIR)/$(BUILD_DEBUG_DIR)/%.o: %.c $(DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(IFLAGS) $(DEBUG_FLAGS) -c $< -o $@

ASAN_TARGET = analyzer_asan
ASAN_OBJ = $(DEBUG_OBJ)

asan: $(ASAN_TARGET)

$(ASAN_TARGET): $(ASAN_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(ASAN_FLAGS) -o $(BIN_DIR)/$@ $^ $(LFLAGS) $(LDFLAGS) $(ASAN_FLAGS) $(LLIBS)


.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
