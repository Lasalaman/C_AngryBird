CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Iinclude
LDFLAGS =

SRCS = src/level.c src/level_defaults.c src/bird_game.c src/obstacle.c src/bird.c src/bird_queue.c src/physics.c src/json_io.c
OBJS = $(SRCS:.c=.o)
SIM_SRCS = src/main.c
SIM_OBJS = $(SIM_SRCS:.c=.o)

.PHONY: all clean sim

all: libangrybird.a angrybird_sim

libangrybird.a: $(OBJS)
	ar rcs $@ $(OBJS)

angrybird_sim: $(OBJS) $(SIM_OBJS)
	$(CC) $(CFLAGS) -o $@ $(SIM_OBJS) $(OBJS) $(LDFLAGS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(SIM_OBJS) libangrybird.a angrybird_sim angrybird_sim.exe
