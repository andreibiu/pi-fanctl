build:
	gcc fanctl.c -o fanctl -lwiringPi -O3 -w

clean:
	rm fanctl fanctl.o