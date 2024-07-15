CC = gcc
RELCFLAGS = -g

LIBS = -lpthread -lm -ldl -lssl -lcrypto -lwebsockets -ljansson
INCLUDES = -I/path/to/your/includes -I/usr/include

SRCS = main.c onem2m.c cJSON.c httpd.c dbmanager.c jsonparser.c mqttClient.c logger.c util.c filterCriteria.c coap.c rtManager.c websocket_server.c resource_manager.c

RSCSRC = $(wildcard resources/*.c)

LIBCOAP = libcoap/coap_address.c libcoap/coap_asn1.c libcoap/coap_async.c libcoap/coap_block.c libcoap/coap_cache.c libcoap/coap_debug.c libcoap/coap_dtls.c libcoap/coap_encode.c libcoap/coap_event.c libcoap/coap_hashkey.c libcoap/coap_io.c libcoap/coap_layers.c libcoap/coap_mem.c libcoap/coap_net.c libcoap/coap_netif.c libcoap/coap_notls.c libcoap/coap_openssl.c libcoap/coap_option.c libcoap/coap_oscore.c libcoap/coap_pdu.c libcoap/coap_prng.c libcoap/coap_resource.c libcoap/coap_session.c libcoap/coap_str.c libcoap/coap_subscribe.c libcoap/coap_tcp.c libcoap/coap_time.c libcoap/coap_uri.c libcoap/coap_ws.c

LSRCS = wolfmqtt/mqtt_client.c wolfmqtt/mqtt_packet.c wolfmqtt/mqtt_socket.c sqlite/sqlite3.c

OBJS = $(SRCS:.c=.o) $(RSCSRC:.c=.o) $(LSRCS:.c=.o) $(LIBCOAP:.c=.o)

RELEXE = server

all: $(RELEXE)

$(RELEXE): $(OBJS)
	$(CC) $(RELCFLAGS) -o $(RELEXE) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(RELCFLAGS) $(INCLUDES) -c $< -o $@

conf: semi-clean all

remake: clean all

clean:
	@rm -rf $(OBJS)
	@rm -rf $(RELEXE)

semi-clean:
	@rm -rf *.o
	@rm -rf $(RELEXE)
