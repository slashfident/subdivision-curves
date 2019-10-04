default:
	cc -g -std=c99 -c -I /opt/raylib/src geom.c -o geom.o
	cc -o geom.elf geom.o -s -Wall -std=c99 -I/opt/raylib/src -L/opt/raylib/release/libs/linux -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
	rm *.o
