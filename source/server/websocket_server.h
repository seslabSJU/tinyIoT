#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include "cJSON.h"
#include "onem2m.h"

void initialize_websocket_server();
int process_onem2m_request(cJSON *json, oneM2MPrimitive *o2pt); // 함수 선언 추가

#endif // WEBSOCKET_SERVER_H
