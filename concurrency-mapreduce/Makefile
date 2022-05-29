all: main.o mapreduce.o mapreduce.h
	gcc -o mapreduce mapreduce.o main.o -Wall -Werror -O -pthread

mapreduce.o: mapreduce.c mapreduce.h
	gcc -c -o mapreduce.o mapreduce.c -Wall -Werror -O -pthread

main.o: main.c
	gcc -c main.c -Wall -Werror -O -pthread

clean:
	rm *.o mapreduce
