# Add compile flags
CFLAGS = -Wall -Wextra -Werror

# Add linking flags
LDFLAGS = -lEGL            \
          -lGLESv2         \
          -lmmngr          \
          -lmmngrbuf       \
          -lomxr_core      \
          -lpthread        \
          -lm              \
          -lwayland-client \
          -lwayland-egl

# Define directories
OBJ_DIR = ./objs
CMN_SRC_DIR = ./common/src
CMN_INC_DIR = ./common/inc

CFLAGS += -I$(CMN_INC_DIR)

# Define variables for Wayland
WL_PROTOCOLS_DIR   = $(shell pkg-config wayland-protocols --variable=pkgdatadir)
XDG_SHELL_PROTOCOL = $(WL_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml

WL_SRC = xdg-shell-protocol.c
WL_INC = xdg-shell-client-protocol.h

# Get common source files
CMN_SRCS = $(CMN_SRC_DIR)/$(WL_SRC) \
           $(shell find $(CMN_SRC_DIR) -name '*.c')

# Get common object files
CMN_OBJS = $(CMN_SRCS:%.c=$(OBJ_DIR)/%.o)

# Define sample apps
APP_DIRS = ./h264-to-file     \
           ./raw-video-to-lcd

APPS = $(APP_DIRS:%=%/main)

# Make sure 'all' and 'clean' are not files
.PHONY: all clean

all: $(APPS)

$(APPS): %: $(OBJ_DIR)/%.o $(CMN_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(CMN_SRC_DIR)/$(WL_SRC):
	wayland-scanner code $(XDG_SHELL_PROTOCOL) $@
	wayland-scanner client-header $(XDG_SHELL_PROTOCOL) $(CMN_INC_DIR)/$(WL_INC)

clean:
	rm -f  $(APPS)
	rm -rf $(OBJ_DIR)
	rm -f  $(CMN_SRC_DIR)/$(WL_SRC)
	rm -f  $(CMN_INC_DIR)/$(WL_INC)
