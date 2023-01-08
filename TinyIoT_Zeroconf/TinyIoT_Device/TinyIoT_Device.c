#define HTTPSERVER_IMPL
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Httpserver.h"
#include <curl/curl.h>
#include <time.h>
#include <pthread.h>

#define AUTHENTICATION_HEADER_NAME "Authentication_key"
#define AUTHENTICATION_KEY "SejongTinyIoT"
#define SUCCESS "Authentication Successful."
#define ERROR "Authentication Failed."
#define AVAHI_DAEMON_CONF_FILE_PATH "/etc/avahi/avahi-daemon.conf"
#define MAX_VALUE 10000
#define VALUE 300
#define BODY_VALUE 1000

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

char *handle_url_Create_CIN(char* url, char* value) {

    CURL *curl;

    char CIN_body[VALUE]="{ \"m2m:cin\":{ \"con\":\"";
    strcat(CIN_body,value);
    strcat(CIN_body,"\"   } }");

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
        
        headers = curl_slist_append(headers, "Content-Type: application/json; ty=4");
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "X-M2M-Origin: TAEC");
        headers = curl_slist_append(headers, "X-M2M-RI: 12345");
        
    	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, CIN_body);

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

char* get_authentication_key(http_string_t request){
	
	char* temp;
	static char authentication_key[MAX_VALUE];

	temp=strstr(request.buf,AUTHENTICATION_HEADER_NAME);
	if(authentication_key==NULL){
        return "Wrong";
	}

    temp=strtok(temp,"\n");
    temp[strlen(temp)-1]='\0';
	temp+=sizeof(char)*(strlen(AUTHENTICATION_HEADER_NAME)+2);
    strcpy(authentication_key, temp);

    return authentication_key;
	

}

void handle_request(struct http_request_s* request) {

    char CIN_URL[VALUE];
    char mode_no_broadcast[13]="\nuse-ipv4=no ";
  	char line[VALUE], *p;
    int find_pos;
  	char *Not_Need;
    FILE* fd=fopen(AVAHI_DAEMON_CONF_FILE_PATH,"r+t");
  
    //RegistSuccess Response
    struct http_response_s* RegistSuccess_response = http_response_init();
    http_response_status(RegistSuccess_response, 200);
    http_response_header(RegistSuccess_response, "Content-Type", "text/plain");
    http_response_body(RegistSuccess_response, SUCCESS, sizeof(SUCCESS) - 1);  

    //Error Response
    struct http_response_s* Error_response = http_response_init();
    http_response_status(Error_response, 404);
    http_response_header(Error_response, "Content-Type", "text/plain");
    http_response_body(Error_response, ERROR, sizeof(ERROR) - 1);  
  
  
    strcpy(CIN_URL,http_request_body(request).buf);
    
    char* authentication_key=get_authentication_key(http_request_method(request));

    //Authenticated Key
    if(!strcmp(authentication_key,AUTHENTICATION_KEY)){
        fprintf(stderr,"Authentication Successful.\n");

        //Respond
  	    http_respond(request, RegistSuccess_response);
  	
  	    //broadcast stop code
  	    if(fd==NULL){
  		    printf("Failed File Open");
  	    }
  	    else{
  		    while(fgets(line,VALUE,fd)!=NULL){
                p=strstr(line,"use-ipv4=yes");
                if(p!=NULL){
                    find_pos=strlen(line) - (p-line) + 1;
                    fseek(fd, (-1)*find_pos, SEEK_CUR);
                    fwrite(mode_no_broadcast,sizeof(mode_no_broadcast),1,fd);
                    fseek(fd,find_pos - sizeof(mode_no_broadcast), SEEK_CUR);
                }
            }
        fclose(fd);
        }
        system("service avahi-daemon restart");
    	
  	

        //create CIN URL
        
        while(1){
            int SubTem=rand();
            char temperature[10];
            double Tem;
            SubTem%=100;
            SubTem+=230;
            Tem=(double)SubTem/10;
            sprintf(temperature,"%.1f",Tem);
            Not_Need=handle_url_Create_CIN(CIN_URL,temperature);
            fprintf(stderr,"Temperature Transfer: %s°C\n",temperature);
            sleep(1);		
        }
            
        fclose(fd);  	

    }
    else{
        fprintf(stderr,"Authentication Failed\n");
        http_respond(request, Error_response);
    }
		


}

int main(){
	
	struct http_server_s* server = http_server_init(3000, handle_request);
	http_server_listen(server);
	return 0;
	
}
