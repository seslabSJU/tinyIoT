#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include "onem2m.h"
#include "zeroconf.h"

char *handle_url_RegistSuccess(char* url, char* CIN_URL) {
    CURL *curl;

    char RegistSuccess_body[BASIC_VALUE]="";
    strcat(RegistSuccess_body,CIN_URL);
    
    struct url_data data;

    data.size = 0;
    data.data = (char*)malloc(4096); 

    if(NULL == data.data) {
        fprintf(stderr, "Failed to allocate memory.\n");

        return NULL;
    }

    data.data[0] = '\0';
    CURLcode res;
    
    curl = curl_easy_init();

    if (curl) {

         struct curl_slist *headers = NULL;

         headers = curl_slist_append(headers, AUTHENTICATION_KEY_HEADER);

         curl_easy_setopt(curl, CURLOPT_URL, url);
         curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
         curl_easy_setopt(curl, CURLOPT_POSTFIELDS, RegistSuccess_body);
    	   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		   curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
         curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
			fprintf(stderr, "curl_easy_perform() failed: %s\n",curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
     }
	 
	 return data.data;
}

//please response header TinyIoT_ID : SejongIoT
char* Zeroconf_Discovery(){
   static char DeviceList_request_data[MAX_VALUE]="";
   FILE * fd;
   long lsize;

   fd=fopen("DeviceList","wt");
   fputs("\nDeviceList\n\n\n",fd);
   fclose(fd);
   
   //here
   pid_t pid;
   pid=fork();
   switch(pid){
      case -1:
         printf("fork failed");
         break;
         
      case 0:
         execl(AVAHI_BROWSE_PATH,"avahi-browse","-at",NULL);
         break;
      
      default:
         wait(0);      
   }
   
   //here
   fd=fopen("DeviceList","r");
   fseek(fd,0,SEEK_END);
   lsize=ftell(fd);
   rewind(fd);
   fread(DeviceList_request_data,1,lsize,fd);
   DeviceList_request_data[lsize]='\0';
   fclose(fd);

   return DeviceList_request_data;
}

//please puts body_data from App in parameter 
//please response "Regist Success" if not -> "Regist Failed"
//please check in header!!  if TinyIoT-ID: SejongIoT -> execute  not -> Error response  

void Request_Device(char* type, char* ae_rn, char* cnt_rn){
   FILE* fd;
   char ServiceType[BASIC_VALUE];
   char buffer[BASIC_VALUE];
   char callName[BASIC_VALUE]="avahi-browse "; 
   char* RegistSuccess_Response;
   char CIN_URL[BASIC_VALUE]=TINYIOT_URL;
   char* temp;

   strcat(CIN_URL,"/");
   strcat(CIN_URL,ae_rn);
   strcat(CIN_URL,"/");
   strcat(CIN_URL,cnt_rn);
   
   strcat(callName,type);
   strcat(callName," --resolve");
   strcat(callName," -t");
   system(callName);
   
   fd=fopen("SaveURL","rt");
   fgets(buffer,100,fd);

   RegistSuccess_Response = handle_url_RegistSuccess(buffer, CIN_URL);

   fclose(fd);
}