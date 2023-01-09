#define HTTPSERVER_IMPL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "httpserver.h"
#include <curl/curl.h>

#define SERVER_URL "http://192.168.35.232:3000/TinyIoT"
#define TINYIOT_ID_HEADER_NAME "TinyIoT-ID"
#define MAX_VALUE 3000
#define VALUE 100
#define MIDDLE_VALUE 300


struct url_data {

    size_t size;

    char* data;

};

size_t write_data(void *ptr, size_t size, size_t nmemb, struct url_data *data) {

    size_t index = data->size;

    size_t n = (size * nmemb);

    char* tmp;



    data->size += (size * nmemb);



#ifdef DEBUG

    fprintf(stderr, "data at %p size=%ld nmemb=%ld\n", ptr, size, nmemb);

#endif

    tmp = realloc(data->data, data->size + 1); /* +1 for '\0' */



    if(tmp) {

        data->data = tmp;

    } else {

        if(data->data) {

            free(data->data);

        }

        fprintf(stderr, "Failed to allocate memory.\n");

        return 0;

    }



    memcpy((data->data + index), ptr, n);

    data->data[data->size] = '\0';

    return size * nmemb;

}

char* handle_url_Request_Zeroconf(char* url) {

    CURL *curl;
    
    struct url_data data;
    
    data.size = 0;

    data.data = malloc(4096); /* reasonable size initial buffer */

    if(NULL == data.data) {

        fprintf(stderr, "Failed to allocate memory.\n");


    }

    data.data[0] = '\0';



    CURLcode res;
    
    curl = curl_easy_init();

    if (curl) {
	struct curl_slist *headers = NULL;
        curl_easy_setopt(curl, CURLOPT_URL, url);
        
        headers = curl_slist_append(headers, "X-fc:Zeroconf");
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "X-M2M-Origin: TAEC");
        headers = curl_slist_append(headers, "X-M2M-RI: 12345");
        
    	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        
        curl_easy_setopt(curl, CURLOPT_WRITEHEADER, &data);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        
        
        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {

                fprintf(stderr, "curl_easy_perform() failed: %s\n",

                        curl_easy_strerror(res));

        }

        curl_easy_cleanup(curl);

     }
	
    return data.data;
}

char* make_json(char* select_type, char* AE_rn, char* CNT_rn){
	
	static char select_data[MIDDLE_VALUE]="{\n\t\"m2m:ae\": {\n\t\t\"rn\" : \"";
    	strcat(select_data, AE_rn);
    	strcat(select_data, "\",\n\t\t\"api\": \"");
    	strcat(select_data, select_type);
    	strcat(select_data, "\",\n\t\t\"rr\" : true\n\t},\n\t\"m2m:cnt\": {\n\t\t\"rn\" : \"");
    	strcat(select_data, CNT_rn);
    	strcat(select_data, "\"\n\t}\n}");
    	
    	return select_data;
	
}

char *handle_url_Create_AE_CNT(char* url, char* select_type, char* AE_rn, char* CNT_rn, char* Cipher) {

    CURL *curl;
    
    char TinyIoT_ID[VALUE]="X-TinyIoT_ID:";
    
    strcat(TinyIoT_ID,Cipher);

    char* select_data = make_json(select_type,AE_rn,CNT_rn);
    
    
    struct url_data data;

    data.size = 0;

    data.data = malloc(4096); /* reasonable size initial buffer */

    if(NULL == data.data) {

        fprintf(stderr, "Failed to allocate memory.\n");

        return NULL;

    }

    data.data[0] = '\0';

    CURLcode res;
    
    curl = curl_easy_init();

    if (curl) {
	
	struct curl_slist *headers = NULL;
        curl_easy_setopt(curl, CURLOPT_URL, url);
        
        headers = curl_slist_append(headers, "X-fc:Create");
        headers = curl_slist_append(headers, "Content-Type: application/json; ty= 5");
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "X-M2M-Origin: TAEC");
        headers = curl_slist_append(headers, "X-M2M-RI: 12345");
        headers = curl_slist_append(headers, TinyIoT_ID);
        
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, select_data);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {

                fprintf(stderr, "curl_easy_perform() failed: %s\n",

                        curl_easy_strerror(res));

        }

        curl_easy_cleanup(curl);

     }

    return data.data;
}


int main() {
	char *Response_set;
	char TinyIoT_ID[VALUE]="";
	char *sub_TinyIoT_ID;
	char *End_TinyIoT_ID;
	char DeviceList[MAX_VALUE];
	char *sub_DeviceList;
	char *Response_Create;
	char selectType[VALUE];
	char AE_rn[VALUE];
	char CNT_rn[VALUE];
	char select_data[MIDDLE_VALUE]="\"";
	
	//Request Zeroconf 
	Response_set=handle_url_Request_Zeroconf(SERVER_URL);
	
	//Create DeviceList
	sub_DeviceList=strstr(Response_set,"DeviceList");
	strcpy(DeviceList,sub_DeviceList);
	
	//Get TinyIoT-ID
	sub_TinyIoT_ID=strstr(Response_set,TINYIOT_ID_HEADER_NAME)+sizeof(TINYIOT_ID_HEADER_NAME);
	End_TinyIoT_ID=strstr(sub_TinyIoT_ID,"\n");
	*(End_TinyIoT_ID + 1)='\0';
	strcat(TinyIoT_ID,sub_TinyIoT_ID);
	
	//Print DeviceList
	printf("\n\n%s",DeviceList);
	
	printf("Please input Service Type\n\n");
	scanf("%s",selectType);
	printf("\nPlease input Device Name(for AE resource name)\n\n");
	scanf("%s",AE_rn);
	printf("\nPlease input Option Name(for CNT resource name)\n\n");
	scanf("%s",CNT_rn);
	
	Response_Create=handle_url_Create_AE_CNT(SERVER_URL, selectType, AE_rn, CNT_rn, TinyIoT_ID);
	
	//Print Response Message
  	printf("\n\nResponse: %s\n\n",Response_Create);
  	
	return 0;
}


