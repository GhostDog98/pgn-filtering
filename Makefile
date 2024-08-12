CC = g++
CFLAGS = -g -fno-trapping-math -fno-signed-zeros -ffinite-math-only -funsafe-math-optimizations -fno-math-errno -ffast-math -fwhole-program -funsafe-loop-optimizations -Wunsafe-loop-optimizations -march=native -Ofast -funroll-loops -Wall -Wextra -std=c99
TARGET = pgn
LDFLAGS = -lm
SRCS = main.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	# strip $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)


