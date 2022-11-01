#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include "zeroconf.h"

char *handle_url_RegistSuccess(char* url, char* CIN_URL) {
    CURL *curl;

    char RegistSuccess_body[BASIC_VALUE]="SejongIoT ";
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
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, RegistSuccess_body);
    	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

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
   static char DeviceList_request_data[MAX_VALUE]={0,};
   FILE * fd;
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
         execl("/usr/local/bin/avahi-browse","avahi-browse","-at",NULL);
         break;
      
      default:
         wait(0);      
   }
   
   //here
   fd=fopen("DeviceList","r");
   fread(DeviceList_request_data,sizeof(DeviceList_request_data)-1,1,fd);
   
   return DeviceList_request_data;
}


//Get AE
AE* Create_AE_zeroconf(char* data_from_app){
   char* temp;
   AE *ae = (AE*)calloc(0, sizeof(AE));
   
   temp=strtok(data_from_app,"\n");
   temp=strtok(NULL,"\n");
   ae-> rn = temp;
   ae-> api = "tiny_project";
   ae-> rr = true;
   
   return ae;
}

//Get CNT
CNT* Create_CNT_zeroconf(char* data_from_app){
   char* temp;
   CNT *cnt = (CNT*)calloc(0, sizeof(CNT));
   
   temp=strtok(data_from_app,"\n");
   temp=strtok(NULL,"\n");
   temp=strtok(NULL,"\n");
   cnt-> rn = temp;
   
   return cnt;
}

//please puts body_data from App in parameter 
//please response "Regist Success" if not -> "Regist Failed"
//please check in header!!  if TinyIoT-ID: SejongIoT -> execute  not -> Error response  

char* Request_Device(char* data_from_app){
   FILE* fd;
   char data[BASIC_VALUE]="a";
   char ServiceType[BASIC_VALUE];
   char buffer[BASIC_VALUE];
   char callName[BASIC_VALUE]="avahi-browse "; 
   char* RegistSuccess_Response;
   char CIN_URL[BASIC_VALUE]=TinyIoT_URL;
   char* temp;

   strcat(data,data_from_app);

   temp=strtok(data,"\"");
   temp=strtok(NULL,"/");
   strcpy(ServiceType,temp);
   temp=strtok(NULL,"/");
   strcat(CIN_URL,"/");
   strcat(CIN_URL,temp);
   temp=strtok(NULL,"\"");
   strcat(CIN_URL,"/");
   strcat(CIN_URL,temp);
   
   strcat(callName,ServiceType);
   strcat(callName," --resolve");
   strcat(callName," -t");
   system(callName);
   
   fd=fopen("SaveURL","rt");
   fgets(buffer,100,fd);
   
   RegistSuccess_Response = handle_url_RegistSuccess(buffer, CIN_URL);

   fclose(fd);
}