all: server

clean:
	@rm -rf *.o
	@rm -rf server

server: main.o httpd.o onem2m.o cJSON.o berkeleyDB.o jsonparse.o zeroconf.o
	gcc -o server $^ -ldb -lpthread -lcurl -lm

main.o: main.c onem2m.h
	gcc -c -o main.o main.c 

httpd.o: httpd.c onem2m.h
	gcc -c -o httpd.o httpd.c

onem2m.o: onem2m.c onem2m.h
	gcc -c -o onem2m.o onem2m.c
	
berkeleyDB.o: onem2m.h berkeleyDB.c 
	gcc -c -o berkeleyDB.o berkeleyDB.c
	
jsonparse.o: onem2m.h jsonparse.c
	gcc -c -o jsonparse.o jsonparse.c

cJSON.o: cJSON.c cJSON.h
	gcc -c -o cJSON.o cJSON.c

zeroconf.o: zeroconf.c zeroconf.h
	gcc -c -o zeroconf.o zeroconf.c

