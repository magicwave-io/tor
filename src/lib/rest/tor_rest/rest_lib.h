#ifndef __REST_LIB_H_INCLUDDED__
#define __REST_LIB_H_INCLUDDED__

#include <stdbool.h>
#include <stdlib.h>

typedef struct tor_http_api_request_param_st{
    const char *name;
    const char *value;
} tor_http_api_request_param_t;

typedef struct tor_http_api_request_st {
    const char *method;
    const char *url;
    const char *body;
    size_t param_count;
    tor_http_api_request_param_t *params;
    char *answer_body;
    int answer_plain_text;
    void(*release)(void *);
} tor_http_api_request_t;

typedef enum {
    TOR_HTTP_RESULT_OK                  = 0,
    TOR_HTTP_RESULT_WRONG_METHOD        = -1,
    TOR_HTTP_RESULT_WRONG_URL           = -2,
    TOR_HTTP_RESULT_WRONG_BODY          = -3,
    TOR_HTTP_RESULT_WRONG_JSON          = -4,
    TOR_HTTP_RESULT_WRONG_PARAMETER     = -5,
    TOR_HTTP_RESULT_UNKNOWN             = -6
} tor_http_result_code_t;

#ifdef __cplusplus
extern "C" {
#endif

int runServer(
        int port,
        void (*log_function)(const char *message),
        int (*handler)(tor_http_api_request_t *request) /* = nullptr*/
    );
int stopServer(void);

#ifdef __cplusplus
}
#endif

#endif
