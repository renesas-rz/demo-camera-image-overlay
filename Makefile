all:
	${CC} -o simple_overlay simple_overlay.c -lEGL -lGLESv2 -lmmngr -lmmngrbuf -l omxr_core -lpthread -lm
