# Copyright (c) 2023 Renesas Electronics Corp.
# SPDX-License-Identifier: MIT-0

# Add compile flags
CFLAGS = -Wall -Wextra -Werror

# Add linking flags
LDFLAGS = -lm                                    \
          -lmmngr                                \
          -lmmngrbuf                             \
          -lomxr_core                            \
          -lpthread                              \
          $(shell pkg-config egl --libs)         \
          $(shell pkg-config glesv2 --libs)      \
          $(shell pkg-config freetype2 --libs)   \
          $(shell pkg-config wayland-egl --libs)

# Define directories
OBJ_DIR = ./objs
TTF_DIR = ./common/ttf
CMN_SRC_DIR = ./common/src
CMN_INC_DIR = ./common/inc

CFLAGS += -I$(CMN_INC_DIR) $(shell pkg-config freetype2 --cflags)

# Define variables for Wayland
WL_PROTOCOLS_DIR   = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
XDG_SHELL_PROTOCOL = $(WL_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml

WL_SRC = xdg-shell-protocol.c
WL_INC = xdg-shell-client-protocol.h

# Get common source files
CMN_SRCS = $(CMN_SRC_DIR)/$(WL_SRC)                 \
           $(shell find $(CMN_SRC_DIR) -name '*.c')

# Get common object files
CMN_OBJS = $(CMN_SRCS:%.c=$(OBJ_DIR)/%.o)

# Define TrueType font
TTF_FILE = LiberationSans-Regular.ttf

# Define sample apps
APP_DIRS = ./h264-to-file     \
           ./raw-video-to-lcd

APPS = $(APP_DIRS:%=%/main)
TTFS = $(APP_DIRS:%=%/$(TTF_FILE))

# Make sure 'all' and 'clean' are not files
.PHONY: all clean

all: $(TTFS) $(APPS)

$(TTFS): %: $(TTF_DIR)/$(TTF_FILE)
	install -m 664 $^ $@

$(APPS): %: $(CMN_OBJS) $(OBJ_DIR)/%.o
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(CMN_SRC_DIR)/$(WL_SRC):
	wayland-scanner code $(XDG_SHELL_PROTOCOL) $@
	wayland-scanner client-header $(XDG_SHELL_PROTOCOL) $(CMN_INC_DIR)/$(WL_INC)

clean:
	rm -f  $(TTFS)
	rm -f  $(APPS)
	rm -rf $(OBJ_DIR)
	rm -f  $(CMN_SRC_DIR)/$(WL_SRC)
	rm -f  $(CMN_INC_DIR)/$(WL_INC)
