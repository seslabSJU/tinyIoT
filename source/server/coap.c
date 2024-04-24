/* Copyright (C) 2010--2024 Olaf Bergmann <bergmann@tzi.org> and others */

#include "config.h"

#ifdef ENABLE_COAP
#include "coap.h"
#include "onem2mTypes.h"

coapPacket *req = NULL;
bool rel5 = false;

/* oneM2M/CoAP options */
static char *coap_opt_name(int opt)
{
    switch (opt)
    {
    case COAP_OPTION_IF_MATCH:
        return "IF_MATCH";
    case COAP_OPTION_URI_HOST:
        return "URI_HOST";
    case COAP_OPTION_ETAG:
        return "ETAG";
    case COAP_OPTION_IF_NONE_MATCH:
        return "IF_NONE_MATCH";
    case COAP_OPTION_OBSERVE:
        return "OBSERVE";
    case COAP_OPTION_URI_PORT:
        return "URI_PORT";
    case COAP_OPTION_LOCATION_PATH:
        return "LOCATION_PATH";
    case COAP_OPTION_OSCORE:
        return "OSCORE";
    case COAP_OPTION_URI_PATH:
        return "URI_PATH";
    case COAP_OPTION_CONTENT_FORMAT:
        return "CONTENT_FORMAT";
    // case COAP_OPTION_CONTENT_TYPE:    return "CONTENT_TYPE";
    case COAP_OPTION_MAXAGE:
        return "MAXAGE";
    case COAP_OPTION_URI_QUERY:
        return "URI_QUERY";
    case COAP_OPTION_HOP_LIMIT:
        return "HOP_LIMIT";
    case COAP_OPTION_ACCEPT:
        return "ACCEPT";
    case COAP_OPTION_Q_BLOCK1:
        return "Q_BLOCK1";
    case COAP_OPTION_LOCATION_QUERY:
        return "LOCATION_QUERY";
    case COAP_OPTION_BLOCK2:
        return "BLOCK2";
    case COAP_OPTION_BLOCK1:
        return "BLOCK1";
    case COAP_OPTION_SIZE2:
        return "SIZE2";
    case COAP_OPTION_Q_BLOCK2:
        return "Q_BLOCK2";
    case COAP_OPTION_PROXY_URI:
        return "PROXY_URI";
    case COAP_OPTION_PROXY_SCHEME:
        return "PROXY_SCHEME";
    case COAP_OPTION_SIZE1:
        return "SIZE1";
    case COAP_OPTION_ECHO:
        return "ECHO";
    case COAP_OPTION_NORESPONSE:
        return "NORESPONSE";
    case COAP_OPTION_RTAG:
        return "RTAG";
    case oneM2M_FR:
        return "ONEM2M_FR";
    case oneM2M_RQI:
        return "ONEM2M_RQI";
    case oneM2M_OT:
        return "ONEM2M_OT";
    case oneM2M_RQET:
        return "ONEM2M_RQET";
    case oneM2M_RSET:
        return "ONEM2M_RSET";
    case oneM2M_OET:
        return "ONEM2M_OET";
    case oneM2M_RTURI:
        return "ONEM2M_RTURI";
    case oneM2M_EC:
        return "ONEM2M_EC";
    case oneM2M_RSC:
        return "ONEM2M_RSC";
    case oneM2M_GID:
        return "ONEM2M_GID";
    case oneM2M_TY:
        return "ONEM2M_TY";
    case oneM2M_CTO:
        return "ONEM2M_CTO";
    case oneM2M_CTS:
        return "ONEM2M_CTS";
    case oneM2M_ATI:
        return "ONEM2M_ATI";
    case oneM2M_RVI:
        return "ONEM2M_RVI";
    case oneM2M_VSI:
        return "ONEM2M_VSI";
    case oneM2M_GTM:
        return "ONEM2M_GTM";
    case oneM2M_AUS:
        return "ONEM2M_AUS";
    case oneM2M_ASRI:
        return "ONEM2M_ASRI";
    case oneM2M_OMR:
        return "ONEM2M_OMR";
    case oneM2M_PRPI:
        return "ONEM2M_PRPI";
    case oneM2M_MSU:
        return "ONEM2M_MSU";
    default:
        return "UNKNOWN";
    }
}

static int op_to_code(Operation op)
{
    int method;
    switch (op)
    {
    case OP_CREATE:
        method = COAP_REQUEST_POST;
        break;
    case OP_UPDATE:
        method = COAP_REQUEST_PUT;
        break;
    case OP_DELETE:
        method = COAP_REQUEST_DELETE;
        break;
    case OP_RETRIEVE:
    case OP_DISCOVERY:
        if (rel5)
            method = COAP_REQUEST_FETCH;
        else
            method = COAP_REQUEST_GET;
        break;
    }
    return method;
}

static Operation coap_parse_operation(char *method)
{
    Operation op;

    if (!strcmp(method, "POST"))
        op = OP_CREATE;
    else if (!strcmp(method, "GET") || !strcmp(method, "FETCH"))
        op = OP_RETRIEVE;
    else if (!strcmp(method, "PUT"))
        op = OP_UPDATE;
    else if (!strcmp(method, "DELETE"))
        op = OP_DELETE;

    return op;
}

static char *coap_parse_type(coap_pdu_type_t type)
{
    switch (type)
    {
    case COAP_MESSAGE_CON:
        return "CON";
    case COAP_MESSAGE_NON:
        return "NON";
    case COAP_MESSAGE_ACK:
        return "ACK";
    case COAP_MESSAGE_RST:
        return "RST";
    }
}

static char *coap_parse_req_code(coap_pdu_code_t code)
{
    switch (code)
    {
    case COAP_REQUEST_GET:
        return "GET";
    case COAP_REQUEST_POST:
        return "POST";
    case COAP_REQUEST_PUT:
        return "PUT";
    case COAP_REQUEST_DELETE:
        return "DELETE";
    case COAP_REQUEST_FETCH:
        return "FETCH";
    }
}

static void coap_respond_to_client(oneM2MPrimitive *o2pt,
                                   coap_resource_t *r,
                                   coap_session_t *session,
                                   const coap_pdu_t *req_pdu,
                                   coap_pdu_t *res_pdu)
{
    char buf[BUF_SIZE] = {'\0'};
    sprintf(buf, "\n%s %d\r\n", coap_parse_type(COAP_MESSAGE_ACK), o2pt->rsc);

    coap_pdu_set_type(res_pdu, COAP_MESSAGE_ACK);

    o2pt->coap_rsc = (o2pt->rsc < 169) ? o2pt->rsc : COAP_RESPONSE_CODE(rsc_to_coap_status(o2pt->rsc));
    coap_pdu_set_code(res_pdu, o2pt->coap_rsc);

    // Set the request options
    if (rel5)
    {
        cJSON *cjson_payload = cJSON_CreateObject();

        cJSON_AddStringToObject(cjson_payload, "rqi", o2pt->rqi);
        cJSON_AddStringToObject(cjson_payload, "rvi", o2pt->rvi);
        cJSON_AddNumberToObject(cjson_payload, "rsc", o2pt->rsc);

        if (o2pt->response_pc)
        {
            cJSON_AddItemToObject(cjson_payload, "pc", o2pt->response_pc);
        }

        o2pt->response_pc = cjson_payload;
    }
    else
    {
        coap_add_option(res_pdu, oneM2M_RQI, strlen(o2pt->rqi), (uint8_t *)o2pt->rqi);
        coap_add_option(res_pdu, oneM2M_RVI, strlen(o2pt->rvi), (uint8_t *)o2pt->rvi);

        unsigned char rsc[2];
        coap_add_option(res_pdu, oneM2M_RSC,
                        coap_encode_var_safe(rsc, sizeof(rsc), o2pt->rsc), rsc);

        sprintf(buf + strlen(buf), "\nOption #1 ONEM2M_RQI: %s\r\n", o2pt->rqi);
        sprintf(buf + strlen(buf), "Option #2 ONEM2M_RVI: %s\r\n", o2pt->rvi);
        sprintf(buf + strlen(buf), "Option #3 ONEM2M_RSC: %d\r\n", o2pt->rsc);

        // if(o2pt->cnot) {
        //     coap_add_option(res_pdu, oneM2M_CTO, 2, (uint8_t *)&o2pt->cnot);
        //     coap_add_option(res_pdu, oneM2M_CTS, 2, (uint8_t *)&o2pt->cnst);
        // }
    }

    char *normalized_pc = NULL;

    if (o2pt->response_pc)
    {
        normalized_pc = cJSON_Print(o2pt->response_pc);
        normalize_payload(normalized_pc);
        sprintf(buf + strlen(buf), "\r\n%s\r\n", normalized_pc);
    }
    else
        normalized_pc = strdup("");

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "\n%s", buf);

    // coap_add_data(res_pdu, strlen(buf), (unsigned char *)buf);
    coap_add_data_large_response(r, session, req_pdu, res_pdu,
                                 NULL, COAP_MEDIATYPE_APPLICATION_JSON, -1, 0,
                                 strlen(normalized_pc), normalized_pc, NULL, NULL);

    free(normalized_pc);
}

static void opt_to_o2pt(oneM2MPrimitive *o2pt, int opt_num, char *opt_buf)
{
    cJSON *qs = NULL;
    switch (opt_num)
    {
    case COAP_OPTION_URI_PATH:
        if (o2pt->to == NULL)
        {
            o2pt->to = (char *)malloc((strlen(opt_buf) + 1) * sizeof(char));
            strcpy(o2pt->to, opt_buf);
        }
        else
        {
            o2pt->to = (char *)realloc(o2pt->to, (strlen(o2pt->to) + strlen(opt_buf) + 2) * sizeof(char));
            strcat(o2pt->to, "/");
            strcat(o2pt->to, opt_buf);
        }
        break;
    case COAP_OPTION_URI_QUERY:
        qs = qs_to_json(opt_buf);
        parse_qs(qs);

        if (cJSON_GetObjectItem(qs, "drt"))
        {
            o2pt->drt = cJSON_GetObjectItem(qs, "drt")->valueint;
        }
        else
        {
            o2pt->drt = DRT_STRUCTURED;
        }

        if (cJSON_GetNumberValue(cJSON_GetObjectItem(qs, "fu")) == FU_DISCOVERY)
        {
            o2pt->op = OP_DISCOVERY;
            o2pt->rcn = RCN_DISCOVERY_RESULT_REFERENCES;
        }

        if (cJSON_GetObjectItem(qs, "rcn"))
        {
            o2pt->rcn = cJSON_GetObjectItem(qs, "rcn")->valueint;
            cJSON_DeleteItemFromObject(qs, "rcn");
        }
        o2pt->fc = qs;
        break;

    /* oneM2M Options */
    case oneM2M_FR:
        o2pt->fr = strdup(opt_buf);
        break;
    case oneM2M_RQI:
        o2pt->rqi = strdup(opt_buf);
        break;
    case oneM2M_RVI:
        o2pt->rvi = strdup(opt_buf);
        if (!strcmp(o2pt->rvi, "5"))
            rel5 = true;
        break;
    case oneM2M_TY:
        o2pt->ty = coap_parse_object_type(atoi(opt_buf));
        break;
    case oneM2M_RSC:
        o2pt->rsc = atoi(opt_buf);
        break;
    }
}

static void payload_opt_to_o2pt(oneM2MPrimitive *o2pt)
{
    if (cJSON_GetObjectItem(o2pt->request_pc, "fr"))
        o2pt->fr = strdup(cJSON_GetObjectItem(o2pt->request_pc, "fr")->valuestring);
    if (cJSON_GetObjectItem(o2pt->request_pc, "rqi"))
        o2pt->rqi = strdup(cJSON_GetObjectItem(o2pt->request_pc, "rqi")->valuestring);
    if (cJSON_GetObjectItem(o2pt->request_pc, "rvi"))
        o2pt->rvi = strdup(cJSON_GetObjectItem(o2pt->request_pc, "rvi")->valuestring);

    if (cJSON_GetObjectItem(o2pt->request_pc, "ty"))
        o2pt->ty = cJSON_GetObjectItem(o2pt->request_pc, "ty")->valueint;
    if (cJSON_GetObjectItem(o2pt->request_pc, "fc"))
    {
        o2pt->fc = cJSON_GetObjectItem(o2pt->request_pc, "fc");
        parse_filter_criteria(o2pt->fc);
        if (cJSON_GetNumberValue(cJSON_GetObjectItem(o2pt->fc, "fu")) == FU_DISCOVERY)
        {
            o2pt->op = OP_DISCOVERY;
            o2pt->rcn = RCN_DISCOVERY_RESULT_REFERENCES;
        }
    }
    else
    {
        o2pt->fc = cJSON_CreateObject();
        parse_filter_criteria(o2pt->fc);
    }
    if (cJSON_GetObjectItem(o2pt->request_pc, "drt"))
    {
        o2pt->drt = cJSON_GetObjectItem(o2pt->request_pc, "drt")->valueint;
    }
    else
    {
        o2pt->drt = DRT_STRUCTURED;
    }
    if (cJSON_GetObjectItem(o2pt->request_pc, "rcn"))
    {
        o2pt->rcn = cJSON_GetObjectItem(o2pt->request_pc, "rcn")->valueint;
    }

    if (cJSON_GetObjectItem(o2pt->request_pc, "pc"))
    {
        o2pt->request_pc = cJSON_GetObjectItem(o2pt->request_pc, "pc");
    }
}

static void cache_free_app_data(void *data)
{
    coap_binary_t *bdata = (coap_binary_t *)data;
    coap_delete_binary(bdata);
}

static void hnd_coap_req(coap_resource_t *r,
                        coap_session_t *session, 
                        const coap_pdu_t *req_pdu,
                        const coap_string_t *token, 
                        coap_pdu_t *res_pdu)
{
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
    req->type = coap_pdu_get_type(req_pdu);
    char *msg_type = coap_parse_type(req->type);
    req->code = coap_pdu_get_code(req_pdu);
    char *method = coap_parse_req_code(req->code);
    req->message_id = coap_pdu_get_mid(req_pdu);

    /* Token */
    coap_bin_const_t token_data = coap_pdu_get_token(req_pdu);
    req->token = token_data.s;
    req->token_len = token_data.length;

    char buf[BUF_SIZE] = {'\0'};
    sprintf(buf, "\n%s %s %d(TKL) %d(MID)\r\n\n", msg_type, method, req->token_len, req->message_id);
    if (req->token_len > 0)
    {
        sprintf(buf + strlen(buf), "Token: ");
        for (size_t i = 0; i < req->token_len; i++)
            sprintf(buf + strlen(buf), "%02X", req->token[i]);
        sprintf(buf + strlen(buf), "\r\n\n");
    }

    // oneM2M Primitive
    oneM2MPrimitive *o2pt = (oneM2MPrimitive *)calloc(1, sizeof(oneM2MPrimitive));
    o2pt->op = coap_parse_operation(method);

    // Setting default rcn
    switch (o2pt->op)
    {
    case OP_CREATE:
    case OP_RETRIEVE:
    case OP_UPDATE:
        o2pt->rcn = RCN_ATTRIBUTES;
        break;
    case OP_DELETE:
        o2pt->rcn = RCN_NOTHING;
        break;
    }

    /* Options */
    coap_opt_iterator_t opt_iter;
    coap_opt_t *option;

    req->option_cnt = 0;
    coap_option_iterator_init(req_pdu, &opt_iter, COAP_OPT_ALL);
    char *ty = NULL;
    while ((option = coap_option_next(&opt_iter)))
    {
        req->option_cnt++;
        sprintf(buf + strlen(buf), "Option #%d %s(%d) ", req->option_cnt, coap_opt_name(opt_iter.number), opt_iter.number);

        size_t opt_len = coap_opt_length(option);
        const uint8_t *opt_val = coap_opt_value(option);

        char opt_buf[255] = {'\0'};

        switch (opt_iter.number)
        {
        case COAP_OPTION_RTAG:
        case COAP_OPTION_SIZE1:
        case COAP_OPTION_SIZE2:
            for (size_t i = 0; i < opt_len; i++)
            {
                sprintf(buf + strlen(buf), "%02X", opt_val[i]);
                if (i < opt_len - 1)
                {
                    sprintf(buf + strlen(buf), ":");
                }
            }
            break;
        /* Value: Number */
        case oneM2M_TY:

            switch (*opt_val)
            {
            case 1:
            case 49:
                ty = "ACP";
                break;
            case 2:
            case 50:
                ty = "AE";
                break;
            case 3:
            case 51:
                ty = "CNT";
                break;
            case 4:
            case 52:
                ty = "CIN";
                break;
            case 5:
            case 53:
                ty = "CSE";
                break;
            case 9:
            case 57:
                ty = "GRP";
                break;
            case 16:
            case 64:
                ty = "CSR";
                break;
            case 23:
            case 71:
                ty = "SUB";
                break;
            }
            sprintf(opt_buf, "%d", *opt_val);
            sprintf(buf + strlen(buf), "%s", ty);
            break;
        case COAP_OPTION_CONTENT_FORMAT:
            if (*opt_val == COAP_MEDIATYPE_APPLICATION_JSON)
            {
                strncpy(opt_buf, "application/json", strlen("application/json"));
                sprintf(buf + strlen(buf), "%s", opt_buf);
            }
            else
            {
                logger(LOG_TAG, LOG_LEVEL_ERROR, "Unsupported Content-Format");
                coap_pdu_set_code(res_pdu, COAP_RESPONSE_CODE_UNSUPPORTED_CONTENT_FORMAT);
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

    if (coap_get_data_large(req_pdu, &size, &data, &offset, &total) && size != total)
    {
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

        if (offset == 0)
        {
            if (!cache_entry)
            {
                cache_entry = coap_new_cache_entry(session, req_pdu, COAP_CACHE_NOT_RECORD_PDU, COAP_CACHE_IS_SESSION_BASED, 0);
            }
            else
            {
                data_so_far = coap_cache_get_app_data(cache_entry);
                if (data_so_far)
                {
                    coap_delete_binary(data_so_far);
                    data_so_far = NULL;
                }
                coap_cache_set_app_data(cache_entry, NULL, NULL);
            }
        }
        if (!cache_entry)
        {
            if (offset == 0)
            {
                logger(LOG_TAG, LOG_LEVEL_ERROR, "Unable to create a new cache entry");
            }
            else
            {
                logger(LOG_TAG, LOG_LEVEL_ERROR, "No cache entry available for the non-first BLOCK");
            }
            coap_pdu_set_code(res_pdu, COAP_RESPONSE_CODE_INTERNAL_ERROR);
            return;
        }

        if (size)
        {
            /* Add in the new data to cache entry */
            data_so_far = coap_cache_get_app_data(cache_entry);
            if (!data_so_far)
            {
                data_so_far = coap_new_binary(size);
                if (data_so_far)
                    memcpy(data_so_far->s, data, size);
            }
            else
            {
                /* Add in new block to end of current data */
                coap_binary_t *new = coap_resize_binary(data_so_far, offset + size);

                if (new)
                {
                    data_so_far = new;
                    memcpy(&data_so_far->s[offset], data, size);
                }
                else
                {
                    /* Insufficient space to extend data_so_far */
                    coap_delete_binary(data_so_far);
                    data_so_far = NULL;
                }
            }
            /* Yes, data_so_far can be NULL */
            coap_cache_set_app_data(cache_entry, data_so_far, cache_free_app_data);
        }
        if (offset + size == total)
        {
            /* All the data is now in */
            data_so_far = coap_cache_get_app_data(cache_entry);
            coap_cache_set_app_data(cache_entry, NULL, NULL);
        }
        else
        {
            /* Give us the next block response */
            coap_pdu_set_code(res_pdu, COAP_RESPONSE_CODE_CONTINUE);
            return;
        }
    }
    else
    {
        /* single body of data received */
        data_so_far = coap_new_binary(size);
        if (data_so_far && size)
        {
            memcpy(data_so_far->s, data, size);
        }
    }

    if (data_so_far->length)
    {
        sprintf(buf + strlen(buf), "\nPayload:\r\n\n%s\r\n", data_so_far->s);
        o2pt->request_pc = cJSON_Parse(data_so_far->s);

        if (cJSON_GetObjectItem(o2pt->request_pc, "rvi"))
        {
            if (!strcmp(cJSON_GetObjectItem(o2pt->request_pc, "rvi")->valuestring, "5"))
                rel5 = true;
        }

        // If Release 5
        if (rel5)
        {
            payload_opt_to_o2pt(o2pt);
        }
        else if(req->code == COAP_REQUEST_FETCH)
        {
            coap_pdu_set_code(res_pdu, COAP_RESPONSE_CODE_BAD_REQUEST);
            return;
        }
    }

    if (!o2pt->request_pc)
        o2pt->request_pc = cJSON_CreateObject();

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

    /* Pre-free for free_o2pt in Release 5 of CoAP */
    if(rel5) {
        cJSON_DeleteItemFromObject(o2pt->response_pc, "fc");
        o2pt->fc = NULL;
    }

    rel5 = false;
    free_o2pt(o2pt);
}

static void hnd_unknown(coap_resource_t *resource COAP_UNUSED,
                        coap_session_t *session,
                        const coap_pdu_t *req_pdu,
                        const coap_string_t *query,
                        coap_pdu_t *res_pdu)
{
    coap_resource_t *r;
    coap_string_t *uri_path;

    /* get the uri_path - will get used by coap_resource_init() */
    uri_path = coap_get_uri_path(req_pdu);
    if (!uri_path)
    {
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

static void free_COAPRequest(coapPacket *req)
{
    if (req->token)
        free(req->token);
    free(req);
}
void coap_notify(oneM2MPrimitive *o2pt, char *noti_json, NotiTarget *nt)
{
    coap_session_t *session = NULL;
    coap_address_t bind_addr;
    coap_pdu_t *pdu = NULL;

    coap_context_t *ctx = coap_new_context(NULL);
    if (!ctx)
    {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Context (Notify)");
        return;
    }

    coap_address_init(&bind_addr);
    ((struct sockaddr_in *)&bind_addr.addr)->sin_family = AF_INET;
    ((struct sockaddr_in *)&bind_addr.addr)->sin_port = htons(nt->port);
    inet_pton(AF_INET, nt->host, &((struct sockaddr_in *)&bind_addr.addr)->sin_addr);

    unsigned int method = COAP_REQUEST_POST;

    session = coap_new_client_session(ctx, NULL, &bind_addr, COAP_PROTO_UDP);
    if (!session)
    {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Session (Notify)");
        return;
    }

    // Protocol Data Unit
    pdu = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_POST, 0, coap_session_max_pdu_size(session));
    if (!pdu)
    {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created PDU (Notify)");
        return;
    }
    coap_add_option(pdu, COAP_OPTION_URI_PATH, strlen(nt->target + 1), (uint8_t *)(nt->target + 1));
    coap_add_data(pdu, strlen(noti_json), noti_json);

    if (coap_send(session, pdu) == COAP_INVALID_TID)
    {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Failed to send (Notify)");
        return;
    }

    /* Clean up library usage */
    coap_free_context(ctx);
    coap_cleanup();
}

static coap_context_t *get_context(void)
{
    coap_context_t *ctx = coap_new_context(NULL);
    if (!ctx)
    {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Context");
        return NULL;
    }

    /* Block transfer */
    coap_context_set_block_mode(ctx, COAP_BLOCK_USE_LIBCOAP); // or COAP_BLOCK_SINGLE_BODY

    /* Define the options to ignore when setting up cache-keys */
    uint16_t cache_ignore_options[] = {COAP_OPTION_BLOCK1, COAP_OPTION_BLOCK2,
                                       /* See https://rfc-editor.org/rfc/rfc7959#section-2.10 */
                                       COAP_OPTION_MAXAGE,
                                       /* See https://rfc-editor.org/rfc/rfc7959#section-2.10 */
                                       COAP_OPTION_IF_NONE_MATCH};

    coap_cache_ignore_options(ctx, cache_ignore_options,
                              sizeof(cache_ignore_options) / sizeof(cache_ignore_options[0]));

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

    return ctx;
}

void *coap_serve()
{

    /* Initialize */
    coap_startup();
    req = (coapPacket *)malloc(sizeof(coapPacket));

    coap_context_t *ctx = get_context();

    coap_address_t serv_addr;
    coap_address_init(&serv_addr);
    serv_addr.addr.sin.sin_family = AF_INET;
    serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
    serv_addr.addr.sin.sin_port = htons(COAP_PORT);

#ifdef ENABLE_COAP_DTLS
    coap_proto_t proto = COAP_PROTO_DTLS;
    logger(LOG_TAG, LOG_LEVEL_INFO, "DTLS Enabled");
#else
    coap_proto_t proto = COAP_PROTO_UDP;
#endif

    coap_endpoint_t *ep = coap_new_endpoint(ctx, &serv_addr, proto);
    if (!ep)
    {
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

    while (1)
    {
        coap_io_process(ctx, COAP_IO_WAIT);
    }

    /* Clean up */
    coap_free_context(ctx);
    free_COAPRequest(req);

    return NULL;
}

int fwd_rsc = 0;
char *fwd_response_pc = NULL;

static coap_response_t response_handler(coap_session_t *session,
                                        const coap_pdu_t *res_pdu,
                                        const coap_pdu_t *req_pdu, 
                                        const coap_mid_t id)
{
    coapPacket *fwd_req = (coapPacket *)malloc(sizeof(coapPacket));

    fwd_req->ver = COAP_VERSION;
    fwd_req->type = coap_pdu_get_type(req_pdu);
    char *msg_type = coap_parse_type(fwd_req->type);
    fwd_req->code = coap_pdu_get_code(req_pdu);

    char buf[BUF_SIZE] = {'\0'};
    sprintf(buf, "\n%s %d\r\n\n", msg_type, fwd_req->code);

    /* Options */
    coap_opt_iterator_t opt_iter;
    coap_opt_t *option;

    fwd_req->option_cnt = 0;
    coap_option_iterator_init(req_pdu, &opt_iter, COAP_OPT_ALL);
    while ((option = coap_option_next(&opt_iter)))
    {
        fwd_req->option_cnt++;
        sprintf(buf + strlen(buf), "Option #%d %s(%d) ", fwd_req->option_cnt, coap_opt_name(opt_iter.number), opt_iter.number);

        size_t opt_len = coap_opt_length(option);
        const uint8_t *opt_val = coap_opt_value(option);

        char opt_buf[255] = {'\0'};
        char rsc[4] = {'\0'};
        switch (opt_iter.number)
        {
        /* Value: Number */
        case COAP_OPTION_CONTENT_FORMAT:
            if (*opt_val == COAP_MEDIATYPE_APPLICATION_JSON)
            {
                strncpy(opt_buf, "application/json", strlen("application/json"));
                sprintf(buf + strlen(buf), "%s", opt_buf);
            }
            else
            {
                logger(LOG_TAG, LOG_LEVEL_ERROR, "Unsupported Content-Format");
                return COAP_RESPONSE_FAIL;
            }
            break;
        case oneM2M_RSC:
            for (size_t i = 0; i < 2; i++)
                sprintf(rsc + strlen(rsc), "%02X", opt_val[i]);
            fwd_rsc = strtol(rsc, NULL, 16);
            sprintf(buf + strlen(buf), "%d", fwd_rsc);
            break;
        /* Value: String */
        default:
            strncpy(opt_buf, (char *)opt_val, opt_len);
            sprintf(buf + strlen(buf), "%s", opt_buf);
            break;
        }
        sprintf(buf + strlen(buf), "\r\n");
    }

    /* Payload */
    size_t size;
    const uint8_t *data;
    size_t offset;
    size_t total;
    coap_binary_t *data_so_far;

    if (coap_get_data_large(req_pdu, &size, &data, &offset, &total) && size != total)
    {
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

        if (offset == 0)
        {
            if (!cache_entry)
            {
                cache_entry = coap_new_cache_entry(session, req_pdu, COAP_CACHE_NOT_RECORD_PDU, COAP_CACHE_IS_SESSION_BASED, 0);
            }
            else
            {
                data_so_far = coap_cache_get_app_data(cache_entry);
                if (data_so_far)
                {
                    coap_delete_binary(data_so_far);
                    data_so_far = NULL;
                }
                coap_cache_set_app_data(cache_entry, NULL, NULL);
            }
        }
        if (!cache_entry)
        {
            if (offset == 0)
            {
                logger(LOG_TAG, LOG_LEVEL_ERROR, "Unable to create a new cache entry");
            }
            else
            {
                logger(LOG_TAG, LOG_LEVEL_ERROR, "No cache entry available for the non-first BLOCK");
            }
            return COAP_RESPONSE_FAIL;
        }

        if (size)
        {
            /* Add in the new data to cache entry */
            data_so_far = coap_cache_get_app_data(cache_entry);
            if (!data_so_far)
            {
                data_so_far = coap_new_binary(size);
                if (data_so_far)
                    memcpy(data_so_far->s, data, size);
            }
            else
            {
                /* Add in new block to end of current data */
                coap_binary_t *new = coap_resize_binary(data_so_far, offset + size);

                if (new)
                {
                    data_so_far = new;
                    memcpy(&data_so_far->s[offset], data, size);
                }
                else
                {
                    /* Insufficient space to extend data_so_far */
                    coap_delete_binary(data_so_far);
                    data_so_far = NULL;
                }
            }
            /* Yes, data_so_far can be NULL */
            coap_cache_set_app_data(cache_entry, data_so_far, cache_free_app_data);
        }
        if (offset + size == total)
        {
            /* All the data is now in */
            data_so_far = coap_cache_get_app_data(cache_entry);
            coap_cache_set_app_data(cache_entry, NULL, NULL);
        }
        else
        {
            /* Give us the next block response */
            return COAP_RESPONSE_FAIL;
        }
    }
    else
    {
        /* single body of data received */
        data_so_far = coap_new_binary(size);
        if (data_so_far && size)
        {
            memcpy(data_so_far->s, data, size);
        }
    }

    if (data_so_far->length)
    {
        sprintf(buf + strlen(buf), "\nPayload:\r\n\n%s\r\n", data_so_far->s);

        if (cJSON_GetObjectItem(cJSON_Parse(data_so_far->s), "rsc"))
        {
            fwd_rsc = cJSON_GetObjectItem(cJSON_Parse(data_so_far->s), "rsc")->valueint;
        }

        if (cJSON_GetObjectItem(cJSON_Parse(data_so_far->s), "pc"))
        {
            fwd_response_pc = cJSON_Print(cJSON_GetObjectItem(cJSON_Parse(data_so_far->s), "pc"));
        }
        else
            fwd_response_pc = strdup(data_so_far->s);
    }

    logger(LOG_TAG, LOG_LEVEL_DEBUG, "Received Forwarding Packet\r\n%s", buf);

    return COAP_RESPONSE_OK;
}

void coap_forwarding(oneM2MPrimitive *o2pt, char *host, int port)
{
    logger(LOG_TAG, LOG_LEVEL_INFO, "coap_forwarding");

    /* Initialize */
    coap_startup();

    coap_session_t *session = NULL;
    coap_address_t bind_addr;
    coap_pdu_t *pdu = NULL;

    coap_address_init(&bind_addr);
    ((struct sockaddr_in *)&bind_addr.addr)->sin_family = AF_INET;
    ((struct sockaddr_in *)&bind_addr.addr)->sin_port = htons(port);
    inet_pton(AF_INET, host, &((struct sockaddr_in *)&bind_addr.addr)->sin_addr);

    coap_context_t *ctx = get_context();

    session = coap_new_client_session(ctx, NULL, &bind_addr, COAP_PROTO_UDP);
    if (!session)
    {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created Session (Forwarding)");
        return;
    }

    unsigned int method = op_to_code(o2pt->op);

    // Protocol Data Unit
    pdu = coap_pdu_init(COAP_MESSAGE_CON, method, 0, coap_session_max_pdu_size(session));
    if (!pdu)
    {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Not Created PDU (Forwarding)");
        return;
    }

    // Set the request options
    coap_add_option(pdu, COAP_OPTION_URI_PATH, strlen(o2pt->to), (uint8_t *)o2pt->to);

    unsigned char buf[2];
    coap_add_option(pdu, COAP_OPTION_CONTENT_FORMAT,
                    coap_encode_var_safe(buf, sizeof(buf), COAP_MEDIATYPE_APPLICATION_JSON), buf);

    if (rel5)
    {
        cJSON *fwd_request_pc = cJSON_CreateObject();

        if (method != COAP_REQUEST_FETCH)
            cJSON_AddItemToObject(fwd_request_pc, "pc", cJSON_GetObjectItem(o2pt->request_pc, "pc"));

        cJSON_AddStringToObject(fwd_request_pc, "fr", o2pt->fr);
        cJSON_AddStringToObject(fwd_request_pc, "rqi", o2pt->rqi);
        cJSON_AddStringToObject(fwd_request_pc, "rvi", o2pt->rvi);

        if (o2pt->ty)
            cJSON_AddNumberToObject(fwd_request_pc, "ty", o2pt->ty);
        if (o2pt->fc)
            cJSON_AddItemToObject(fwd_request_pc, "fc", o2pt->fc);
        if (o2pt->drt)
            cJSON_AddNumberToObject(fwd_request_pc, "drt", o2pt->drt);

        o2pt->request_pc = fwd_request_pc;
    }
    else
    {
        coap_add_option(pdu, oneM2M_FR, strlen(o2pt->fr), (uint8_t *)o2pt->fr);
        coap_add_option(pdu, oneM2M_RQI, strlen(o2pt->rqi), (uint8_t *)o2pt->rqi);
        coap_add_option(pdu, oneM2M_RVI, strlen(o2pt->rvi), (uint8_t *)o2pt->rvi);

        if (o2pt->ty)
        {
            unsigned char ty[2];
            coap_add_option(pdu, oneM2M_TY, coap_encode_var_safe(ty, sizeof(ty), o2pt->ty), ty);
        }

        /* URI QUERY */
        char qs[256] = {0};
        if (o2pt->fc)
        {
            sprintf(qs, "fu=1");
        }
        if (o2pt->drt)
        {
            if (strlen(qs))
                sprintf(qs + strlen(qs), "&");
            sprintf(qs + strlen(qs), "drt=%d", o2pt->drt);
        }
        if (strlen(qs))
            coap_add_option(pdu, COAP_OPTION_URI_QUERY, strlen(qs), (uint8_t *)qs);
    }

    // Set the request payload
    if (o2pt->request_pc)
    {
        char *normalized_pc = cJSON_Print(o2pt->response_pc);
        normalize_payload(normalized_pc);

        // coap_add_data(pdu, strlen(o2pt->pc), (uint8_t *)o2pt->pc);
        coap_add_data_large_request(session, pdu, strlen(normalized_pc), normalized_pc, NULL, NULL);
    }

    coap_register_response_handler(ctx, response_handler);

    if (coap_send(session, pdu) == COAP_INVALID_TID)
    {
        logger(LOG_TAG, LOG_LEVEL_ERROR, "Failed to send (Forwarding)");
        return;
    }

    coap_io_process(ctx, COAP_IO_WAIT);

    o2pt->rsc = fwd_rsc;
    if (fwd_response_pc)
        o2pt->response_pc = cJSON_Parse(fwd_response_pc);

    /* Clean up library usage */
    coap_free_context(ctx);
    coap_cleanup();
}
#endif