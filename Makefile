# Makefile for cutter - Linear Cutting Optimizer
# Requires: libglpk-dev libcairo2-dev libsqlite3-dev libgtk-4-dev

CC = gcc
CFLAGS_BASE = -Wall -Wextra -std=c11 -O2 -g
CFLAGS_CLI = $(CFLAGS_BASE) $(shell pkg-config --cflags cairo sqlite3)
CFLAGS_GUI = $(CFLAGS_BASE) $(shell pkg-config --cflags gtk4 cairo sqlite3)
LDFLAGS_CLI = -lglpk -lm $(shell pkg-config --libs cairo sqlite3)
LDFLAGS_GUI = -lglpk -lm $(shell pkg-config --libs gtk4 cairo sqlite3)

# Directories
SRC_DIR = src
GUI_DIR = src/gui
OBJ_DIR = obj
OBJ_GUI_DIR = obj/gui
BIN_DIR = bin

# Core source files (shared between CLI and GUI)
CORE_SRCS = $(SRC_DIR)/colgen.c \
            $(SRC_DIR)/glpk_master.c \
            $(SRC_DIR)/knapsack.c \
            $(SRC_DIR)/csv_io.c \
            $(SRC_DIR)/pdf_export.c \
            $(SRC_DIR)/db.c \
            $(SRC_DIR)/settings.c \
            $(SRC_DIR)/ods_io.c

CORE_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CORE_SRCS))

# CLI-specific
CLI_SRCS = $(SRC_DIR)/main.c
CLI_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(CLI_SRCS))
CLI_TARGET = $(BIN_DIR)/cutter

# Benchmark
BENCH_SRCS = $(SRC_DIR)/benchmark.c
BENCH_OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(BENCH_SRCS))
BENCH_TARGET = $(BIN_DIR)/cutter-benchmark

# GUI-specific
GUI_SRCS = $(SRC_DIR)/gui_main.c \
           $(GUI_DIR)/cutter_app.c \
           $(GUI_DIR)/cutter_window.c \
           $(GUI_DIR)/views/inventory_view.c \
           $(GUI_DIR)/views/pieces_view.c \
           $(GUI_DIR)/views/optimize_view.c \
           $(GUI_DIR)/views/results_view.c \
           $(GUI_DIR)/views/settings_view.c \
           $(GUI_DIR)/models/stock_object.c \
           $(GUI_DIR)/models/piece_object.c

GUI_OBJS = $(OBJ_DIR)/gui_main.o \
           $(OBJ_GUI_DIR)/cutter_app.o \
           $(OBJ_GUI_DIR)/cutter_window.o \
           $(OBJ_GUI_DIR)/views/inventory_view.o \
           $(OBJ_GUI_DIR)/views/pieces_view.o \
           $(OBJ_GUI_DIR)/views/optimize_view.o \
           $(OBJ_GUI_DIR)/views/results_view.o \
           $(OBJ_GUI_DIR)/views/settings_view.o \
           $(OBJ_GUI_DIR)/models/stock_object.o \
           $(OBJ_GUI_DIR)/models/piece_object.o

# Core objects compiled with GUI flags for GUI build
CORE_GUI_OBJS = $(patsubst $(OBJ_DIR)/%.o,$(OBJ_GUI_DIR)/core/%.o,$(CORE_OBJS))

GUI_TARGET = $(BIN_DIR)/cutter-gtk

.PHONY: all cli gui clean demo install benchmark

# Default: build GUI
all: gui

# CLI target
cli: $(CLI_TARGET)

# GUI target
gui: $(GUI_TARGET)

# Benchmark target
benchmark: $(BENCH_TARGET)

# Create directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_GUI_DIR):
	mkdir -p $(OBJ_GUI_DIR)/views $(OBJ_GUI_DIR)/models $(OBJ_GUI_DIR)/core

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile core objects for CLI
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS_CLI) -c $< -o $@

# Compile core objects for GUI (need GTK flags for headers)
$(OBJ_GUI_DIR)/core/%.o: $(SRC_DIR)/%.c | $(OBJ_GUI_DIR)
	$(CC) $(CFLAGS_GUI) -c $< -o $@

# Compile GUI main
$(OBJ_DIR)/gui_main.o: $(SRC_DIR)/gui_main.c | $(OBJ_DIR)
	$(CC) $(CFLAGS_GUI) -c $< -o $@

# Compile GUI modules
$(OBJ_GUI_DIR)/%.o: $(GUI_DIR)/%.c | $(OBJ_GUI_DIR)
	$(CC) $(CFLAGS_GUI) -c $< -o $@

$(OBJ_GUI_DIR)/views/%.o: $(GUI_DIR)/views/%.c | $(OBJ_GUI_DIR)
	$(CC) $(CFLAGS_GUI) -c $< -o $@

$(OBJ_GUI_DIR)/models/%.o: $(GUI_DIR)/models/%.c | $(OBJ_GUI_DIR)
	$(CC) $(CFLAGS_GUI) -c $< -o $@

# Link CLI
$(CLI_TARGET): $(CORE_OBJS) $(CLI_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS_CLI)

# Link GUI
$(GUI_TARGET): $(CORE_GUI_OBJS) $(GUI_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS_GUI)

# Link Benchmark
$(BENCH_TARGET): $(CORE_OBJS) $(BENCH_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS_CLI)

# Run demo (CLI)
demo: cli
	./$(CLI_TARGET) --demo

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Install
PREFIX ?= /usr/local
install: cli
	install -d $(PREFIX)/bin
	install -m 755 $(CLI_TARGET) $(PREFIX)/bin/

install-gui: gui
	install -d $(PREFIX)/bin
	install -m 755 $(GUI_TARGET) $(PREFIX)/bin/

# Dependencies
$(OBJ_DIR)/main.o: $(SRC_DIR)/csp_types.h $(SRC_DIR)/colgen.h $(SRC_DIR)/csv_io.h $(SRC_DIR)/pdf_export.h $(SRC_DIR)/db.h
$(OBJ_DIR)/colgen.o: $(SRC_DIR)/csp_types.h $(SRC_DIR)/colgen.h $(SRC_DIR)/glpk_master.h $(SRC_DIR)/knapsack.h
$(OBJ_DIR)/glpk_master.o: $(SRC_DIR)/csp_types.h $(SRC_DIR)/glpk_master.h
$(OBJ_DIR)/knapsack.o: $(SRC_DIR)/csp_types.h $(SRC_DIR)/knapsack.h
$(OBJ_DIR)/csv_io.o: $(SRC_DIR)/csp_types.h $(SRC_DIR)/csv_io.h
$(OBJ_DIR)/pdf_export.o: $(SRC_DIR)/csp_types.h $(SRC_DIR)/pdf_export.h
$(OBJ_DIR)/db.o: $(SRC_DIR)/csp_types.h $(SRC_DIR)/db.h
