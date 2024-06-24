#include <stdlib.h>
#include <string.h>
#include "onem2m.h"
#include "logger.h"
#include "dbmanager.h"
#include "util.h"
#include "config.h"


#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <ctype.h>
#include <sys/timeb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "httpd.h"
#include "onem2mTypes.h"
#include "jsonparser.h"
#include "mqttClient.h"
#include "coap.h"

char* Mobius_func(const char* addr);

int Mobius_deRegister_csr();

void Mobius_http_forwarding(oneM2MPrimitive *o2pt, char *host, int port);

RTNode *Mobius_find_rtnode(char *addr);