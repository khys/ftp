all: myftpc myftps

myftpc: myftpc.o func.o
	gcc -o myftpc myftpc.o func.o

myftps: myftps.o func.o
	gcc -o myftps myftps.o func.o

myftpc.o: myftpc.c myftp.h
	gcc -c myftpc.c -g

myftps.o: myftps.c myftp.h
	gcc -c myftps.c -g

func.o: func.c myftp.h
	gcc -c func.c -g

clean:
	\rm myftpc myftps *.o
