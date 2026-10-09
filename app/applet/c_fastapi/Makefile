# ==============================================================================
# Makefile for C-FastAPI: Asynchronous Non-Blocking Web Framework in C11
# ==============================================================================

CC ?= gcc
CFLAGS ?= -Wall -Wextra -Wpedantic -O2 -std=c11 -D_GNU_SOURCE
DEBUG_CFLAGS = -Wall -Wextra -Wpedantic -g3 -O0 -std=c11 -D_GNU_SOURCE -fsanitize=address,undefined

INCLUDES = -Iinclude -Isrc
LDFLAGS = 

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
BIN_DIR = bin

TARGET = $(BIN_DIR)/fastapi_server
TARGET_DEBUG = $(BIN_DIR)/fastapi_server_debug

SRCS = $(SRC_DIR)/http_parser.c \
       $(SRC_DIR)/router.c \
       $(SRC_DIR)/response.c \
       $(SRC_DIR)/server.c \
       $(SRC_DIR)/api_routes.c \
       $(SRC_DIR)/main.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
DEBUG_OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%_debug.o, $(SRCS))

PORT ?= 8080
HOST ?= 0.0.0.0

.PHONY: all build debug run test clean help

all: build

help:
	@echo "Available targets in C-FastAPI Makefile:"
	@echo "  make build       - Compile release binary ($(TARGET))"
	@echo "  make debug       - Compile with debug symbols and AddressSanitizer"
	@echo "  make run         - Build and start server on PORT=$(PORT)"
	@echo "  make test        - Run automated API endpoint test suite"
	@echo "  make clean       - Remove compiled object files and binaries"

build: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@echo "  [LD]  $@"
	@$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)
	@echo "  ✨ Build complete: $@"

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "  [CC]  $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug: $(TARGET_DEBUG)

$(TARGET_DEBUG): $(DEBUG_OBJS)
	@mkdir -p $(BIN_DIR)
	@echo "  [LD-DEBUG]  $@"
	@$(CC) $(DEBUG_CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)
	@echo "  ✨ Debug build complete: $@"

$(BUILD_DIR)/%_debug.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	@echo "  [CC-DEBUG]  $<"
	@$(CC) $(DEBUG_CFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	@echo "  🚀 Starting C-FastAPI server on port $(PORT)..."
	@$(TARGET) $(PORT) $(HOST)

test: $(TARGET)
	@chmod +x test_server.sh
	@./test_server.sh $(PORT)

clean:
	@echo "  [RM]  Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "  🧹 Clean done."
