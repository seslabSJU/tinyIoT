UNAME := $(shell uname)

# Check DB_TYPE from config.h to determine which database implementation to use
DB_TYPE := $(shell grep -E "^\s*\#define\s+DB_TYPE\s+" config.h | grep -E "(DB_SQLITE|DB_POSTGRESQL)" | sed 's/.*DB_//')




# macOS 
ifeq ($(UNAME), Darwin)
    ifeq ($(DB_TYPE), POSTGRESQL)
        RELCFLAGS = -g -Wall -I/opt/homebrew/opt/openssl@3/include -I$(shell brew --prefix libpq)/include
        LIBS = -lpthread -lm -ldl -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -L$(shell brew --prefix libpq)/lib -lpq
    else
        RELCFLAGS = -g -Wall -I/opt/homebrew/opt/openssl@3/include
        LIBS = -lpthread -lm -ldl -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto
    endif

# Linux
else
    ifeq ($(DB_TYPE), POSTGRESQL)
        RELCFLAGS = -g -I/usr/include/postgresql
        LIBS = -lpthread -lm -ldl -lssl -lcrypto -lpq
    else
        RELCFLAGS = -g
        LIBS = -lpthread -lm -ldl -lssl -lcrypto
    endif
endif

CC=gcc


ifeq ($(DB_TYPE), SQLITE)
    DB_IMPL = sqlite_implement.c
    DB_LIB_SRC = sqlite/sqlite3.c
else ifeq ($(DB_TYPE), POSTGRESQL)
    DB_IMPL = postgresql_implement.c
    DB_LIB_SRC = 
endif

SRCS = main.c onem2m.c cJSON.c httpd.c $(DB_IMPL) jsonparser.c mqttClient.c logger.c util.c monitor.c filterCriteria.c coap.c rtManager.c websocket/net_libwebsockets.c

RSCSRC = $(wildcard resources/*.c)

LIBCOAP = libcoap/coap_address.c libcoap/coap_asn1.c libcoap/coap_async.c libcoap/coap_block.c libcoap/coap_cache.c libcoap/coap_debug.c libcoap/coap_dtls.c libcoap/coap_encode.c libcoap/coap_event.c libcoap/coap_hashkey.c libcoap/coap_io.c libcoap/coap_layers.c libcoap/coap_mem.c libcoap/coap_net.c libcoap/coap_netif.c libcoap/coap_notls.c libcoap/coap_openssl.c libcoap/coap_option.c libcoap/coap_oscore.c libcoap/coap_pdu.c libcoap/coap_prng.c libcoap/coap_resource.c libcoap/coap_session.c libcoap/coap_str.c libcoap/coap_subscribe.c libcoap/coap_tcp.c libcoap/coap_time.c libcoap/coap_uri.c libcoap/coap_ws.c 
# not include: libcoap/coap_io_contiki.c libcoap/coap_io_lwip.c libcoap/coap_io_riot.c coap_dtls_get_overhead() libcoap/coap_gnutls.c libcoap/coap_mbedtls.c libcoap/coap_tinydtls.c
# oscore: libcoap/oscore/oscore_cbor.c libcoap/oscore/oscore_context.c libcoap/oscore/oscore_cose.c libcoap/oscore/oscore_crypto.c libcoap/oscore/oscore.c

LSRCS = wolfmqtt/mqtt_client.c wolfmqtt/mqtt_packet.c wolfmqtt/mqtt_socket.c $(DB_LIB_SRC)

OBJS = $(SRCS:.c=.o) $(RSCSRC:.c=.o) $(LSRCS:.c=.o) $(LIBCOAP:.c=.o)

#LWS추가
LDFLAGS += $(shell pkg-config --libs libwebsockets)
CFLAGS += $(shell pkg-config --cflags libwebsockets)


RELEXE = server

all: $(RELEXE)

# 빌드 타겟
$(RELEXE) : $(OBJS)
	$(CC) $(RELCFLAGS) $(CFLAGS) -o $(RELEXE) $(OBJS) $(LIBS) $(LDFLAGS)

# 오브젝트 파일 컴파일
%.o: %.c
	$(CC) $(RELCFLAGS) $(CFLAGS) -c $< -o $@

conf: semi-clean all

remake: clean all

clean:
	@rm -rf $(OBJS)
	@rm -rf $(RELEXE)

semi-clean:
	@rm -rf *.o
	@rm -rf $(RELEXE)