#include "onem2m.h"
#define HTTPSERVER_IMPL
#define MAX_VALUE 10000
#define BASIC_VALUE 200
#define MIN_VALUE 30
#define TinyIoT_URL "http://192.168.234.129:3000/TinyIoT"

char *handle_url_RegistSuccess(char* url, char* CIN_URL);

//please response header TinyIoT_ID : SejongIoT
char* Zeroconf_Discovery();

//Get AE
AE* Create_AE_zeroconf(char* data_from_app);

//Get CNT
CNT* Create_CNT_zeroconf(char* data_from_app);

char* Request_Device(char* data_from_app);