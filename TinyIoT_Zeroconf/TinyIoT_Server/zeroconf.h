#define HTTPSERVER_IMPL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>


#define MAX_VALUE 10000
#define BASIC_VALUE 200
#define MIN_VALUE 30
#define TINYIOT_URL "http://192.168.35.232:3000/TinyIoT"
#define TINYIOT_ID "SejongTinyIoT"
#define AVAHI_BROWSE_PATH "/usr/local/bin/avahi-browse"
#define AUTHENTICATION_KEY_HEADER "Authentication_key: SejongTinyIoT"

char *handle_url_RegistSuccess(char* url, char* CIN_URL);
//please response header TinyIoT_ID : SejongIoT
char* Zeroconf_Discovery();

//please puts body_data from App in parameter 
//please response "Regist Success" if not -> "Regist Failed"
//please check in header!!  if TinyIoT-ID: SejongIoT -> execute  not -> Error response  
void Request_Device(char* type, char* ae_rn, char* cnt_rn);

































