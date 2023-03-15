# Add compile flags
CFLAGS = -Wall

# Add linking flags
LDFLAGS = -lEGL -lGLESv2 -lmmngr -lmmngrbuf -l omxr_core -lpthread -lm

# Define a list of source files
SOURCE_FILES = egl.c gl.c omx.c main.c mmngr.c util.c v4l2.c queue.c

# Define a list of object files
OBJECT_FILES = $(SOURCE_FILES:.c=.o)

# Tell command 'make' that 'all' and 'clean' are not files
.PHONY: all clean

# Define the application's name
EXECUTABLE = simple_overlay

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECT_FILES)
	$(CC) $(LDFLAGS) $(OBJECT_FILES) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $< -c

clean:
	rm -f *.o $(EXECUTABLE)
