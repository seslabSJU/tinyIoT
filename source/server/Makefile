UNAME := $(shell uname)

# Check DB_TYPE from config.h to determine which database implementation to use
DB_TYPE := $(shell grep -E "^\s*\#define\s+DB_TYPE\s+" config.h | grep -E "(DB_SQLITE|DB_POSTGRESQL)" | sed 's/.*DB_//')

# macOS 
ifeq ($(UNAME), Darwin)
    ifeq ($(DB_TYPE), POSTGRESQL)
        RELCFLAGS = -g -I/opt/homebrew/opt/openssl@3/include -I$(shell brew --prefix libpq)/include
        LIBS = -lpthread -lm -ldl -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto -L$(shell brew --prefix libpq)/lib -lpq
    else
        RELCFLAGS = -g -I/opt/homebrew/opt/openssl@3/include
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

SRCS = main.c onem2m.c cJSON.c httpd.c $(DB_IMPL) jsonparser.c mqttClient.c logger.c util.c filterCriteria.c coap.c rtManager.c sdt.c

RSCSRC = $(wildcard resources/*.c)

LIBCOAP = libcoap/coap_address.c libcoap/coap_asn1.c libcoap/coap_async.c libcoap/coap_block.c libcoap/coap_cache.c libcoap/coap_debug.c libcoap/coap_dtls.c libcoap/coap_encode.c libcoap/coap_event.c libcoap/coap_hashkey.c libcoap/coap_io.c libcoap/coap_layers.c libcoap/coap_mem.c libcoap/coap_net.c libcoap/coap_netif.c libcoap/coap_notls.c libcoap/coap_openssl.c libcoap/coap_option.c libcoap/coap_oscore.c libcoap/coap_pdu.c libcoap/coap_prng.c libcoap/coap_resource.c libcoap/coap_session.c libcoap/coap_str.c libcoap/coap_subscribe.c libcoap/coap_tcp.c libcoap/coap_time.c libcoap/coap_uri.c libcoap/coap_ws.c 
# not include: libcoap/coap_io_contiki.c libcoap/coap_io_lwip.c libcoap/coap_io_riot.c coap_dtls_get_overhead() libcoap/coap_gnutls.c libcoap/coap_mbedtls.c libcoap/coap_tinydtls.c
# oscore: libcoap/oscore/oscore_cbor.c libcoap/oscore/oscore_context.c libcoap/oscore/oscore_cose.c libcoap/oscore/oscore_crypto.c libcoap/oscore/oscore.c

LSRCS = wolfmqtt/mqtt_client.c wolfmqtt/mqtt_packet.c wolfmqtt/mqtt_socket.c $(DB_LIB_SRC)

OBJS = $(SRCS:.c=.o) $(RSCSRC:.c=.o) $(LSRCS:.c=.o) $(LIBCOAP:.c=.o)

RELEXE = server

# SDT XML to FCP conversion using official oneM2M SDTTool
SDT_SOURCES = $(wildcard sdt_sources/*.xml)
SDT_OUTPUTS = $(patsubst sdt_sources/%.xml,sdt_definitions/%.fcp,$(SDT_SOURCES))
SDT_TOOL_DIR = tools
SDT_TOOL = $(SDT_TOOL_DIR)/SDTTool.py
SDT_PYTHON = $(HOME)/project/sdt-tool/venv/bin/python3
SDT_SHORTNAMES = tools/shortnames.csv
SDT_MODEL_VERSION = 5.6.0

all: sdt $(RELEXE)

$(RELEXE) : $(OBJS)
	$(CC) $(RELCFLAGS) -o $(RELEXE) $(OBJS) $(LIBS)

%.o: %.c
	$(CC) $(RELCFLAGS) -c $< -o $@

# SDT conversion target (PHONY to avoid conflicts)
.PHONY: sdt sdt-rebuild sdt-clean

sdt: $(SDT_OUTPUTS)
	@if [ -n "$(SDT_OUTPUTS)" ]; then \
		echo "SDT conversion complete."; \
	else \
		echo "No SDT source files found in sdt_sources/"; \
	fi

sdt_definitions/%.fcp: sdt_sources/%.xml $(SDT_TOOL) $(SDT_SHORTNAMES)
	@echo "Converting $< to $@..."
	@namespace=$$(echo "$*" | sed -E 's/.*-(Home|Common|Health|City|Agriculture|Industry|Management|Vehicular|Railway|PublicSafety|Metadata).*/\1/' | tr '[:upper:]' '[:lower:]' | sed 's/home/hod/;s/common/cod/;s/health/hed/;s/city/cid/;s/agriculture/agd/;s/industry/ind/;s/management/mad/;s/vehicular/ved/;s/railway/rad/;s/publicsafety/psd/;s/metadata/mdd/'); \
	PYTHONPATH=$(SDT_TOOL_DIR):$$PYTHONPATH $(SDT_PYTHON) $(SDT_TOOL) -i $< -o $@ -of apjson -ns $$namespace --shortnamesinfile $(SDT_SHORTNAMES) --modelversion $(SDT_MODEL_VERSION) 2>&1 | grep -v "WARNING: No such file" || true

# Force regeneration of all SDT files
sdt-rebuild:
	@echo "Rebuilding all SDT definitions..."
	@rm -f $(SDT_OUTPUTS)
	@$(MAKE) sdt

conf: semi-clean all

remake: clean all

clean:
	@rm -rf $(OBJS)
	@rm -rf $(RELEXE)

# Clean only SDT-generated FCP files
sdt-clean:
	@echo "Cleaning SDT-generated FCP files..."
	@rm -f $(SDT_OUTPUTS)

# Full clean including SDT files
full-clean: clean sdt-clean

semi-clean:
	@rm -rf *.o
	@rm -rf $(RELEXE)