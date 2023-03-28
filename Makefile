# Add compile flags
CFLAGS = -Wall -Wextra -Werror

# Add linking flags
LDFLAGS = -lEGL -lGLESv2 -lmmngr -lmmngrbuf -l omxr_core -lpthread -lm

# Define directories
OBJ_DIR = ./objs
CMN_INC_DIR = ./common/inc
CMN_SRC_DIR = ./common/src

H264_TO_FILE_DIR = ./h264-to-file

# Get common source files
CFLAGS += -I $(CMN_INC_DIR)
CMN_SRCS = $(shell find $(CMN_SRC_DIR) -name '*.c')

# Get common object files
CMN_OBJS = $(CMN_SRCS:%.c=$(OBJ_DIR)/%.o)

# Get sample app: H.264 to file
H264_TO_FILE_SRC = $(H264_TO_FILE_DIR)/main.c
H264_TO_FILE_OBJ = $(H264_TO_FILE_SRC:%.c=$(OBJ_DIR)/%.o)
H264_TO_FILE_APP = $(patsubst %.c,%,$(H264_TO_FILE_SRC))

# Make sure 'all' and 'clean' are not files
.PHONY: all clean

all: $(H264_TO_FILE_APP)

$(H264_TO_FILE_APP): $(H264_TO_FILE_OBJ) $(CMN_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(H264_TO_FILE_APP)
