CC = gcc
FLAGS = -g -Iincludes
LINKS = -lhttp -lhash-table -lcnet -lmagic -lssl -lcrypto -lm -luuid

server: bin/server.o
	$(CC) $(FLAGS) -o $@ $^ $(LINKS)

bin/%.o: src/%.c
	$(CC) $(FLAGS) -o $@ -c $^
