CFLAGS=-Wall -std=c++17 -O0
LFLAGS=-O0
LIBS=-lstdc++fs

.PHONY: all clean
all: reMap

reMap : main.o solver.o sysinfo.o
	g++ ${LFLAGS} -o reMap main.o solver.o sysinfo.o ${LIBS}

main.o: main.cpp solver.h sysinfo.h
	g++ -c ${CFLAGS} -o main.o main.cpp

solver.o: solver.cpp solver.h
	g++ -c ${CFLAGS} -o solver.o solver.cpp

sysinfo.o: sysinfo.cpp sysinfo.h
	g++ -c ${CFLAGS} -o sysinfo.o sysinfo.cpp

clean:
	rm -f reMap *.o

