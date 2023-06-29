CC=gcc
RELCFLAGS = -g

LIBS = -lpthread -lm -ldb

SRCS = main.c onem2m.c cJSON.c httpd.c dbmanager.c jsonparser.c mqttClient.c logger.c util.c filterCriteria.c wolfmqtt/mqtt_client.c wolfmqtt/mqtt_packet.c wolfmqtt/mqtt_socket.c sqlite/sqlite3.c

OBJS = $(SRCS:.c=.o)

RELEXE = server

all: $(RELEXE)

$(RELEXE) : $(OBJS)
	$(CC) $(RELCFLAGS) -o $(RELEXE) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(RELCFLAGS) -c $< -o $@


remake: clean all

clean:
	@rm -rf *.o
	@rm -rf $(RELEXE)
