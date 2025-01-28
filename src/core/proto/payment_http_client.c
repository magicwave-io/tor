#include "core/proto/payment_http_client.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <json-c/json.h>
#include <curl/curl.h>
#include <src/lib/malloc/malloc.h>
#include <src/lib/log/log.h>


struct curl_fetch_st {
    char *payload;
    size_t size;
};


payment_response_t* create_payment_info(char *url, create_payment_info_t* body) {
    json_object*  json_request = json_object_new_object();

    /* build post data */
    json_object_object_add(json_request, "ServiceType", json_object_new_string(body->service_type));
    json_object_object_add(json_request, "CommodityType", json_object_new_string(body->commodity_type));
    json_object_object_add(json_request, "Amount", json_object_new_int(body->amount));

    char* request_string = json_object_to_json_string(json_request);

    char log[200] = "";
    strcat(log, request_string);
    log_notice(1, log);

    char* json_response = send_http_post_request(url, json_request);



    struct payment_response_t *response = tor_malloc_zero(sizeof(payment_response_t));

//    json_object *response_body_json1 = json_object_object_get(json_response, "GwPresentTransactionXdr");
//    char *response_body1 = json_object_get_string(response_body_json1);
//
//    json_object *response_body_json2 = json_object_object_get(json_response, "GwReimbursementTransactionXdr");
//    char *response_body2 = json_object_get_string(response_body_json2);

    response->response_body = json_response;

    return response;
}

payment_response_t* process_payment(char *url, process_payment_request_t* body) {
    json_object*  json_request = json_object_new_object();


    json_object *jarray = json_object_new_array();
    for (int i = 0; i < 0; ++i) {
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "NodeId", json_object_new_string(body->routing_node[i].node_id));
        json_object_object_add(obj, "Address", json_object_new_string(body->routing_node[i].address));
        json_object_array_add(jarray,obj);
    }


    /* build post data */
    json_object_object_add(json_request, "CallbackUrl", json_object_new_string(body->call_back_url));
    json_object_object_add(json_request, "NodeId", json_object_new_string(body->node_id));
    json_object_object_add(json_request, "Route", jarray);
    json_object_object_add(json_request, "PaymentRequest", json_object_new_string(body->payment_request));

    char* request_string = json_object_to_json_string(json_request);

    char log[2000] = "";
    strcat(log, request_string);
    log_notice(1, log);


    char* json_response = send_http_post_request(url, json_request);

    payment_response_t response;
    response.response_body = json_response;

    return &response;
}

payment_response_t* process_command(char *url, utility_command_t* body) {
    json_object*  json_request = json_object_new_object();

    /* build post data */
    json_object_object_add(json_request, "CommandType", json_object_new_int(body->command_type));
    json_object_object_add(json_request, "CommandBody", json_object_new_string(body->command_body));
    json_object_object_add(json_request, "CommandId", json_object_new_string(body->command_id));
    json_object_object_add(json_request, "CallbackUrl", json_object_new_string(body->callback_url));
    json_object_object_add(json_request, "NodeId", json_object_new_string(body->node_id));
    char* json_response = send_http_post_request(url, json_request);

    struct payment_response_t *response = tor_malloc_zero(sizeof(payment_response_t));

    response->response_body = json_response;

    return response;
}

payment_response_t* process_response(char *url, utility_response_t* body) {
    json_object*  json_request = json_object_new_object();

    /* build post data */
    json_object_object_add(json_request, "ResponseBody", json_object_new_string(body->response_body));
    json_object_object_add(json_request, "NodeId", json_object_new_string(body->node_id));
    json_object_object_add(json_request, "CommandId", json_object_new_string(body->command_id));
    char* str = json_object_to_json_string(json_request);
    char* json_response = send_http_post_request(url, json_request);

    struct payment_response_t *response = tor_malloc_zero(sizeof(payment_response_t));

    response->response_body = json_response;

    return response;
}

stellar_address_response_t* get_stellar_address(char *url) {
    json_object* json_response = send_http_get_request(url);
    json_object *address_obj = json_object_object_get(json_response, "Address");
    char *address = json_object_get_string(address_obj);
    struct stellar_address_response_t *response = tor_malloc_zero(sizeof(stellar_address_response_t));

    response->address = address;

    return response;
}

/* callback for curl fetch */
size_t curl_callback (void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;                             /* calculate buffer size */
    struct curl_fetch_st *p = (struct curl_fetch_st *) userp;   /* cast pointer to fetch struct */

    /* expand buffer */
    p->payload = (char *) realloc(p->payload, p->size + realsize + 1);

    /* check buffer */
    if (p->payload == NULL) {
        /* this isn't good */
        fprintf(stderr, "ERROR: Failed to expand buffer in curl_callback");
        /* free buffer */
        free(p->payload);
        /* return */
        return -1;
    }

    /* copy contents to buffer */
    memcpy(&(p->payload[p->size]), contents, realsize);

    /* set new buffer size */
    p->size += realsize;

    /* ensure null termination */
    p->payload[p->size] = 0;

    /* return size */
    return realsize;
}

/* fetch and return url body via curl */
CURLcode curl_fetch_url(CURL *ch, const char *url, struct curl_fetch_st *fetch) {
    CURLcode rcode;                   /* curl result code */

    /* init payload */
    fetch->payload = (char *) tor_calloc_(1, sizeof(fetch->payload));

    /* check payload */
    if (fetch->payload == NULL) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to allocate payload in curl_fetch_url");
        /* return error */
        return CURLE_FAILED_INIT;
    }

    /* init size */
    fetch->size = 0;

    /* set url to fetch */
    curl_easy_setopt(ch, CURLOPT_URL, url);

    /* set calback function */
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_callback);

    /* pass fetch struct pointer */
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) fetch);

    /* set default user agent */
    curl_easy_setopt(ch, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* set timeout */
    curl_easy_setopt(ch, CURLOPT_TIMEOUT, 5);

    /* enable location redirects */
    curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);

    /* set maximum allowed redirects */
    curl_easy_setopt(ch, CURLOPT_MAXREDIRS, 1);

    /* fetch the url */
    rcode = curl_easy_perform(ch);

    /* return */
    return rcode;
}



char* send_http_post_request(const char* url_input, const json_object* json) {
    CURL *ch;                                               /* curl handle */
    CURLcode rcode;                                         /* curl result code */

   // json_object *json;                                      /* json post body */
    enum json_tokener_error jerr = json_tokener_success;    /* json parse error */

    struct curl_fetch_st curl_fetch;                        /* curl fetch struct */
    struct curl_fetch_st *cf = &curl_fetch;                 /* pointer to fetch struct */
    struct curl_slist *headers = NULL;                      /* http headers to send with request */


//    request_response_t* response;
//    response = malloc(sizeof(request_response_t));
//    response->error_code = 1;
//    response->json_response = NULL;

    /* url to test site */

    /* init curl handle */
    if ((ch = curl_easy_init()) == NULL) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to create curl handle in fetch_session");
        /* return error */
        return NULL;
    }

    /* set content type */
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");

//   json = json_object_new_object();
//
//    /* build post data */
//    json_object_object_add(json, "title", json_object_new_string(body->prm_1));
//    json_object_object_add(json, "body", json_object_new_string(body->prm_2));
//    json_object_object_add(json, "userId", json_object_new_int(133));

    /* set curl options */
    curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(ch, CURLOPT_POSTFIELDS, json_object_to_json_string(json));

    /* fetch page and capture return code */
    rcode = curl_fetch_url(ch, url_input, cf);

    /* cleanup curl handle */
    curl_easy_cleanup(ch);

    /* free headers */
    curl_slist_free_all(headers);

    /* free json object */
    json_object_put(json);

    /* check return code */
    if (rcode != CURLE_OK || cf->size < 1) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to fetch url (%s) - curl said: %s",
                url_input, curl_easy_strerror(rcode));
        /* return error */

        return NULL;
    }

    /* check payload */
    if (cf->payload != NULL) {
        /* print result */
        printf("CURL Returned: \n%s\n", cf->payload);
        /* parse return */
        json = json_tokener_parse_verbose(cf->payload, &jerr);

        return cf->payload;
    } else {
        /* error */
        fprintf(stderr, "ERROR: Failed to populate payload");
        /* free payload */
        free(cf->payload);
        /* return */
        return NULL;
    }

    /* check error */
    if (jerr != json_tokener_success) {
        /* error */
        fprintf(stderr, "ERROR: Failed to parse json string");
        /* free json object */
        json_object_put(json);
        /* return */
        return NULL;
    }

    /* debugging */
    printf("Parsed JSON: %s\n", json_object_to_json_string(json));
    /* exit */
    return NULL;
}

json_object* send_http_get_request(const char* url_input) {
    CURL *ch;                                               /* curl handle */
    CURLcode rcode;                                         /* curl result code */
    // json_object *json;                                      /* json post body */
    enum json_tokener_error jerr = json_tokener_success;    /* json parse error */

    struct curl_fetch_st curl_fetch;                        /* curl fetch struct */
    struct curl_fetch_st *cf = &curl_fetch;                 /* pointer to fetch struct */
    struct curl_slist *headers = NULL;                      /* http headers to send with request */


//    request_response_t* response;
//    response = malloc(sizeof(request_response_t));
//    response->error_code = 1;
//    response->json_response = NULL;

    /* url to test site */

    /* init curl handle */
    if ((ch = curl_easy_init()) == NULL) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to create curl handle in fetch_session");
        /* return error */
        return NULL;
    }

    /* set content type */
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");

//   json = json_object_new_object();
//
//    /* build post data */
//    json_object_object_add(json, "title", json_object_new_string(body->prm_1));
//    json_object_object_add(json, "body", json_object_new_string(body->prm_2));
//    json_object_object_add(json, "userId", json_object_new_int(133));

    /* set curl options */
    curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "GET");
    curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);

    /* fetch page and capture return code */
    rcode = curl_fetch_url(ch, url_input, cf);

    /* cleanup curl handle */
    curl_easy_cleanup(ch);

    /* free headers */
    curl_slist_free_all(headers);

    /* check return code */
    if (rcode != CURLE_OK || cf->size < 1) {
        /* log error */
        fprintf(stderr, "ERROR: Failed to fetch url (%s) - curl said: %s",
                url_input, curl_easy_strerror(rcode));
        /* return error */

        return NULL;
    }

    /* check payload */
    if (cf->payload != NULL) {
        /* print result */
        printf("CURL Returned: \n%s\n", cf->payload);
        /* parse return */
        json_object* json = json_tokener_parse_verbose(cf->payload, &jerr);
        printf("Parsed JSON: %s\n", json_object_to_json_string(json));
        return json;
    } else {
        /* error */
        fprintf(stderr, "ERROR: Failed to populate payload");
        /* free payload */
        free(cf->payload);
        /* return */
        return NULL;
    }

    /* check error */
    if (jerr != json_tokener_success) {
        /* error */
        fprintf(stderr, "ERROR: Failed to parse json string");
        /* free json object */
        /* return */
        return NULL;
    }

    /* debugging */

    /* exit */
    return NULL;
}




