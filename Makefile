all:
	gcc -g3 -Wall -o ndn *.c 

# clean house
clean:
	rm -f ndn
