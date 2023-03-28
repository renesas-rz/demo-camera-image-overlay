# Add compile flags
CFLAGS = -Wall -Wextra -Werror

# Add linking flags
LDFLAGS = -lEGL -lGLESv2 -lmmngr -lmmngrbuf -l omxr_core -lpthread -lm

# Define directories
OBJ_DIR = ./objs
CMN_INC_DIR = ./common/inc
CMN_SRC_DIR = ./common/src

APP_DIRS = ./h264-to-file     \
           ./raw-video-to-lcd

# Get common source files
CFLAGS += -I $(CMN_INC_DIR)
CMN_SRCS = $(shell find $(CMN_SRC_DIR) -name '*.c')

# Get common object files
CMN_OBJS = $(CMN_SRCS:%.c=$(OBJ_DIR)/%.o)

# Get sample apps
APPS = $(APP_DIRS:%=%/main)

# Make sure 'all' and 'clean' are not files
.PHONY: all clean

all: $(APPS)

$(APPS): %: $(OBJ_DIR)/%.o $(CMN_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(APPS)
	rm -rf $(OBJ_DIR)
