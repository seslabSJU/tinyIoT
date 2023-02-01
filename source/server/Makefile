all: server

clean:
	@rm -rf *.o
	@rm -rf server

server: main.o httpd.o onem2m.o cJSON.o berkeleyDB.o jsonparse.o mqttClient.o wolfmqtt/mqtt_client.o wolfmqtt/mqtt_packet.o wolfmqtt/mqtt_socket.o
	gcc -o server $^ -ldb -lpthread -lcurl -lm

main.o: main.c
	gcc -c -o main.o main.c 

httpd.o: httpd.c
	gcc -c -o httpd.o httpd.c

onem2m.o: onem2m.c
	gcc -c -o onem2m.o onem2m.c
	
berkeleyDB.o: berkeleyDB.c
	gcc -c -o berkeleyDB.o berkeleyDB.c
	
jsonparse.o: jsonparse.c
	gcc -c -o jsonparse.o jsonparse.c

cJSON.o: cJSON.c
	gcc -c -o cJSON.o cJSON.c

mqttClient.o: mqttClient.c
	gcc -c -o mqttClient.o mqttClient.c



