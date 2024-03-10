/* Copyright (C) 2010--2024 Olaf Bergmann <bergmann@tzi.org> and others */

#include "config.h"

#ifdef ENABLE_COAP
#include "coap.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LOG_TAG "COAP"
#define BUF_SIZE 65535

coapPacket* req = NULL;
int rel5 = 0;

/* oneM2M/CoAP options */
char *coap_opt_name(int opt) {
    switch (opt) {
        case COAP_OPTION_IF_MATCH:        return "IF_MATCH";
        case COAP_OPTION_URI_HOST:        return "URI_HOST";
        case COAP_OPTION_ETAG:            return "ETAG";
        case COAP_OPTION_IF_NONE_MATCH:   return "IF_NONE_MATCH";
        case COAP_OPTION_OBSERVE:         return "OBSERVE";
        case COAP_OPTION_URI_PORT:        return "URI_PORT";
        case COAP_OPTION_LOCATION_PATH:   return "LOCATION_PATH";
        case COAP_OPTION_OSCORE:          return "OSCORE";
        case COAP_OPTION_URI_PATH:        return "URI_PATH";
        case COAP_OPTION_CONTENT_FORMAT:  return "CONTENT_FORMAT";
        // case COAP_OPTION_CONTENT_TYPE:    return "CONTENT_TYPE";
        case COAP_OPTION_MAXAGE:          return "MAXAGE";
        case COAP_OPTION_URI_QUERY:       return "URI_QUERY";
        case COAP_OPTION_HOP_LIMIT:       return "HOP_LIMIT";
        case COAP_OPTION_ACCEPT:          return "ACCEPT";
        case COAP_OPTION_Q_BLOCK1:        return "Q_BLOCK1";
        case COAP_OPTION_LOCATION_QUERY:  return "LOCATION_QUERY";
        case COAP_OPTION_BLOCK2:          return "BLOCK2";
        case COAP_OPTION_BLOCK1:          return "BLOCK1";
        case COAP_OPTION_SIZE2:           return "SIZE2";
        case COAP_OPTION_Q_BLOCK2:        return "Q_BLOCK2";
        case COAP_OPTION_PROXY_URI:       return "PROXY_URI";
        case COAP_OPTION_PROXY_SCHEME:    return "PROXY_SCHEME";
        case COAP_OPTION_SIZE1:           return "SIZE1";
        case COAP_OPTION_ECHO:            return "ECHO";
        case COAP_OPTION_NORESPONSE:      return "NORESPONSE";
        case COAP_OPTION_RTAG:            return "RTAG";
        case oneM2M_FR:                   return "ONEM2M_FR";
        case oneM2M_RQI:                  return "ONEM2M_RQI";
        case oneM2M_OT:                   return "ONEM2M_OT";
        case oneM2M_RQET:                 return "ONEM2M_RQET";
        case oneM2M_RSET:                 return "ONEM2M_RSET";
        case oneM2M_OET:                  return "ONEM2M_OET";
        case oneM2M_RTURI:                return "ONEM2M_RTURI";
        case oneM2M_EC:                   return "ONEM2M_EC";
        case oneM2M_RSC:                  return "ONEM2M_RSC";
        case oneM2M_GID:                  return "ONEM2M_GID";
        case oneM2M_TY:                   return "ONEM2M_TY";
        case oneM2M_CTO:                  return "ONEM2M_CTO";
        case oneM2M_CTS:                  return "ONEM2M_CTS";
        case oneM2M_ATI:                  return "ONEM2M_ATI";
        case oneM2M_RVI:                  return "ONEM2M_RVI";
        case oneM2M_VSI:                  return "ONEM2M_VSI";
        case oneM2M_GTM:                  return "ONEM2M_GTM";
        case oneM2M_AUS:                  return "ONEM2M_AUS";
        case oneM2M_ASRI:                 return "ONEM2M_ASRI";
        case oneM2M_OMR:                  return "ONEM2M_OMR";
        case oneM2M_PRPI:                 return "ONEM2M_PRPI";
        case oneM2M_MSU:                  return "ONEM2M_MSU";
        default:                          return "UNKNOWN";
    }
}

int op_to_code(Operation op){
    int method;
    switch (op){
        case OP_CREATE: method = COAP_REQUEST_POST; break;
        case OP_UPDATE: method = COAP_REQUEST_PUT;  break;
        case OP_DELETE: method = COAP_REQUEST_DELETE; break;
        case OP_RETRIEVE:
        case OP_DISCOVERY:
            method = COAP_REQUEST_GET;
            break;
    }
    return method;
}

Operation coap_parse_operation(char *method){
	Operation op;

	if(!strcmp(method, "POST"))                                 op = OP_CREATE;
	else if(!strcmp(method, "GET") || !strcmp(method, "FETCH")) op = OP_RETRIEVE;
	else if (!strcmp(method, "PUT"))                            op = OP_UPDATE;
	else if (!strcmp(method, "DELETE"))                         op = OP_DELETE;

	return op;
}

char *coap_parse_type(coap_pdu_type_t type){
    switch(type) {
        case COAP_MESSAGE_CON: return "CON";
        case COAP_MESSAGE_NON: return "NON";
        case COAP_MESSAGE_ACK: return "ACK";
        case COAP_MESSAGE_RST: return "RST";
    }
}

char *coap_parse_req_code(coap_pdu_code_t code){
    switch(code) {
        case COAP_REQUEST_GET:      return "GET";
        case COAP_REQUEST_POST:     return "POST";
        case COAP_REQUEST_PUT:      return "PUT";
        case COAP_REQUEST_DELETE:   return "DELETE";
        case COAP_REQUEST_FETCH:    return "FETCH";
    }
}

void coap_respond_to_client(oneM2MPrimitive *o2pt, coap_resource_t *r, coap_session_t *session, 
                            const coap_pdu_t *req_pdu, coap_pdu_t *res_pdu) {
    int status_code = rsc_to_coap_status(o2pt->rsc);
    char *status;

    switch(status_code) {
        case 201: status = "2.01 Created";                      break;
        case 202: status = "2.02 Deleted";                      break;
        case 204: status = "2.04 Changed";                      break;
        case 205: status = "2.05 Content";                      break;
        case 400: status = "4.00 Bad_Request";                  break;
        case 402: status = "4.02 Bad_Option";                   break;
        case 403: status = "4.03 Forbidden";                    break;
        case 404: status = "4.04 Not_Found";                    break;
        case 405: status = "4.05 Method_Not_Allowed";           break;
        case 406: status = "4.06 Not_Acceptable";               break;
        case 408: status = "4.08 Request_Entity_Incomplete";    break;
        case 409: status = "4.09 Conflict";                     break;
        case 415: status = "4.15 Unsupported_Content-Format";   break;
        case 500: status = "5.00 Internal_Server_Error";        break;
        case 501: status = "5.01 Not_Implemented";              break;
        case 503: status = "5.03 Service_Unavailable";          break;
        case 504: status = "5.04 Gateway_Timeout";              break;
    }

    char buf[BUF_SIZE] = {'\0'};
    sprintf(buf, "\n%s %s %d\r\n", coap_parse_type(COAP_MESSAGE_ACK), status, o2pt->rsc);

    sprintf(buf + strlen(buf), "\r\n");
    sprintf(buf + strlen(buf), "RQI: %s\r\n", o2pt->rqi);
    sprintf(buf + strlen(buf), "RVI: %s\r\n", o2pt->rvi);

    if(o2pt->pc){
        sprintf(buf + strlen(buf), "\r\n");
        normalize_payload(o2pt->pc);
        strcat(buf, o2pt->pc);
        sprintf(buf + strlen(buf), "\r\n");
    }

    if(o2pt->cnot){
        sprintf(buf + strlen(buf), "CNOT: %d\r\n", o2pt->cnot);
        sprintf(buf + strlen(buf), "CNST: %d\r\n", o2pt->cnst);
    }
    
    coap_pdu_set_type(res_pdu, COAP_MESSAGE_ACK);
    coap_pdu_set_code(res_pdu, COAP_RESPONSE_CODE(status_code));

    // coap_add_data(res_pdu, strlen(buf), (unsigned char *)buf);
    coap_add_data_large_response(r, session, req_pdu, res_pdu, 
                                o2pt->fc, COAP_MEDIATYPE_TEXT_PLAIN, -1, 0,
                                strlen(buf), (unsigned char *)buf, NULL, NULL);

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "\n%s", buf);
}

void opt_to_o2pt(oneM2MPrimitive *o2pt, int opt_num, char *opt_buf) {
    switch(opt_num) {
        case COAP_OPTION_URI_PATH: 
            if(o2pt->to == NULL) {
                o2pt->to = (char *)malloc((strlen(opt_buf) + 1) * sizeof(char));
                strcpy(o2pt->to, opt_buf);
            } else {
                o2pt->to = (char *)realloc(o2pt->to, (strlen(o2pt->to) + strlen(opt_buf) + 2) * sizeof(char));
                strcat(o2pt->to, "/");
                strcat(o2pt->to, opt_buf);
            }
            break;
        case COAP_OPTION_URI_QUERY: 
            cJSON *qs = qs_to_json(opt_buf);
            parse_qs(qs);

            if(cJSON_GetObjectItem(qs, "drt")){
                o2pt->drt = cJSON_GetObjectItem(qs, "drt")->valueint;
            } else {
                o2pt->drt = DRT_STRUCTURED;
            }

            if(cJSON_GetNumberValue(cJSON_GetObjectItem(qs, "fu")) == FU_DISCOVERY){
                o2pt->op = OP_DISCOVERY;
            }
            o2pt->fc = qs;
            break;

        /* oneM2M Options */
        case oneM2M_FR: 
            o2pt->fr = strdup(opt_buf); break;
        case oneM2M_RQI:
            o2pt->rqi = strdup(opt_buf); break;
        case oneM2M_RVI:
            o2pt->rvi = strdup(opt_buf); 
            if(!strcmp(o2pt->rvi, "5")) rel5 = 1;
            break;
        case oneM2M_TY:
            o2pt->ty = coap_parse_object_type(atoi(opt_buf)); break;    
    }
}

void payload_opt_to_o2pt(oneM2MPrimitive *o2pt, cJSON *cjson_payload) {
    o2pt->fr = strdup(cJSON_GetObjectItem(cjson_payload, "fr")->valuestring);
    o2pt->rqi = strdup(cJSON_GetObjectItem(cjson_payload, "rqi")->valuestring);
    o2pt->rvi = strdup(cJSON_GetObjectItem(cjson_payload, "rvi")->valuestring);

    if(cJSON_GetObjectItem(cjson_payload, "ty"))
        o2pt->ty = coap_parse_object_type(cJSON_GetObjectItem(cjson_payload, "ty")->valueint);
    if(cJSON_GetObjectItem(cjson_payload, "fc")) {
        o2pt->fc = cJSON_GetObjectItem(cjson_payload, "fc");
        parse_filter_criteria(o2pt->fc);
        if(cJSON_GetNumberValue(cJSON_GetObjectItem(o2pt->fc, "fu")) == FU_DISCOVERY){
            o2pt->op = OP_DISCOVERY;
        }
    }
    if(cJSON_GetObjectItem(cjson_payload, "drt")) {
        o2pt->drt = cJSON_GetObjectItem(cjson_payload, "drt")->valueint;
    } else {
        o2pt->drt = DRT_STRUCTURED;
    }

    if(cJSON_GetObjectItem(cjson_payload, "pc")) {
        o2pt->pc = cJSON_Print(cJSON_GetObjectItem(cjson_payload, "pc"));
        o2pt->cjson_pc = cJSON_GetObjectItem(cjson_payload, "pc");
    }
}

static void cache_free_app_data(void *data) {
    coap_binary_t *bdata = (coap_binary_t *)data;
    coap_delete_binary(bdata);
}

void hnd_coap_req(coap_resource_t *r, coap_session_t *session, const coap_pdu_t *req_pdu, 
             const coap_string_t *token, coap_pdu_t *res_pdu) {
    /* CoAP packet format
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |Ver| T |  TKL  |      Code     |          Message ID           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   Token (if any, TKL bytes) ...
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         Options (if any) ...
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |1 1 1 1 1 1 1 1|               Payload (if any) ...
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */    

    /* Parse CoAP packet */
    /* Version, Type, Token Length, Code, Message ID */
    req->ver = COAP_VERSION;
    req->type = coap_pdu_get_type(req_pdu); char *msg_type = coap_parse_type(req->type);
    req->code = coap_pdu_get_code(req_pdu); char *method = coap_parse_req_code(req->code);
    req->message_id = coap_pdu_get_mid(req_pdu);

    /* Token */
    coap_bin_const_t token_data = coap_pdu_get_token(req_pdu);
    req->token = token_data.s;
    req->token_len = token_data.length;

    char buf[BUF_SIZE] = {'\0'};
    sprintf(buf, "\n%s %s %d(TKL) %d(MID)\r\n\n", msg_type, method, req->token_len, req->message_id);
    if(req->token_len > 0) {
        sprintf(buf + strlen(buf), "Token: ");
        for (size_t i = 0; i < req->token_len; i++) 
            sprintf(buf + strlen(buf), "%02X", req->token[i]);
        sprintf(buf + strlen(buf), "\r\n\n");
    }

    // oneM2M Primitive
    oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive)); 
    o2pt->op = coap_parse_operation(method);  

    /* Options */
    coap_opt_iterator_t opt_iter;
    coap_opt_t *option;

    req->option_cnt = 0;
    coap_option_iterator_init(req_pdu, &opt_iter, COAP_OPT_ALL);
    while ((option = coap_option_next(&opt_iter))) {
        req->option_cnt++;
        sprintf(buf + strlen(buf), "Option #%d %s(%d) ", req->option_cnt, coap_opt_name(opt_iter.number), opt_iter.number);
        
        size_t opt_len = coap_opt_length(option);
        const uint8_t *opt_val = coap_opt_value(option);

        char opt_buf[255] = {'\0'};

        switch(opt_iter.number) {
            case COAP_OPTION_RTAG:
            case COAP_OPTION_SIZE1:
            case COAP_OPTION_SIZE2:
                for (size_t i = 0; i < opt_len; i++) {
                    sprintf(buf + strlen(buf), "%02X", opt_val[i]);
                    if (i < opt_len - 1) {
                        sprintf(buf + strlen(buf), ":");
                    }
                }
                break;
            /* Value: Number */
            case oneM2M_TY:
                char *ty;
                switch(*opt_val) {
                    case 49 : ty = "ACP"; break;
                    case 50 : ty = "AE"; break;
                    case 51 : ty = "CNT"; break;
                    case 52 : ty = "CIN"; break;
                    case 53 : ty = "CSE"; break;
                    case 57 : ty = "GRP"; break;
                    case 64 : ty = "CSR"; break;
                    case 71 : ty = "SUB"; break;
                }
                sprintf(opt_buf, "%d", *opt_val);
                sprintf(buf + strlen(buf), "%s", ty);
                break;
            case COAP_OPTION_CONTENT_FORMAT:
                if(*opt_val == COAP_MEDIATYPE_APPLICATION_JSON) {
                    strncpy(opt_buf, "application/json", strlen("application/json"));
                    sprintf(buf + strlen(buf), "%s", opt_buf);
                } else {
                    logger(LOG_TAG, LOG_LEVEL_ERROR, "Unsupported Content-Format");
                    o2pt->rsc = RSC_UNSUPPORTED_MEDIATYPE;
                    coap_respond_to_client(o2pt, r, session, req_pdu, res_pdu);
                    return;
                }
                break;
            /* Value: String */
            default:
                strncpy(opt_buf, (char *)opt_val, opt_len);
                sprintf(buf + strlen(buf), "%s", opt_buf);
                break;
        }
        opt_to_o2pt(o2pt, opt_iter.number, opt_buf);
        sprintf(buf + strlen(buf), "\r\n");
    }

    /* Payload */
    size_t size;
    const uint8_t *data;
    size_t offset;
    size_t total;
    coap_binary_t *data_so_far;

    if (coap_get_data_large(req_pdu, &size, &data, &offset, &total) && size != total) {
        /*
        * A part of the data has been received (COAP_BLOCK_SINGLE_BODY not set).
        * However, total unfortunately is only an indication, so it is not safe to allocate a block based on total. As per https://rfc-editor.org/rfc/rfc7959#section-4
        *   o  In a request carrying a Block1 Option, to indicate the current
        *         estimate the client has of the total size of the resource representation, measured in bytes ("size indication").
        *
        * coap_cache_ignore_options() must have previously been called with at
        * least COAP_OPTION_BLOCK1 set as the option value will change per block.
        */
        coap_cache_entry_t *cache_entry = coap_cache_get_by_pdu(session, req_pdu, COAP_CACHE_IS_SESSION_BASED);

        if (offset == 0) {
            if (!cache_entry) {
                cache_entry = coap_new_cache_entry(session, req_pdu, COAP_CACHE_NOT_RECORD_PDU, COAP_CACHE_IS_SESSION_BASED, 0);
            } else {
                data_so_far = coap_cache_get_app_data(cache_entry);
                if (data_so_far) {
                    coap_delete_binary(data_so_far);
                    data_so_far = NULL;
                }
                coap_cache_set_app_data(cache_entry, NULL, NULL);
            }
        }
        if (!cache_entry) {
            if (offset == 0) {
                logger(LOG_TAG, LOG_LEVEL_ERROR, "Unable to create a new cache entry");
            } else {
                logger(LOG_TAG, LOG_LEVEL_ERROR, "No cache entry available for the non-first BLOCK");
            }
            coap_pdu_set_code(res_pdu, COAP_RESPONSE_CODE_INTERNAL_ERROR);
            return;
        }

        if (size) {
            /* Add in the new data to cache entry */
            data_so_far = coap_cache_get_app_data(cache_entry);
            if (!data_so_far) {
                data_so_far = coap_new_binary(size);
                if (data_so_far)
                    memcpy(data_so_far->s, data, size);
            } else {
                /* Add in new block to end of current data */
                coap_binary_t *new = coap_resize_binary(data_so_far, offset + size);

                if (new) {
                    data_so_far = new;
                    memcpy(&data_so_far->s[offset], data, size);
                } else {
                    /* Insufficient space to extend data_so_far */
                    coap_delete_binary(data_so_far);
                    data_so_far = NULL;
                }
            }
            /* Yes, data_so_far can be NULL */
            coap_cache_set_app_data(cache_entry, data_so_far, cache_free_app_data);
        }
        if (offset + size == total) {
            /* All the data is now in */
            data_so_far = coap_cache_get_app_data(cache_entry);
            coap_cache_set_app_data(cache_entry, NULL, NULL);
        } else {
            /* Give us the next block response */
            coap_pdu_set_code(res_pdu, COAP_RESPONSE_CODE_CONTINUE);
            return;
        }
    } else { 
        /* single body of data received */
        data_so_far = coap_new_binary(size);
        if (data_so_far && size) {
            memcpy(data_so_far->s, data, size);
        }
    }

    if(data_so_far->length) {
        sprintf(buf + strlen(buf), "\nPayload:\r\n\n%s\r\n", data_so_far->s);

        char *payload = strdup(data_so_far->s);
        cJSON *cjson_payload = cJSON_Parse(payload);

        if(cJSON_GetObjectItem(cjson_payload, "rvi")) {
            if(!strcmp(cJSON_GetObjectItem(cjson_payload, "rvi")->valuestring, "5")) rel5 = 1;
        }

        // If Release 5
        if(rel5 == 1) { 
            payload_opt_to_o2pt(o2pt, cjson_payload);
        } else {
            if(req->code == COAP_REQUEST_FETCH) {
                o2pt->rsc = RSC_BAD_REQUEST;
                coap_respond_to_client(o2pt, r, session, req_pdu, res_pdu);
                return;
            } else {
                o2pt->pc = strdup(data_so_far->s);
                o2pt->cjson_pc = cJSON_Parse(o2pt->pc);
            }
        }
    }
    if(!o2pt->pc) o2pt->pc = strdup("");
    
    logger(LOG_TAG, LOG_LEVEL_DEBUG, "\n%s", buf);

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "fr: %s", o2pt->fr);
    logger(LOG_TAG, LOG_LEVEL_DEBUG, "to: %s", o2pt->to);
    logger(LOG_TAG, LOG_LEVEL_INFO, "Request : %s", method);

    /* get client ip address */
    const coap_address_t *remote_addr = coap_session_get_addr_remote(session);
    o2pt->ip = (char *)malloc((INET_ADDRSTRLEN + 1) * sizeof(char));
    inet_ntop(AF_INET, &remote_addr->addr.sin.sin_addr, o2pt->ip, INET_ADDRSTRLEN);
    logger(LOG_TAG, LOG_LEVEL_INFO, "Client connected %s\n", o2pt->ip);

    o2pt->errFlag = false;

	route(o2pt);

    coap_respond_to_client(o2pt, r, session, req_pdu, res_pdu);

    free_o2pt(o2pt);
}

void hnd_unknown(coap_resource_t *resource COAP_UNUSED,
                    coap_session_t *session,
                    const coap_pdu_t *req_pdu,
                    const coap_string_t *query,
                    coap_pdu_t *res_pdu) {
    coap_resource_t *r;
    coap_string_t *uri_path;

    /* get the uri_path - will get used by coap_resource_init() */
    uri_path = coap_get_uri_path(req_pdu);
    if (!uri_path) {
        coap_pdu_set_code(res_pdu, COAP_RESPONSE_CODE_NOT_FOUND);
        return;
    }   

    /*
    * Create a resource to handle the new URI
    * uri_path will get deleted when the resource is removed
    */
    r = coap_resource_init((coap_str_const_t *)uri_path,
                            COAP_RESOURCE_FLAGS_RELEASE_URI | COAP_RESOURCE_FLAGS_NOTIFY_CON);
    coap_register_request_handler(r, COAP_REQUEST_GET, hnd_coap_req);
    coap_register_request_handler(r, COAP_REQUEST_POST, hnd_coap_req);
    coap_register_request_handler(r, COAP_REQUEST_PUT, hnd_coap_req);
    coap_register_request_handler(r, COAP_REQUEST_DELETE, hnd_coap_req);
    coap_register_request_handler(r, COAP_REQUEST_FETCH, hnd_coap_req);
    coap_add_resource(coap_session_get_context(session), r);

    /* Do for this first call */
    hnd_coap_req(r, session, req_pdu, query, res_pdu);
}

void free_COAPRequest(coapPacket *req){
    if(req->token) free(req->token);
    if(req->payload) free(req->payload);
    free(req);   
}
void coap_notify(oneM2MPrimitive *o2pt, char *noti_json, NotiTarget *nt) {
    coap_session_t *session = NULL;
    coap_address_t bind_addr;
    coap_pdu_t *pdu = NULL;

    coap_context_t *ctx = coap_new_context(NULL);
    if (!ctx) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Context (Notify)");
        return; 
    }

    coap_address_init(&bind_addr);
    ((struct sockaddr_in *)&bind_addr.addr)->sin_family = AF_INET;
    ((struct sockaddr_in *)&bind_addr.addr)->sin_port = htons(nt->port);
    inet_pton(AF_INET, nt->host, &((struct sockaddr_in *)&bind_addr.addr)->sin_addr);

    unsigned int method = COAP_REQUEST_POST;

    session = coap_new_client_session(ctx, NULL, &bind_addr, COAP_PROTO_UDP);
    if (!session) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Session (Notify)");
        return;
    }

    // Protocol Data Unit
    pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_POST, 0, coap_session_max_pdu_size(session));
    if (!pdu) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created PDU (Notify)");
        return;
    }
    coap_add_option(pdu, COAP_OPTION_URI_PATH, strlen(nt->target + 1), (uint8_t *)(nt->target + 1));
    coap_add_data(pdu, strlen(noti_json), noti_json);

    if (coap_send(session, pdu) == COAP_INVALID_TID) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Failed to send (Notify)");
        return;
    }

    /* Clean up library usage */
    coap_free_context(ctx);
    coap_cleanup();
}

void* coap_serve(void) {
    uint16_t cache_ignore_options[] = { COAP_OPTION_BLOCK1, COAP_OPTION_BLOCK2,
                                        /* See https://rfc-editor.org/rfc/rfc7959#section-2.10 */
                                        COAP_OPTION_MAXAGE,
                                        /* See https://rfc-editor.org/rfc/rfc7959#section-2.10 */
                                        COAP_OPTION_IF_NONE_MATCH
                                    };
    /* Initialize */
    coap_startup();
    req = (coapPacket*)malloc(sizeof(coapPacket));

    coap_address_t serv_addr;
    coap_address_init(&serv_addr);
    serv_addr.addr.sin.sin_family = AF_INET;
    serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
    serv_addr.addr.sin.sin_port = htons(COAP_PORT);
    
    coap_context_t *ctx = coap_new_context(NULL);
    if (!ctx) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Context");
        return NULL; 
    }

    /* Define the options to ignore when setting up cache-keys */
    coap_cache_ignore_options(ctx, cache_ignore_options,
                                sizeof(cache_ignore_options)/sizeof(cache_ignore_options[0]));

    /* Register oneM2M options */
    coap_register_option(ctx, oneM2M_FR);
    coap_register_option(ctx, oneM2M_RQI);
    coap_register_option(ctx, oneM2M_OT);
    coap_register_option(ctx, oneM2M_RQET);
    coap_register_option(ctx, oneM2M_RSET);
    coap_register_option(ctx, oneM2M_OET);
    coap_register_option(ctx, oneM2M_RTURI);
    coap_register_option(ctx, oneM2M_EC);
    coap_register_option(ctx, oneM2M_RSC);
    coap_register_option(ctx, oneM2M_GID);
    coap_register_option(ctx, oneM2M_TY);
    coap_register_option(ctx, oneM2M_CTO);
    coap_register_option(ctx, oneM2M_CTS);
    coap_register_option(ctx, oneM2M_ATI);
    coap_register_option(ctx, oneM2M_RVI);
    coap_register_option(ctx, oneM2M_VSI);
    coap_register_option(ctx, oneM2M_GTM);
    coap_register_option(ctx, oneM2M_AUS);
    coap_register_option(ctx, oneM2M_ASRI);
    coap_register_option(ctx, oneM2M_OMR);
    coap_register_option(ctx, oneM2M_PRPI);
    coap_register_option(ctx, oneM2M_MSU);

    coap_endpoint_t *ep = coap_new_endpoint(ctx, &serv_addr, COAP_PROTO_UDP);
    if (!ep) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Endpoint");
        return NULL;
    }

    logger(LOG_TAG, LOG_LEVEL_INFO, "CoAP Setup Completed");

    /* Register resource handler */
    coap_resource_t *r = coap_resource_unknown_init2(hnd_unknown, 0);
    coap_register_handler(r, COAP_REQUEST_GET, hnd_unknown);
    coap_register_handler(r, COAP_REQUEST_POST, hnd_unknown);
    coap_register_handler(r, COAP_REQUEST_PUT, hnd_unknown);
    coap_register_handler(r, COAP_REQUEST_DELETE, hnd_unknown);
    coap_register_handler(r, COAP_REQUEST_FETCH, hnd_unknown);
    coap_add_resource(ctx, r);

    /* Block transfer */
    coap_context_set_block_mode(ctx, COAP_BLOCK_USE_LIBCOAP); // or COAP_BLOCK_SINGLE_BODY

    while (1) {
        coap_io_process(ctx, COAP_IO_WAIT);
    }

    /* Clean up */
    coap_free_endpoint(ep);
    coap_free_context(ctx);
    free_COAPRequest(req);
}

int rsc;

coap_response_t response_handler(coap_session_t *session, const coap_pdu_t *res_pdu, const coap_pdu_t *req_pdu, 
                                const coap_mid_t id) {
    size_t len;
    const uint8_t *databuf = NULL;

    if (coap_get_data(req_pdu, &len, &databuf)) {
        char *type = strdup(strtok(databuf, " "));
        char *res_code = strdup(strtok(NULL, " "));
        char *res_description = strdup(strtok(NULL, " "));
        rsc = atoi(strdup(strtok(NULL, " ")));  
        return COAP_RESPONSE_OK;
    }

    return COAP_RESPONSE_FAIL;
}

void coap_forwarding(oneM2MPrimitive *o2pt, char *host, int port) {
    logger(LOG_TAG, LOG_LEVEL_INFO, "coap_forwarding");

    /* Initialize */
    coap_startup();

    coap_session_t *session = NULL;
    coap_address_t bind_addr;
    coap_pdu_t *pdu = NULL;

    coap_context_t *ctx = coap_new_context(NULL);
    if (!ctx) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Context (Forwarding)");
        return; 
    }

    coap_address_init(&bind_addr);
    ((struct sockaddr_in *)&bind_addr.addr)->sin_family = AF_INET;
    ((struct sockaddr_in *)&bind_addr.addr)->sin_port = htons(port);
    inet_pton(AF_INET, host, &((struct sockaddr_in *)&bind_addr.addr)->sin_addr);

    unsigned int method = op_to_code(o2pt->op);

    session = coap_new_client_session(ctx, NULL, &bind_addr, COAP_PROTO_UDP);
    if (!session) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Session (Forwarding)");
        return;
    }

    // Protocol Data Unit
    pdu = coap_pdu_init(COAP_MESSAGE_CON, method, 0, coap_session_max_pdu_size(session));
    if (!pdu) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created PDU (Forwarding)");
        return;
    }

    // Set the request options
    coap_add_option(pdu, COAP_OPTION_URI_PATH, strlen(o2pt->to), (uint8_t *)o2pt->to);
    coap_add_option(pdu, oneM2M_FR, strlen(o2pt->fr), (uint8_t *)o2pt->fr);
    coap_add_option(pdu, oneM2M_RQI, strlen(o2pt->rqi), (uint8_t *)o2pt->rqi);
    coap_add_option(pdu, oneM2M_RVI, strlen("2a"), "2a");

    uint8_t buf[2];
    size_t len = coap_encode_var_safe(buf, sizeof(buf), 50); // application/json
    coap_add_option(pdu, COAP_OPTION_CONTENT_TYPE, len, buf);

    if(o2pt->ty > 0) {
        size_t len = coap_encode_var_safe(buf, sizeof(buf), o2pt->ty);
        coap_add_option(pdu, oneM2M_TY, len, buf);
    }

    // Set the request payload
    if(o2pt->pc) coap_add_data(pdu, strlen(o2pt->pc), (uint8_t *)o2pt->pc);

    coap_register_response_handler(ctx, response_handler);

    if (coap_send(session, pdu) == COAP_INVALID_TID) {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Failed to send (Forwarding)");
        return;
    }

    coap_io_process(ctx, COAP_IO_WAIT);    

    o2pt->rsc = rsc;

    /* Clean up library usage */
    coap_free_context(ctx);
    coap_cleanup();;
}
#endif