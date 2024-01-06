CC=gcc
RELCFLAGS = -g

LIBS = -lpthread -lm -ldl

SRCS = main.c onem2m.c cJSON.c httpd.c dbmanager.c jsonparser.c mqttClient.c logger.c util.c filterCriteria.c 

LSRCS = wolfmqtt/mqtt_client.c wolfmqtt/mqtt_packet.c wolfmqtt/mqtt_socket.c sqlite/sqlite3.c

OBJS = $(SRCS:.c=.o) $(LSRCS:.c=.o)

RELEXE = server

all: $(RELEXE)

$(RELEXE) : $(OBJS)
	$(CC) $(RELCFLAGS) -o $(RELEXE) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(RELCFLAGS) -c $< -o $@

conf: semi-clean all

remake: clean all

clean:
	@rm -rf $(OBJS)
	@rm -rf $(RELEXE)

semi-clean:
	@rm -rf *.o
	@rm -rf $(RELEXE)