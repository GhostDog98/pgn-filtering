CFLAGS = -g3 -Ofast -fno-trapping-math -fno-signed-zeros -ffinite-math-only -funsafe-math-optimizations -fno-math-errno -ffast-math -fwhole-program -funsafe-loop-optimizations -Wunsafe-loop-optimizations -march=native -funroll-loops -Wall -Wextra
CC = gcc
TARGET = pgn
#CFLAGS = -g3 -Wall -Wextra
LDFLAGS = -lm
SRCS = main.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)


