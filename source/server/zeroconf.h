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
#define TINYIOT_URL "http://127.0.0.1:3000/TinyIoT"

char *handle_url_RegistSuccess(char* url, char* CIN_URL);
//please response header TinyIoT_ID : SejongIoT
char* Zeroconf_Discovery();

//Get AE
AE* Create_AE_Zeroconf(char* data_from_app);

//Get CNT
CNT* Create_CNT_Zeroconf(char* data_from_app);

//please puts body_data from App in parameter 
//please response "Regist Success" if not -> "Regist Failed"
//please check in header!!  if TinyIoT-ID: SejongIoT -> execute  not -> Error response  
char* Get_AE_CNT_data(char* data_from_app);

































