OBJS	= main.o
SOURCE	= main.c
HEADER	= ola.h
OUT	= ndn
FLAGS	 = -g3 -c -Wall
LFLAGS	 = 

all: ndn

ndn: $(OBJS)
	$(CC) $(FLAGS) -o $@ $^ $(LFLAGS)

%.o: %.c $(HEADER)
	$(CC) $(FLAGS) -c -o $@ $< $(LFLAGS)

# clean house
clean:
	rm -f $(OBJS) $(OUT)

# compile program with debugging information
debug: $(OUT)
	valgrind $(OUT)

# run program with valgrind for errors
valgrind: $(OUT)
	valgrind $(OUT)

# run program with valgrind for leak checks
valgrind_leakcheck: $(OUT)
	valgrind --leak-check=full $(OUT)

# run program with valgrind for leak checks (extreme)
valgrind_extreme: $(OUT)
	valgrind --leak-check=full --show-leak-kinds=all --leak-resolution=high --track-origins=yes --vgdb=yes $(OUT)