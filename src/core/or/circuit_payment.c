/* Copyright (c) 2017 The Tor Plus Project, Inc. */
/* See LICENSE for licensing information */


//file circuitpayment.c
//brief Circuit-level payment implementation

 
#include "core/or/or.h"
#include "core/or/circuitpadding.h"
#include "core/or/circuit_payment.h"
#include "core/or/circuitlist.h"
#include "core/or/relay.h"
#include "core/or/channel.h"
#include "lib/crypt_ops/crypto_rand.h"
#include "lib/crypt_ops/crypto_format.h"
#include "core/or/crypt_path_st.h"
#include "core/or/circuit_st.h"
#include "core/or/origin_circuit_st.h"
#include "feature/nodelist/routerstatus_st.h"
#include "feature/nodelist/routerset.h"
#include "feature/nodelist/node_st.h"
#include "core/or/cell_st.h"
#include "core/or/extend_info_st.h"
#include "feature/nodelist/nodelist.h"
#include "app/config/config.h"
#include "trunnel-impl.h"
#include "core/mainloop/mainloop.h"
#include <src/core/or/or_circuit_st.h>
#include "lib/version/torversion.h"
#include <pthread.h>
#include "lib/evloop/compat_libevent.h"
#include "core/mainloop/cpuworker.h"
#include "lib/evloop/workqueue.h"
#include "feature/client/circpathbias.h"
#include <ctype.h>

#if defined(__COVERITY__) || defined(__clang_analyzer__)
/* If we're running a static analysis tool, we don't want it to complain
 * that some of our remaining-bytes checks are dead-code. */
const static int sendmecell_deadcode_dummy__ = 0;
#define OR_DEADCODE_DUMMY || sendmecell_deadcode_dummy__
#else
#define OR_DEADCODE_DUMMY
#endif
#define CHECK_REMAINING(nbytes, label)                           \
  do {                                                           \
    if (remaining < (nbytes) OR_DEADCODE_DUMMY) {                \
      goto label;                                                \
    }                                                            \
  } while (0)

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

#define CHUNK_SIZE (MAX_MESSAGE_LEN - 1)

static smartlist_t *global_payment_session_list = NULL;
static smartlist_t *global_chunks_list = NULL;

static smartlist_t *global_payment_api_messsages = NULL;
static tor_mutex_t global_payment_mutex;
static tor_cond_t global_payment_cond;


static error_t circuit_payment_send_command_to_hop(origin_circuit_t *circ, uint8_t hopnum, uint8_t relay_command, const uint8_t *payload, ssize_t payload_len);
static error_t circuit_payment_send_command_to_origin(circuit_t *circ, uint8_t relay_command, const uint8_t *payload, ssize_t payload_len);
static OR_OP_request_t * payment_payload_new(void);

static char s_cb_url_api_response[PAYMENT_URL_LEN];
static char s_cb_url_api_command[PAYMENT_URL_LEN];
static char s_cb_url_api_paymentComplete[PAYMENT_URL_LEN];

static char s_url_api_util_createPaymentInfo[PAYMENT_URL_LEN];
static char s_url_api_util_processCommand[PAYMENT_URL_LEN];
static char s_url_api_gw_processPayment[PAYMENT_URL_LEN];
static char s_url_api_gw_processResponse[PAYMENT_URL_LEN];

typedef struct tor_command {
    const char * commandBody;
    const char * commandId;
    const char * commandType;
    const char * nodeId;
    const char * sessionId;
    const char * json_body;
} tor_command;

typedef struct payment_completed {
    const char * sessionId;
    int status;
    const char * json_body;
} payment_completed;

typedef struct tor_command_replay {
    const char * commandResponse;
    const char * commandId;
    const char * nodeId;
    const char * sessionId;
    const char * json_body;
    const char * commandType;
} tor_command_replay;

static int circuit_get_length(origin_circuit_t * circ)
{
    int n = 0;
    if (circ != NULL && circ->cpath != NULL) {
        crypt_path_t *cpath, *cpath_next = NULL;
        for (cpath = circ->cpath;
                cpath->state == CPATH_STATE_OPEN && cpath_next != circ->cpath;
                cpath = cpath_next) {
            cpath_next = cpath->next;
            ++n;
            if(cpath_next->extend_info == NULL) return n;
        }
    }
    return n;
}

typedef struct payment_message_for_http_st {
    const char *url_part;
    void *msg;
    int done;
    int result;
} payment_message_for_http_t;

typedef struct payment_message_for_http_handler_st {
    const char *method;
    const char *url;
    int (*handler_fn)(payment_message_for_http_t *message);
    int (*request_fn)(const char *url_part, tor_http_api_request_t *request);
} payment_message_for_http_handler_t;

static int tp_send_http_api_request(payment_message_for_http_t *request)
{
    request->done = 0;
    tor_mutex_acquire(&global_payment_mutex);
    smartlist_add(global_payment_api_messsages, request);
    int rc = 0;
    while(0 == (rc = tor_cond_wait(&global_payment_cond, &global_payment_mutex, NULL))) {
        if (request->done) {
            break;
        }
    }
    tor_mutex_release(&global_payment_mutex);
    if (rc)
        return TOR_HTTP_RESULT_UNKNOWN;
    return request->result;
}

typedef struct tor_route {
    routing_node_t* nodes;
    size_t nodes_len;
    char sessionId[SESSION_ID_LEN + 1];
    char exclude_node_id[MAX_HEX_NICKNAME_LEN + 1];
    char exclude_address[STELLAR_ADDRESS_LEN + 1];
    const char* call_back_url;
    const char* status_call_back_url;
} tor_route;

static void tp_process_payment_for_sending(payment_message_for_sending_t* message);

static const char* tp_get_buffer_part_number(const char * buffer, size_t buffer_size, size_t part_size, size_t part_number, size_t *real_part_size)
{
    const size_t part_offset = part_number * part_size;
    const char *buf_ptr = &buffer[part_offset];
    if ((part_offset + part_size) > buffer_size)
        *real_part_size = buffer_size - part_offset;
    else
        *real_part_size = part_size;

    return buf_ptr;
}

void tp_fill_stellar_address(char *dst)
{
DISABLE_GCC_WARNING("-Wunknown-warning-option")
DISABLE_GCC_WARNING("-Wstringop-truncation")
    strlcpy(dst, get_options()->StellarAddress, STELLAR_ADDRESS_LEN);
ENABLE_GCC_WARNING("-Wstringop-truncation")
ENABLE_GCC_WARNING("-Wunknown-warning-option")
}

#define VALID_STELLAR_ADDRESS_LEN 56

int is_invalid_stellar_address(const char *addr)
{
    if (!addr)
        return -1;
    if(0 == addr[0])
        return -2;
    // const int addr_len = strnlen(addr, STELLAR_ADDRESS_LEN);
    // if (0 == addr_len)
    //     return ;
    // if (STELLAR_ADDRESS_LEN <= addr_len)
    //     return ;
    // if (VALID_STELLAR_ADDRESS_LEN != addr_len)
    //     return ;
    if(addr[0] != 'G')
        return -3;
    for (size_t i = 1; i < VALID_STELLAR_ADDRESS_LEN; i++) {
        const char c = addr[i];
        if (!isupper(c) && !isdigit(c))
            return -4;
    }
    return 0;
}

static const char* tp_get_address(void)
{
    int request_port = get_options()->PPChannelPort;
    char url[PAYMENT_URL_LEN];
    snprintf(url, PAYMENT_URL_LEN, "%s:%d/%s", "http://127.0.0.1", request_port, "api/utility/stellarAddress");
    json_object* json_response = tp_http_get_request(url);
    json_object *address_obj = json_object_object_get(json_response, "Address");
    if (NULL == address_obj)
        return NULL;
    const char *address = json_object_get_string(address_obj);
    if (NULL != address)
        return tor_strdup(address);
    return NULL;
}

static void tp_rest_log(const char * message)
{
    if(!get_options()->EnablePaymentLog)
        return;
    log_info(LD_HTTP,"Payment REST_LOG:\n%s", message);
}

void tp_init_lists(void)
{
    tor_mutex_init_for_cond(&global_payment_mutex);
    tor_cond_init(&global_payment_cond);
    global_payment_api_messsages = smartlist_new();
    global_payment_session_list = smartlist_new();
    global_chunks_list = smartlist_new();
}

void tp_scan_sessions(void)
{
    time_t now = time(NULL);
    time_t life_time = get_options()->PPSessionLifetime;
    smartlist_t *outdated = NULL;
    SMARTLIST_FOREACH_BEGIN(global_payment_session_list, payment_session_context_t *, element) {
        if ((now - element->timestamp_created) > life_time) {
            if (!outdated) {
                outdated = smartlist_new();
                tor_assert(outdated != NULL);
            }
            smartlist_add(outdated, element);
        }
    } SMARTLIST_FOREACH_END(element);
    if (outdated) {
        log_err(LD_CIRC, "Cleaning %i outdated payment sessions", smartlist_len(outdated));
        SMARTLIST_FOREACH_BEGIN(outdated, payment_session_context_t *, element) {
            smartlist_remove(global_payment_session_list, element);
            tor_free_(element);
        } SMARTLIST_FOREACH_END(element);
        smartlist_free(outdated);
    }
}

static periodic_timer_t *s_limit_refresh_timer = NULL;

static void tp_timer_callback(periodic_timer_t *timer, void *data);

static void tp_init_timer(void)
{
    static const struct timeval interval = {0, 100000};
    s_limit_refresh_timer = periodic_timer_new(tor_libevent_get_base(), &interval, tp_timer_callback, NULL);
}

static void tp_deinit_timer(void)
{
    periodic_timer_free(s_limit_refresh_timer);
}

static int tp_rest_api_direct(const char *url_part, tor_http_api_request_t *request)
{
    payment_message_for_http_t message;
    tp_zero_mem(&message, sizeof(message));
    message.url_part = url_part;
    message.msg = request;
    int rc =  tp_send_http_api_request(&message);
    ship_log(PAYMENT_CALLBACK, request->url, request->body, request->answer_body);
    return rc;
}

typedef struct tor_api_onehop_st {
    tor_http_api_request_t *request;
    routerset_t *nodes;
    int reset;
} tor_api_onehop_t;

typedef struct tor_api_circuit_length_st {
    tor_http_api_request_t *request;
    int circuit_length;
} tor_api_circuit_length_t;

static int tp_rest_api_onehop(const char *url_part, tor_http_api_request_t *request)
{
    if (!request->body)
        return TOR_HTTP_RESULT_WRONG_BODY;
    int rc = TOR_HTTP_RESULT_UNKNOWN;
    log_notice(LD_HTTP, "%s request: %s", url_part, request->body);
    struct json_object *json = NULL;
    do {
        enum json_tokener_error jerr = json_tokener_success;
        json = json_tokener_parse_verbose(request->body, &jerr);
        if (jerr != json_tokener_success) {
            log_err(LD_HTTP, "Can't parse json object (reason:%s) from: %s", json_tokener_error_desc(jerr), request->body);
            rc = TOR_HTTP_RESULT_WRONG_JSON;
            break;
        }
        struct json_object *OneHopNodesObj = NULL;
        if (!json_object_object_get_ex(json, "Nodes", &OneHopNodesObj)){
            rc = TOR_HTTP_RESULT_WRONG_JSON;
            break;
        }
        const char *OneHopNodes = json_object_get_string(OneHopNodesObj);
        if (NULL == OneHopNodes) {
            rc = TOR_HTTP_RESULT_WRONG_PARAMETER;
            break;
        }
        routerset_t *exit_rs = routerset_new();
        if (routerset_parse(exit_rs, OneHopNodes, "OneHopNodes") == 0) {
            tor_api_onehop_t onehop;
            tp_zero_mem(&onehop, sizeof(onehop));
            onehop.nodes = exit_rs;
            onehop.reset = !strcmp(OneHopNodes, "");
            payment_message_for_http_t request_msg;
            tp_zero_mem(&request_msg, sizeof(request_msg));
            request_msg.url_part = url_part;
            request_msg.msg = &onehop;
            rc = tp_send_http_api_request(&request_msg);
        } else {
            routerset_free(exit_rs);
            rc = TOR_HTTP_RESULT_WRONG_PARAMETER;
        }
    } while(false);
    if (json)
        json_object_put(json);
    return rc;
}

static int tp_rest_api_circuit_length(const char *url_part, tor_http_api_request_t *request)
{
    if (!request->body)
        return TOR_HTTP_RESULT_WRONG_BODY;
    int rc = TOR_HTTP_RESULT_UNKNOWN;
    log_notice(LD_HTTP, "%s request: %s", url_part, request->body);
    struct json_object *json = NULL;
    do {
        enum json_tokener_error jerr = json_tokener_success;
        json = json_tokener_parse_verbose(request->body, &jerr);
        if (jerr != json_tokener_success) {
            log_err(LD_HTTP, "Can't parse json object (reason:%s) from: %s", json_tokener_error_desc(jerr), request->body);
            rc = TOR_HTTP_RESULT_WRONG_JSON;
            break;
        }
        struct json_object *Obj = NULL;
        if (!json_object_object_get_ex(json, "length", &Obj)){
            rc = TOR_HTTP_RESULT_WRONG_JSON;
            break;
        }
        const int length = json_object_get_int(Obj);
        if (length < 2) {
            rc = TOR_HTTP_RESULT_WRONG_PARAMETER;
            break;
        }
            tor_api_circuit_length_t msg;
            tp_zero_mem(&msg, sizeof(msg));
            msg.circuit_length = length;
            payment_message_for_http_t request_msg;
            tp_zero_mem(&request_msg, sizeof(request_msg));
            request_msg.url_part = url_part;
            request_msg.msg = &msg;
            rc = tp_send_http_api_request(&request_msg);
    } while(false);
    if (json)
        json_object_put(json);
    return rc;
}

static int tp_rest_handler(tor_http_api_request_t *request);

void tp_init(void)
{
    const char* stellar = tp_get_address();
    if(NULL != stellar) {
        or_options_t *options = get_options_mutable();
        options->StellarAddress = tor_strdup(stellar);
    }

    const int ppc_port = get_options()->PPChannelPort;
    const int callback_port = get_options()->PPChannelCallbackPort;

    snprintf(s_cb_url_api_response, PAYMENT_URL_LEN,  "http://localhost:%d/api/response", callback_port);
    snprintf(s_cb_url_api_command, PAYMENT_URL_LEN, "http://localhost:%d/api/command", callback_port);
    snprintf(s_cb_url_api_paymentComplete, PAYMENT_URL_LEN, "http://localhost:%d/api/paymentComplete", callback_port);

    snprintf(s_url_api_util_createPaymentInfo, PAYMENT_URL_LEN, "http://localhost:%d/api/utility/createPaymentInfo", ppc_port);
    snprintf(s_url_api_util_processCommand, PAYMENT_URL_LEN, "http://localhost:%d/api/utility/processCommand", ppc_port);
    snprintf(s_url_api_gw_processPayment, PAYMENT_URL_LEN, "http://localhost:%d/api/gateway/processPayment", ppc_port);
    snprintf(s_url_api_gw_processResponse, PAYMENT_URL_LEN, "http://localhost:%d/api/gateway/processResponse", ppc_port);

    tp_init_lists();
 
    const int ppcb_port = get_options()->PPChannelCallbackPort;
    if ( ppcb_port != -1 ) {
        runServer(ppcb_port, tp_rest_log, tp_rest_handler);
    }

    tp_init_timer();
}

void tp_deinit(void)
{
    tp_deinit_timer();
    stopServer();
}

static const node_t* circuit_payment_get_nth_node(origin_circuit_t *circ, int hop) {
    crypt_path_t *iter = circuit_get_cpath_hop(circ, hop);

    if (!iter || iter->state != CPATH_STATE_OPEN)
        return NULL;

    return node_get_by_id(iter->extend_info->identity_digest);
}

static error_t circuit_payment_send_OP(circuit_t *circ, uint8_t target_hopnum, OR_OP_request_t* input)
{
    cell_t cell;
    ssize_t len;
    origin_circuit_t* orig_circ = TO_ORIGIN_CIRCUIT(circ);

    if (!circuit_payment_get_nth_node(orig_circ, target_hopnum)) {
        return 0;
    }

    tp_zero_mem(&cell, sizeof(cell_t));

    cell.command = CELL_RELAY;

    if ((len = circuit_payment_negotiate_encode(cell.payload, CELL_PAYLOAD_SIZE, input)) < 0)
        return 0;

    error_t res = circuit_payment_send_command_to_hop(orig_circ, target_hopnum,
                                               RELAY_COMMAND_PAYMENT_COMMAND_TO_NODE,
                                               cell.payload, len);
    return res;
}

static error_t circuit_payment_send_OR(circuit_t *circ, OR_OP_request_t* input)
{
    uint8_t payload[RELAY_PAYLOAD_SIZE];
    ssize_t len;
    if (CIRCUIT_IS_ORIGIN(circ))
        return 0;
    if ((len = circuit_payment_negotiate_encode(payload, CELL_PAYLOAD_SIZE,  input)) < 0)
        return 0;
    return circuit_payment_send_command_to_origin(circ,
        RELAY_COMMAND_PAYMENT_COMMAND_TO_ORIGIN, payload, len);
}

static error_t circuit_payment_send_command_to_hop(origin_circuit_t *circ, uint8_t hopnum, uint8_t relay_command, const uint8_t *payload, ssize_t payload_len)
{
    crypt_path_t *target_hop = circuit_get_cpath_hop(circ, hopnum);
    error_t ret;
/* Check that the cpath has the target hop */
    if (!target_hop) {
        log_fn(LOG_WARN, LD_BUG, "Padding circuit %u has %d hops, not %d",
               circ->global_identifier, circuit_get_cpath_len(circ), hopnum);
        return 0;
    }
/* Check that the target hop is opened */
    if (target_hop->state != CPATH_STATE_OPEN) {
        log_fn(LOG_WARN, LD_CIRC,
               "Padding circuit %u has %d hops, not %d",
               circ->global_identifier,
               circuit_get_cpath_opened_len(circ), hopnum);
        return 0;
    }

/* Send the drop command to the hop */
    ret = relay_send_command_from_edge(0, TO_CIRCUIT(circ), relay_command,
                                       (void*)payload, (size_t)payload_len,
                                       target_hop);
    return ret;
}

static error_t circuit_payment_send_command_to_origin(circuit_t *circ, uint8_t relay_command, const uint8_t *payload, ssize_t payload_len)
{
    error_t ret;
/* Send the drop command to the origin */
    ret = relay_send_command_from_edge(0, circ, relay_command,
                                       (void*)payload, (size_t)payload_len,
                                       NULL);
    return ret;
}

static OR_OP_request_t* circuit_payment_handle_payment_negotiate(const cell_t *cell)
{
    OR_OP_request_t *negotiate;
    if (circuit_payment_negotiate_parse(&negotiate, cell->payload+RELAY_HEADER_SIZE,
                                        CELL_PAYLOAD_SIZE-RELAY_HEADER_SIZE) < 0) {
        log_fn(LOG_PROTOCOL_WARN, LD_CIRC,
               "Received malformed payment negotiate cell; dropping.");
        return NULL;
    }

    return negotiate;
}

static void circuit_payment__free(OR_OP_request_t *obj)
{
    if (obj == NULL)
        return;
    trunnel_memwipe(obj, sizeof(circpad_negotiate_t));
    trunnel_free_(obj);
}

static OR_OP_request_t * payment_payload_new(void)
{
    OR_OP_request_t *val = tor_malloc_(sizeof(OR_OP_request_t));
    if (NULL == val)
        return NULL;
    val->version = PAYMENT_MSG_VERSION;
    return val;
}

static ssize_t payment_into(OR_OP_request_t *obj, const uint8_t *input, const size_t len_in)
{
    const uint8_t *ptr = input;
    size_t remaining = len_in;
    ssize_t result = 0;
    (void)result;

    /* Parse u8 version IN [0] */
    CHECK_REMAINING(1, truncated);
    obj->version = (trunnel_get_uint8(ptr));
    remaining -= 1; ptr += 1;

    /* Parse u8 command IN [CELL_PAYMENT_REQUEST] */
    CHECK_REMAINING(1, truncated);
    obj->command = (trunnel_get_uint8(ptr));
    remaining -= 1; ptr += 1;

    /* Parse u8 command IN [CELL_PAYMENT_REQUEST] */
    CHECK_REMAINING(1, truncated);
    obj->message_type = (trunnel_get_uint8(ptr));
    remaining -= 1; ptr += 1;

    /* Parse u8 command IN [CELL_PAYMENT_REQUEST] */
    CHECK_REMAINING(1, truncated);
    obj->is_last = (trunnel_get_uint8(ptr));
    remaining -= 1; ptr += 1;

    /* Parse u8 command IN [CELL_PAYMENT_REQUEST] */
    CHECK_REMAINING(2, truncated);
    obj->command_type = (trunnel_get_int16(ptr));
    remaining -= 2; ptr += 2;

    CHECK_REMAINING(2, truncated);
    obj->session_id_length = (trunnel_get_uint16(ptr));
    remaining -= 2; ptr += 2;

    /* Parse char name[len] */
    CHECK_REMAINING(SESSION_ID_LEN, fail);
    memcpy(obj->session_id, ptr, SESSION_ID_LEN);
    remaining -= SESSION_ID_LEN; ptr += SESSION_ID_LEN;

    CHECK_REMAINING(2, truncated);
    obj->command_id_length = (trunnel_get_uint16(ptr));
    remaining -= 2; ptr += 2;

    /* Parse char name[len] */
    CHECK_REMAINING(COMMAND_ID_LEN, fail);
    memcpy(obj->command_id, ptr, COMMAND_ID_LEN);
    remaining -= COMMAND_ID_LEN; ptr += COMMAND_ID_LEN;

    CHECK_REMAINING(2, truncated);
    obj->nicknameLength = (trunnel_get_uint16(ptr));
    remaining -= 2; ptr += 2;


    /* Parse char name[len] */
    CHECK_REMAINING(USER_NAME_LEN, fail);
    memcpy(obj->nickname, ptr, USER_NAME_LEN);
    remaining -= USER_NAME_LEN; ptr += USER_NAME_LEN;

    CHECK_REMAINING(2, truncated);
    obj->messageLength = (trunnel_get_uint16(ptr));
    remaining -= 2; ptr += 2;

    /* Parse char name[len] */
    CHECK_REMAINING(MAX_MESSAGE_LEN, fail);
    memcpy(obj->message, ptr, MAX_MESSAGE_LEN);
    remaining -= MAX_MESSAGE_LEN; ptr += MAX_MESSAGE_LEN;

    CHECK_REMAINING(2, truncated);
    obj->messageTotalLength = (trunnel_get_uint16(ptr));
    remaining -= 2; ptr += 2;

    trunnel_assert(ptr + remaining == input + len_in);
    return len_in - remaining;

    truncated:
    return -2;
    fail:
    result = -1;
    return result;
}

ssize_t circuit_payment_negotiate_parse(OR_OP_request_t **output, const uint8_t *input, const size_t len_in)
{
    ssize_t result;
    *output = payment_payload_new();
    if (NULL == *output)
        return -1;
    result = payment_into(*output, input, len_in);
    if (result < 0) {
        circuit_payment__free(*output);
        *output = NULL;
    }
    return result;
}

ssize_t circuit_payment_negotiate_encode(uint8_t *output, const size_t avail, const OR_OP_request_t *obj)
{
    ssize_t result = 0;
    size_t written = 0;
    uint8_t *ptr = output;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
    const ssize_t encoded_len = circpad_negotiate_encoded_len(obj);
#endif

#ifdef TRUNNEL_CHECK_ENCODED_LEN
    trunnel_assert(encoded_len >= 0);
#endif

    /* Encode u8 version IN [0] */
    trunnel_assert(written <= avail);
    if (avail - written < 1)
        goto truncated;
    trunnel_set_uint8(ptr, (obj->version));
    written += 1; ptr += 1;

    /* Encode u8 command IN [CIRCPAD_COMMAND_START, CIRCPAD_COMMAND_STOP] */
    trunnel_assert(written <= avail);
    if (avail - written < 1)
        goto truncated;
    trunnel_set_uint8(ptr, (obj->command));
    written += 1; ptr += 1;

    /* Encode u8 command IN [CIRCPAD_COMMAND_START, CIRCPAD_COMMAND_STOP] */
    trunnel_assert(written <= avail);
    if (avail - written < 1)
        goto truncated;
    trunnel_set_uint8(ptr, (obj->message_type));
    written += 1; ptr += 1;

    /* Encode u8 command IN  */
    trunnel_assert(written <= avail);
    if (avail - written < 1)
        goto truncated;
    trunnel_set_uint8(ptr, (obj->is_last));
    written += 1; ptr += 1;

    /* Encode u8 command IN  */
    trunnel_assert(written <= avail);
    if (avail - written < 2)
        goto truncated;
    trunnel_set_int16(ptr, (obj->command_type));
    written += 2; ptr += 2;

    /* Encode u8 command IN [CIRCPAD_COMMAND_START, CIRCPAD_COMMAND_STOP] */
    trunnel_assert(written <= avail);
    if (avail - written < 2)
        goto truncated;
    trunnel_set_uint16(ptr, (obj->session_id_length));
    written += 2; ptr += 2;

    /* Encode u4 data[TRUNNEL_PAYMENT_LEN] */
    trunnel_assert(written <= avail);
    if (avail - written < SESSION_ID_LEN)
        goto truncated;
    memcpy(ptr, obj->session_id,SESSION_ID_LEN);
    written += SESSION_ID_LEN; ptr += SESSION_ID_LEN;
    trunnel_assert(ptr == output + written);

 /* Encode u8 command IN [CIRCPAD_COMMAND_START, CIRCPAD_COMMAND_STOP] */
    trunnel_assert(written <= avail);
    if (avail - written < 2)
        goto truncated;
    trunnel_set_uint16(ptr, (obj->command_id_length));
    written += 2; ptr += 2;

    /* Encode u4 data[TRUNNEL_PAYMENT_LEN] */
    trunnel_assert(written <= avail);
    if (avail - written < COMMAND_ID_LEN)
        goto truncated;
    memcpy(ptr, obj->command_id,COMMAND_ID_LEN);
    written += COMMAND_ID_LEN; ptr += COMMAND_ID_LEN;
    trunnel_assert(ptr == output + written);

    /* Encode u8 command IN [CIRCPAD_COMMAND_START, CIRCPAD_COMMAND_STOP] */
    trunnel_assert(written <= avail);
    if (avail - written < 2)
        goto truncated;
    trunnel_set_uint16(ptr, (obj->nicknameLength));
    written += 2; ptr += 2;

    /* Encode u4 data[TRUNNEL_PAYMENT_LEN] */
    trunnel_assert(written <= avail);
    if (avail - written < USER_NAME_LEN)
        goto truncated;
    memcpy(ptr, obj->nickname,USER_NAME_LEN);
    written += USER_NAME_LEN; ptr += USER_NAME_LEN;
    trunnel_assert(ptr == output + written);

    /* Encode u8 command IN [CIRCPAD_COMMAND_START, CIRCPAD_COMMAND_STOP] */
    trunnel_assert(written <= avail);
    if (avail - written < 2)
        goto truncated;
    trunnel_set_uint16(ptr, (obj->messageLength));
    written += 2; ptr += 2;

    trunnel_assert(written <= avail);
    if (avail - written < MAX_MESSAGE_LEN)
        goto truncated;
    memcpy(ptr, obj->message, MAX_MESSAGE_LEN);
    written += MAX_MESSAGE_LEN; ptr += MAX_MESSAGE_LEN;
    trunnel_assert(ptr == output + written);

    /* Encode u8 command IN [CIRCPAD_COMMAND_START, CIRCPAD_COMMAND_STOP] */
    trunnel_assert(written <= avail);
    if (avail - written < 2)
        goto truncated;
    trunnel_set_uint16(ptr, (obj->messageTotalLength));
    written += 2; ptr += 2;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
    {
    trunnel_assert(encoded_len >= 0);
    trunnel_assert((size_t)encoded_len == written);
  }

#endif

    return written;

    truncated:
    result = -2;
    goto fail;
    result = -1;
    goto fail;
    fail:
    trunnel_assert(result < 0);
    return result;
}


static int circuit_get_num_by_nickname(origin_circuit_t * circ, const char* nickname)
{
    int n = 1;
    if(strcmp(circ->cpath->extend_info->nickname, nickname) == 0)
        return 1;
    if (circ != NULL && circ->cpath != NULL) {
        crypt_path_t *cpath, *cpath_next = NULL;
        for (cpath = circ->cpath;
             cpath->state == CPATH_STATE_OPEN
             && cpath_next != circ->cpath;
             cpath = cpath_next) {
            cpath_next = cpath->next;
            ++n;
            if(cpath_next->extend_info == NULL) return 0;
            if(strcmp(cpath_next->extend_info->nickname, nickname) == 0)
                return n;
        }
    }
    return 0;
}

void tp_store_session_context(const char* session, const char* nickname, uint64_t channel_global_id, uint32_t circuit_id)
{
    payment_session_context_t *origin = get_from_session_context_by_session_id(session);

    if (origin == NULL) {
        payment_session_context_t *ent = (payment_session_context_t *) tor_malloc_zero_(
                sizeof(payment_session_context_t));
        strcpy(ent->session_id, session);
        strcpy(ent->nickname, nickname);
        ent->channel_global_id = channel_global_id;
        ent->circuit_id = circuit_id;
        ent->timestamp_created = time(NULL);
        smartlist_add(global_payment_session_list, ent);
    } else {
        strcpy(origin->session_id, session);
        strcpy(origin->nickname, nickname);
        origin->channel_global_id = channel_global_id;
        origin->circuit_id = circuit_id;
    }
}

payment_session_context_t* get_from_session_context_by_session_id(const char* session)
{
    SMARTLIST_FOREACH_BEGIN(global_payment_session_list, payment_session_context_t *, element) {
        if (strcmp(element->session_id, session) == 0) {
            return element;
        }
    } SMARTLIST_FOREACH_END(element);

    return NULL;
}

static payment_session_context_t* get_from_session_context(uint64_t channel_global_id, circid_t circ_id)
{
    SMARTLIST_FOREACH_BEGIN(global_payment_session_list, payment_session_context_t *, element) {
        if (element->circuit_id == circ_id &&
            element->channel_global_id == channel_global_id) {
            return element;
        }
    } SMARTLIST_FOREACH_END(element);

    return NULL;
}

void remove_from_session_context(payment_session_context_t* element)
{
    smartlist_remove(global_payment_session_list, element);
    tor_free_(element);
}

static void tp_get_sessions(smartlist_t *output_list, uint64_t channel_global_id, circid_t circ_id)
{
    SMARTLIST_FOREACH_BEGIN(global_payment_session_list, payment_session_context_t *, element) {
        if (element->circuit_id == circ_id && element->channel_global_id == channel_global_id) {
            smartlist_add(output_list, element);
        }
    } SMARTLIST_FOREACH_END(element);
}

void tp_free_session_context(uint64_t channel_global_id, circid_t circ_id)
{
    smartlist_t *remove_list = smartlist_new();
    tp_get_sessions(remove_list, channel_global_id, circ_id);
    SMARTLIST_FOREACH_BEGIN(remove_list, payment_session_context_t *, element) {
        remove_from_session_context(element);
    } SMARTLIST_FOREACH_END(element);
    smartlist_free(remove_list);
}

static payment_chunks_t * tp_store_chunk(const OR_OP_request_t* payment_request_payload, const char* key)
{
    payment_chunks_t *origin = NULL;

    SMARTLIST_FOREACH_BEGIN(global_chunks_list, payment_chunks_t *, element) {
        if(strcmp(element->key, key) == 0){
            origin = element;
            break;
        }
    } SMARTLIST_FOREACH_END(element);

    if(origin == NULL){
        payment_chunks_t* ent = (payment_chunks_t*)tor_malloc_(sizeof(payment_chunks_t));
        memcpy(ent->key, key, PAYMENT_HASH_KEY_LEN);
        memcpy(ent->value, payment_request_payload->message, MAX_MESSAGE_LEN);
        origin = ent;
        smartlist_add(global_chunks_list, ent);
    }
    else {
        strncat(origin->value, payment_request_payload->message, payment_request_payload->messageLength);
    }

    return origin;
}

static void tp_update_circ_counters(or_circuit_t *or_circut)
{
    tor_assert(or_circut);

    or_circut->delay_payments_counter++;

    if (!get_options()->PPEnableSlowing)
        return;

    if (or_circut->delay_payments_counter < 5)
        return;

    if (or_circut->is_limited)
        return;

    or_circut->is_limited = 1;
    log_notice(LD_OR, "tp_update_circ_counters: use circuit bandwidth limitation");

    circuit_t *circ = TO_CIRCUIT(or_circut);
    if (or_circut->p_chan)
        circuitmux_circ_set_limited(or_circut->p_chan->cmux, circ, CELL_DIRECTION_IN);
    if (circ->n_chan)
        circuitmux_circ_set_limited(circ->n_chan->cmux, circ, CELL_DIRECTION_OUT);
}

typedef struct send_payment_request_to_client_st {
    circid_t circuit_id;
    uint64_t channel_global_id;
    char *response;
    struct json_object *json;
    char *session;
} send_payment_request_to_client_t;

static void free_send_payment_request_to_client_job(send_payment_request_to_client_t *job)
{
    if (job) {
        tor_free(job->response);
        tor_free(job->session);
        if (job->json) {
            json_object_put(job->json);
            job->json = NULL;
        }
    }
    tor_free_(job);
}

static workqueue_reply_t send_payment_request_to_client_threadfn(void *state_, void *work_)
{
    (void)state_;
    create_payment_info_t request;
    send_payment_request_to_client_t *job = work_;

    request.service_type = "tor";
    request.commodity_type = "data";
    request.amount = 10;

    job->response = tp_create_payment_info(s_url_api_util_createPaymentInfo, &request);
    if (job->response == NULL)
        return WQ_RPL_REPLY;

    do {
        enum json_tokener_error jerr = json_tokener_success;
        job->json = json_tokener_parse_verbose(job->response, &jerr);
        if (jerr != json_tokener_success) {
            tor_assert_nonfatal(NULL == job->json);
            log_err(LD_HTTP, "Can't parse json object (reason:%s) from: %s", json_tokener_error_desc(jerr), job->response);
            break;
        }
        struct json_object *session_id = NULL;
        if (!json_object_object_get_ex(job->json, "ServiceSessionId", &session_id))
            break;
        const char *session_str = json_object_get_string(session_id);
        if (session_str == NULL)
            break;
        job->session = (0 < strlen(session_str)) ?  tor_strdup(session_str): NULL;
    } while(false);

    return WQ_RPL_REPLY;
}

static circuit_t *tp_find_or_circuit(uint64_t global_identifier, circid_t circ_id)
{
    smartlist_t *circuits = circuit_get_global_list();
    SMARTLIST_FOREACH_BEGIN(circuits, circuit_t *, element) {
        if(CIRCUIT_IS_ORIGIN(element))
            continue;
        tor_assert(CIRCUIT_IS_ORCIRC(element));
        or_circuit_t *or_circ = TO_OR_CIRCUIT(element);
        if (or_circ->p_chan->global_identifier != global_identifier)
            continue;
        if (or_circ->p_circ_id == circ_id) {
            return element;
        }
    } SMARTLIST_FOREACH_END(element);
    return NULL;
}

static void send_payment_request_to_client_replyfn(void * work_)
{
    send_payment_request_to_client_t *job = work_;
    if (NULL == job ||
        NULL == job->session ||
        NULL == job->response) {
        free_send_payment_request_to_client_job(job);
        return;
    }
    circuit_t *circ = tp_find_or_circuit(job->channel_global_id, job->circuit_id);
    
    if(!circ) {
        free_send_payment_request_to_client_job(job);
        return;
    }

    OR_OP_request_t input;
    input.version = 0;
    input.message_type = 1;
    tp_zero_mem(input.command_id, COMMAND_ID_LEN);
    tp_zero_mem(input.session_id, SESSION_ID_LEN);
    input.command_id_length = 0;
    input.session_id_length = strlen(job->session);
    memcpy(input.session_id, job->session, SESSION_ID_LEN);
    input.command = RELAY_COMMAND_PAYMENT_COMMAND_TO_ORIGIN;

    const or_options_t *options = get_options();
    tp_zero_mem(input.nickname, USER_NAME_LEN);
    memcpy(input.nickname, options->Nickname, USER_NAME_LEN);
    input.nicknameLength = strlen(options->Nickname);
    input.is_last = 0;
    const size_t body_len = strlen(job->response);
    input.messageTotalLength = body_len;
    const size_t part_size =  body_len / CHUNK_SIZE;
    for (size_t g = 0; g <= part_size; g++) {
        tp_zero_mem(input.message, MAX_MESSAGE_LEN);
        size_t chunck_real_size = 0;
        const char * buf_ptr = tp_get_buffer_part_number(job->response, body_len, CHUNK_SIZE, g, &chunck_real_size);
        memcpy(input.message, buf_ptr, chunck_real_size);
        input.message[chunck_real_size] = 0;
        input.messageLength = chunck_real_size;
        input.is_last = (g < part_size) ? 0 : 1;
        circuit_payment_send_OR(circ, &input);
    }
    free_send_payment_request_to_client_job(job);
}

void tp_send_payment_request_to_client_async(circuit_t *circ, int message_number)
{
    (void)message_number;
    circ->total_package_received = 0;
    circ->total_package_sent = 0;

    or_circuit_t * or_circ = TO_OR_CIRCUIT(circ);
    tp_update_circ_counters(or_circ);

    send_payment_request_to_client_t *job = tor_calloc(1, sizeof(send_payment_request_to_client_t));
    tor_assert(NULL != job);
    job->channel_global_id = or_circ->p_chan->global_identifier;
    job->circuit_id = or_circ->p_circ_id;
    workqueue_entry_t *work = 
        cpuworker_queue_work(WQ_PRI_LOW, send_payment_request_to_client_threadfn, send_payment_request_to_client_replyfn, job);

    tor_assert_nonfatal(NULL != work);
}

static void tp_circuitmux_reset_limits(circuit_t * circ)
{
    tor_assert(circ);
    tor_assert(CIRCUIT_IS_ORCIRC(circ));
    circ->cell_limit = 0;
    or_circuit_t *or_circut = TO_OR_CIRCUIT(circ);
    or_circut->delay_payments_counter = 0;
    if (!or_circut->is_limited)
        return;
    or_circut->is_limited = 0;
    log_notice(LD_OR, "tp_circuitmux_reset_limits: reset circuit bandwidth limitation");

    if (or_circut->p_chan)
        circuitmux_circ_reset_limited(or_circut->p_chan->cmux, circ, CELL_DIRECTION_IN);
    if (circ->n_chan)
        circuitmux_circ_reset_limited(circ->n_chan->cmux, circ, CELL_DIRECTION_OUT);

}

typedef struct process_payment_cell_st {
    OR_OP_request_t *payload;
    payment_chunks_t* chunk;
}  process_payment_cell_t;

static workqueue_reply_t process_payment_cell_threadfn(void *state_, void *work_)
{
    if(NULL == state_ || NULL == work_)
        return WQ_RPL_ERROR;
    process_payment_cell_t *job = (process_payment_cell_t*)work_;
    utility_command_t request;
    request.command_type = job->payload->command_type;
    request.command_body = job->chunk->value;
    request.node_id = job->payload->nickname;
    request.callback_url = s_cb_url_api_response;
    request.command_id = job->payload->command_id;
    request.session_id = job->payload->session_id;
    tp_http_command(s_url_api_util_processCommand, &request);
    tor_free(job->payload);
    tor_free(job->chunk);
    tor_free(job);
    return WQ_RPL_REPLY;
}

static void process_payment_cell_replyfn(void * arg)
{
    (void)arg;
}

int tp_process_payment_cell_async(const cell_t *cell, circuit_t *circ)
{
    OR_OP_request_t *payment_request_payload = circuit_payment_handle_payment_negotiate(cell);
    char cell_key [PAYMENT_HASH_KEY_LEN];
    strcat(strcat(strcpy(cell_key, payment_request_payload->nickname), "|"), payment_request_payload->session_id);
    payment_chunks_t* origin = tp_store_chunk(payment_request_payload, cell_key);

    if(!payment_request_payload->is_last) {
        tor_free(payment_request_payload);
        return 0;
    }

    smartlist_remove(global_chunks_list, origin);

    if(payment_request_payload->message_type != 100) {
        or_circuit_t * or_circ = TO_OR_CIRCUIT(circ);
        tp_store_session_context(payment_request_payload->session_id,
            payment_request_payload->nickname,
            or_circ->p_chan->global_identifier,
            or_circ->p_circ_id);

        process_payment_cell_t *job = tor_calloc(1, sizeof(process_payment_cell_t));
        job->chunk = origin;
        job->payload = payment_request_payload;
    
        workqueue_entry_t *work = 
            cpuworker_queue_work(WQ_PRI_LOW, process_payment_cell_threadfn, process_payment_cell_replyfn, job);
        tor_assert_nonfatal(NULL != work);
        return 0;
    }

    payment_session_context_t *session_context =
            get_from_session_context_by_session_id(payment_request_payload->session_id);
    if(session_context != NULL)
        remove_from_session_context(session_context);
    tor_free(origin);
    tor_free(payment_request_payload);
    return 0;
}
typedef struct process_payment_command_cell_to_node_st {
    OR_OP_request_t *request;
    payment_chunks_t* chunk;
    routing_node_t *nodes;
    size_t hop_num;
} process_payment_command_cell_to_node_t;

static void free_payment_command_cell_to_node_job(process_payment_command_cell_to_node_t *job)
{
    tor_free(job->chunk);
    tor_free(job->request);
    tor_free(job->nodes);
    tor_free(job);
}

static workqueue_reply_t process_payment_command_cell_to_node_threadfn(void *state_, void *work_)
{
    if(NULL == state_ || NULL == work_)
        return WQ_RPL_ERROR;
    process_payment_command_cell_to_node_t *job = work_;
    switch(job->request->message_type){
    case 1: // Payment creation request method
        {
            process_payment_request_t request;
            request.payment_request = job->chunk->value;
            request.node_id = job->request->nickname;
            request.routing_node = job->nodes;
            request.call_back_url = s_cb_url_api_command;
            request.status_call_back_url = s_cb_url_api_paymentComplete;
            tp_http_payment(s_url_api_gw_processPayment, &request, job->hop_num);
        }
        break;
    case 4:
        {
            utility_response_t request;
            request.command_id = job->request->command_id;
            request.session_id = job->request->session_id;
            request.node_id = job->request->nickname;
            request.response_body = job->chunk->value;
            tp_http_response(s_url_api_gw_processResponse, &request);
        }
        break;
    default:
        log_warn(LD_BUG,"Payment, unknown request type of message: %i", job->request->message_type);
    }
    free_payment_command_cell_to_node_job(job);
    return WQ_RPL_REPLY;
}

static void process_payment_command_cell_to_node_replyfn(void * arg)
{
    (void)arg;
}

int tp_process_payment_command_cell_to_node_async(const cell_t *cell, circuit_t *circ)
{
    OR_OP_request_t *request = circuit_payment_handle_payment_negotiate(cell);
    if (!request)
        return -1;

    char to_node_key[PAYMENT_HASH_KEY_LEN];
    strcat(strcat(strcpy(to_node_key, request->nickname), "|"), request->session_id);

    payment_chunks_t* origin = tp_store_chunk(request, to_node_key);
    tor_assert(NULL != origin);

    if(request->is_last == 0){
        tor_free(request);
        return 0;
    }

    smartlist_remove(global_chunks_list, origin);

    process_payment_command_cell_to_node_t *job = tor_calloc(1, sizeof(process_payment_command_cell_to_node_t));
    job->chunk = origin;
    job->request = request;
    switch(job->request->message_type){
    case 1: // Payment creation request method
        {
            origin_circuit_t* origin_circuit = TO_ORIGIN_CIRCUIT(circ);
            job->hop_num = circuit_get_num_by_nickname(origin_circuit, job->request->nickname);
            if(job->hop_num == 0) {
                free_payment_command_cell_to_node_job(job);
                job = NULL;
                break;
            }
            job->nodes = tor_calloc(sizeof(routing_node_t), job->hop_num);
            crypt_path_t * next = origin_circuit->cpath;
            for (size_t i = 0; i < job->hop_num - 1; ++i) {
                strlcpy(job->nodes[i].node_id, next->extend_info->nickname, sizeof(job->nodes[i].node_id));
                strlcpy(job->nodes[i].address, next->extend_info->stellar_address, sizeof(job->nodes[i].address));
                next = next->next;
            }
            tp_store_session_context(job->request->session_id, job->request->nickname, circ->n_chan->global_identifier, circ->n_circ_id);
        }
        break;
    case 4:
        break;
    default:
        log_warn(LD_BUG,"Payment, unknown request type of message: %i", job->request->message_type);
        free_payment_command_cell_to_node_job(job);
        job = NULL;
    }
    if (NULL != job) {
        workqueue_entry_t *work = 
            cpuworker_queue_work(WQ_PRI_LOW, process_payment_command_cell_to_node_threadfn, process_payment_command_cell_to_node_replyfn, job);
        tor_assert_nonfatal(NULL != work);
    }
    return 0;
}

static int copy_parameter(tor_http_api_request_t *request, const char *param_name, char *value, size_t max_value_length)
{
    for (size_t i = 0; i < request->param_count; i++) {
        if (!strcasecmp(request->params[i].name, param_name)) {
            strlcpy(value, request->params[i].value, max_value_length);
            return 0;
        }
    }
    return -1;
}

static int tp_rest_api_paymentRoute(const char *url_part, tor_http_api_request_t *request)
{
    if (!request)
        return TOR_HTTP_RESULT_UNKNOWN;
    if(!url_part || !request->url)
        return TOR_HTTP_RESULT_WRONG_PARAMETER;
    tor_route route;
    tp_zero_mem(&route, sizeof(route));
    const size_t url_part_len = strlen(url_part);
    const char *session_id = request->url + url_part_len;
    const char *parameters = strchr(session_id, '?');
    if (!parameters)
        parameters = strchr(session_id, '&');
    if (!parameters)
        strlcpy(route.sessionId, session_id, sizeof(route.sessionId));
    else {
        size_t session_id_len =  parameters - session_id + 1;
        if (session_id_len > SESSION_ID_LEN)
            session_id_len = SESSION_ID_LEN;
        strlcpy(route.sessionId, session_id, session_id_len);
    }

    copy_parameter(request, "exclude_node_id", route.exclude_node_id, sizeof(route.exclude_node_id));
    copy_parameter(request, "exclude_address", route.exclude_address, sizeof(route.exclude_address));
    log_notice(LD_HTTP, "%s request : %s, excluding node with node_id:%s or address:%s", url_part, request->url, route.exclude_node_id, route.exclude_address);

    payment_message_for_http_t message;
    tp_zero_mem(&message, sizeof(message));
    message.url_part = url_part;
    message.msg = &route;
    int rc = tp_send_http_api_request(&message);
    if (rc == TOR_HTTP_RESULT_OK) {
        json_object* json = json_object_new_object();
        json_object* json_route = json_object_new_array();
		for (size_t n = 0; n < route.nodes_len; n++) {
            json_object* json_node = json_object_new_object();
            json_object_object_add(json_node, "NodeId", json_object_new_string(route.nodes[n].node_id));
            json_object_object_add(json_node, "Address", json_object_new_string(route.nodes[n].address));
            json_object_array_add(json_route, json_node);
		}
        json_object_object_add(json, "Route", json_route);
        if (route.call_back_url)
            json_object_object_add(json, "CallbackUrl", json_object_new_string(route.call_back_url));
        if (route.status_call_back_url)
            json_object_object_add(json, "StatusCallbackUrl", json_object_new_string(route.status_call_back_url));
        request->answer_body = tor_strdup(json_object_to_json_string(json));
        json_object_put(json);
        tor_free(route.nodes);
        ship_log(PAYMENT_CALLBACK, request->url, request->body, request->answer_body);
    }
    return rc;
}

static int tp_process_payment_message_for_paymentRoute(payment_message_for_http_t *route_message)
{
    if (!route_message)
        return TOR_HTTP_RESULT_UNKNOWN;

    tor_route *route = (tor_route *)route_message->msg;
    if (!route){
        route_message->done = 1;
        return TOR_HTTP_RESULT_UNKNOWN;
    }
    payment_session_context_t *session_context = get_from_session_context_by_session_id(route->sessionId);

    smartlist_t *list = circuit_get_global_origin_circuit_list();
    if (list == NULL) {
        route_message->done = 1;
        return TOR_HTTP_RESULT_UNKNOWN;
    }

    SMARTLIST_FOREACH_BEGIN(list, origin_circuit_t *, origin_circuit) {
        if (origin_circuit == NULL)
            continue;
        circuit_t * circ = TO_CIRCUIT(origin_circuit);
        if (NULL != session_context) {
            if (circ->n_circ_id != session_context->circuit_id ||
                circ->n_chan->global_identifier != session_context->channel_global_id)
                continue;
        } else {
            log_info(LD_CHANNEL, "get_route try check circuit (%d): %s %s %s",
                circ->n_circ_id,
                circuit_state_to_string(circ->state),
                circuit_purpose_to_string(circ->purpose),
                pathbias_state_to_string(origin_circuit->path_state));

            if (circ->state != CIRCUIT_STATE_OPEN) {
                log_info(LD_CHANNEL, "circuit (%d) - was not opened:",
                        circ->n_circ_id);
                continue;
            }
            if (circ->purpose != CIRCUIT_PURPOSE_C_GENERAL &&
                circ->purpose != CIRCUIT_PURPOSE_C_REND_JOINED) {
                log_info(LD_CHANNEL, "circuit (%d) - purpose was not general or joined rend:",
                        circ->n_circ_id);
                continue;
            }
            // if (origin_circuit->path_state != PATH_STATE_BUILD_SUCCEEDED) {
            //     log_info(LD_CHANNEL, "circuit (%d) - path was not use succeeded:",
            //             circ->n_circ_id);
            //     continue;
            // }
            payment_session_context_t* context = get_from_session_context(circ->n_chan->global_identifier, circ->n_circ_id);
            if (context) {
                continue;
            }
        }

        crypt_path_t *next = origin_circuit->cpath;
        route->nodes_len = circuit_get_length(origin_circuit);
        route->nodes = (routing_node_t *) tor_malloc_(route->nodes_len * sizeof(routing_node_t));
        for (size_t i = 0; i < route->nodes_len; ++i) {
            int skip = 0;
            if (strcmp(route->exclude_address, next->extend_info->stellar_address) == 0) {
                log_notice(LD_HTTP, "tp_get_route: exclude nodes with address: %s, hop: %zu", route->exclude_address, i);
                skip = 1;
            } else if (strcmp(route->exclude_node_id, next->extend_info->nickname) == 0) {
                log_notice(LD_HTTP, "tp_get_route: exclude nodes with nickname: %s, hop: %zu", route->exclude_node_id, i);
                skip = 1;
            } else if (is_invalid_stellar_address(next->extend_info->stellar_address)) {
                log_notice(LD_HTTP, "tp_get_route: Some nodes without stellar address. nodes_count: %zu, hop: %zu", route->nodes_len, i);
                skip = 1;
            }
            if (skip) {
                tor_free_(route->nodes);
                route->nodes = NULL;
                route->nodes_len = 0;
                break;
            }
            strlcpy(route->nodes[i].node_id, next->extend_info->nickname, sizeof(route->nodes[i].node_id));
            strlcpy(route->nodes[i].address, next->extend_info->stellar_address, sizeof(route->nodes[i].address));
            next = next->next;
        }
        if(0 == route->nodes_len)
            continue;
        tp_store_session_context(
            route->sessionId, get_options()->Nickname,
            circ->n_chan->global_identifier,
            circ->n_circ_id);
        break;
    } SMARTLIST_FOREACH_END(origin_circuit);
    route_message->done = 1;
    return TOR_HTTP_RESULT_OK;
}

static int tp_process_payment_message_for_onehop(payment_message_for_http_t *message)
{
    if (!message)
        return TOR_HTTP_RESULT_UNKNOWN;

    tor_api_onehop_t *onehop = (tor_api_onehop_t *)message->msg;
    or_options_t *options = get_options_mutable();
    routerset_t *old_onehoop = options->OneHopNodes;
    routerset_free(old_onehoop);
    if (onehop->reset) {
        options->OneHopNodes = NULL;
        if (onehop->nodes)
            routerset_free(onehop->nodes);
        log_notice(LD_HTTP, "Invalidate all circuits due to OneHopNodes option change");
    } else {
        if (onehop->nodes) {
            routerset_refresh_countries(onehop->nodes);
            options->OneHopNodes = onehop->nodes;
            char *routers_string = routerset_to_string(options->OneHopNodes);
            log_notice(LD_HTTP, "Force exit nodes to: %s. Invalidate all circuits due to option change", routers_string);
            tor_free(routers_string);
        }
    }
    onehop->nodes = NULL;
    circuit_mark_all_unused_circs();
    circuit_mark_all_dirty_circs_as_unusable();
    message->done = 1;
    return TOR_HTTP_RESULT_OK;
}

static int tp_process_payment_message_for_circuit_length(payment_message_for_http_t *message)
{
    if (!message)
        return TOR_HTTP_RESULT_UNKNOWN;

    tor_api_circuit_length_t *msg = (tor_api_circuit_length_t *)message->msg;
    or_options_t *options = get_options_mutable();
    if (msg->circuit_length >= 2) {
        options->CircuitLength = msg->circuit_length;
        log_notice(LD_HTTP, "Invalidate all circuits due to CircuitLength option change");
        circuit_mark_all_unused_circs();
        circuit_mark_all_dirty_circs_as_unusable();
    }
    else {
        log_notice(LD_HTTP, "Invalid CircuitLength option");
    }
    message->done = 1;
    return TOR_HTTP_RESULT_OK;
}

static int tp_process_payment_message_for_versionex(payment_message_for_http_t *message)
{
    if (!message)
        return TOR_HTTP_RESULT_UNKNOWN;
    tor_http_api_request_t *request = (tor_http_api_request_t *)message->msg;

    if (!strcasecmp(request->url, "/api/version")) {
        request->answer_body = tor_strdup(get_version());
        request->answer_plain_text = 1;
    } else if (!strcasecmp(request->url, "/api/versionex")) {
        json_object* json = json_object_new_object();
        json_object_object_add(json, "VersionString", json_object_new_string(get_version()));
        json_object_object_add(json, "NodeId", json_object_new_string(get_options()->Nickname));
        if (get_options()->StellarAddress)
            json_object_object_add(json, "StellarAddress", json_object_new_string(get_options()->StellarAddress));

        if (request->param_count) {
            json_object* params = json_object_new_object();
            for (size_t i = 0; i < request->param_count; i++) {
                json_object_object_add(params, request->params[i].name, json_object_new_string(request->params[i].value));
            }
            json_object_object_add(json, "RequestParams", params);
        }
        request->answer_body = tor_strdup(json_object_to_json_string(json));
        json_object_put(json);
    } else {
        message->done = 1;
        return TOR_HTTP_RESULT_WRONG_URL;
    }
    message->done = 1;
    return TOR_HTTP_RESULT_OK;
}

static int tp_process_payment_message_for_circuits(payment_message_for_http_t *message)
{
    if (!message)
        return TOR_HTTP_RESULT_UNKNOWN;
    tor_http_api_request_t *request = (tor_http_api_request_t *)message->msg;
    json_object* json = json_object_new_object();
    json_object* circuits_array = json_object_new_array();
    smartlist_t *list = circuit_get_global_origin_circuit_list();
    SMARTLIST_FOREACH_BEGIN(list, origin_circuit_t *, origin_circuit) {
        if (origin_circuit == NULL)
            continue;
        circuit_t * circ = TO_CIRCUIT(origin_circuit);
        json_object* circuit_json = json_object_new_object();
        json_object* circuit_path = json_object_new_array();
        crypt_path_t *cpath, *cpath_next = NULL;
        for (cpath = origin_circuit->cpath;
             cpath_next != origin_circuit->cpath;
             cpath = cpath_next) {
            cpath_next = cpath->next;
            json_object* node_json = json_object_new_object();
            json_object_object_add(node_json, "state", json_object_new_string(cpath->state == CPATH_STATE_OPEN ? "opened" : "closed"));
            if(cpath->extend_info != NULL) {
                json_object_object_add(node_json, "nodeid", json_object_new_string(cpath->extend_info->nickname));
                json_object_object_add(node_json, "stellaraddress", json_object_new_string(cpath->extend_info->stellar_address));
                json_object_object_add(node_json, "digest", json_object_new_string(hex_str(cpath->extend_info->identity_digest, DIGEST_LEN)));
                json_object_object_add(node_json, "ed", json_object_new_string(ed25519_fmt(&cpath->extend_info->ed_identity)));
                char *addr_str = tor_addr_to_str_dup(&cpath->extend_info->orports->addr);
                json_object_object_add(node_json, "address", json_object_new_string(addr_str));
                tor_free(addr_str);
            }
            json_object_array_add(circuit_path, node_json);
        }
        json_object_object_add(circuit_json, "path", circuit_path);
        json_object_object_add(circuit_json, "streams", json_object_new_string(origin_circuit->p_streams ? "1" : "0"));
        json_object_object_add(circuit_json, "gid", json_object_new_int64(circ->n_chan->global_identifier));
        json_object_object_add(circuit_json, "cid", json_object_new_int64(circ->n_circ_id));
        json_object_object_add(circuit_json, "state", json_object_new_string(circuit_state_to_string(circ->state)));
        json_object_object_add(circuit_json, "purpose", json_object_new_string(circuit_purpose_to_string(circ->purpose)));
        json_object_object_add(circuit_json, "pathstate", json_object_new_string(pathbias_state_to_string(origin_circuit->path_state)));
        json_object_array_add(circuits_array, circuit_json);
    } SMARTLIST_FOREACH_END(origin_circuit);
    json_object_object_add(json, "origin", circuits_array);
    request->answer_body = tor_strdup(json_object_to_json_string(json));
    json_object_put(json);
    message->done = 1;
    return TOR_HTTP_RESULT_OK;
}

static int tp_process_payment_message_for_sessions(payment_message_for_http_t *message)
{
    if (!message)
        return TOR_HTTP_RESULT_UNKNOWN;
    tor_http_api_request_t *request = (tor_http_api_request_t *)message->msg;
    json_object* json = json_object_new_object();
    json_object* sessions_array = json_object_new_array();
    SMARTLIST_FOREACH_BEGIN(global_payment_session_list, payment_session_context_t *, session) {
        if (session == NULL)
            continue;
        json_object* session_json = json_object_new_object();
        json_object_object_add(session_json, "gid", json_object_new_int64(session->channel_global_id));
        json_object_object_add(session_json, "cid", json_object_new_int64(session->circuit_id));
        json_object_object_add(session_json, "sessionid", json_object_new_string(session->session_id));
        json_object_object_add(session_json, "nodeid", json_object_new_string(session->nickname));
        json_object_array_add(sessions_array, session_json);
    } SMARTLIST_FOREACH_END(session);
    json_object_object_add(json, "sessions", sessions_array);
    request->answer_body = tor_strdup(json_object_to_json_string(json));
    json_object_put(json);
    message->done = 1;
    return TOR_HTTP_RESULT_OK;
}

static int tp_process_payment_message_for_channels(payment_message_for_http_t *message)
{
    if (!message)
        return TOR_HTTP_RESULT_UNKNOWN;
    tor_http_api_request_t *request = (tor_http_api_request_t *)message->msg;
    json_object* json = json_object_new_object();
    json_object* channels_array = json_object_new_array();
    smartlist_t *channels = channel_all_channels();
    SMARTLIST_FOREACH_BEGIN(channels, channel_t *, chan) {
        json_object* chan_json = json_object_new_object();
        json_object_object_add(chan_json, "gid", json_object_new_int64(chan->global_identifier));
        if (chan->describe_peer)
            json_object_object_add(chan_json, "describepeer", json_object_new_string(chan->describe_peer(chan)));
        if (chan->describe_transport)
            json_object_object_add(chan_json, "describetransport", json_object_new_string(chan->describe_transport(chan)));
        json_object_array_add(channels_array, chan_json);
    } SMARTLIST_FOREACH_END(chan);
    json_object_object_add(json, "channels", channels_array);
    request->answer_body = tor_strdup(json_object_to_json_string(json));
    json_object_put(json);
    message->done = 1;
    return TOR_HTTP_RESULT_OK;
}

static const char * get_json_string_value(struct json_object *json, const char *value_name)
{
    struct json_object *obj = NULL;
    if (!json_object_object_get_ex(json, value_name, &obj))
        return NULL;
    return json_object_get_string(obj);
}

static int get_json_int_value(struct json_object *json, const char *value_name)
{
    struct json_object *obj = NULL;
    if (!json_object_object_get_ex(json, value_name, &obj))
        return 0;
    return json_object_get_int(obj);
}

static void tp_process_payment_for_sending(payment_message_for_sending_t* message)
{
    payment_session_context_t *session_context = get_from_session_context_by_session_id(message->sessionId);
    if(session_context != NULL) {
        /* Get the channel */
        channel_t *chan = channel_find_by_global_id(session_context->channel_global_id);
        /* Get the circuit */
        circuit_t *circ = circuit_get_by_circid_channel_even_if_marked(session_context->circuit_id, chan);
        if(message->message == NULL) { // from tp_process_payment_message_for_paymentcomplete
            OR_OP_request_t input;
            memset(&input, 0, sizeof(input));

            memcpy(input.session_id, message->sessionId, sizeof(input.session_id));
            input.session_id_length = strlen(message->sessionId);

            input.command = RELAY_COMMAND_PAYMENT_COMMAND_TO_NODE;
            input.is_last = 1;
            input.message_type = 100;
            input.command_type = 0;
            input.version = 0;
            input.messageTotalLength = 0;
            input.messageLength = 0;
            input.command_id_length = 0;
            input.nicknameLength = 0;
            if (circ != NULL) {
                origin_circuit_t *origin_circuit = TO_ORIGIN_CIRCUIT(circ);
                int length = circuit_get_length(origin_circuit);
                for (int i = 1; i < length + 1; ++i) {
                    circuit_payment_send_OP(circ, i, &input);
                }
                remove_from_session_context(session_context);
            }
        } else {
            if (circ != NULL) {
                if (CIRCUIT_IS_ORIGIN(circ)) {
                    origin_circuit_t *origin_circuit = TO_ORIGIN_CIRCUIT(circ);
                    int hop_num = circuit_get_num_by_nickname(origin_circuit, message->nodeId);
                    circuit_payment_send_OP(circ, hop_num, message->message);
                } else {
                    circuit_payment_send_OR(circ, message->message);
                    if (message->message->message_type == 4) {
                        tp_circuitmux_reset_limits(circ);
                    }
                }
            }
        }
    }
    if (NULL != message->message)
        tor_free_(message->message);
}

static int tp_process_payment_message_for_command(payment_message_for_http_t *message)
{
    if (!message)
        return TOR_HTTP_RESULT_UNKNOWN;
    tor_command* command = (tor_command* ) message->msg;
    if (NULL == command) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_command: invalid arguments");
        return TOR_HTTP_RESULT_UNKNOWN;
    }

    if (NULL == command->nodeId ||
        NULL == command->commandType ||
        NULL == command->commandId ||
        NULL == command->sessionId ||
        NULL == command->commandBody) {
            log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_command: invalid arguments");
            return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }
    
    const size_t command_type_length = strlen(command->commandType);
    const int command_type = atoi(command->commandType);
    const size_t nicknameLength = strlen(command->nodeId);
    const size_t session_id_length = strlen(command->sessionId);
    const size_t command_id_length = strlen(command->commandId);

    if(0 == command_type_length || 0 > command_type) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_command: argument 'commandType' invalid string length: %zu, value:%s", command_type_length, command->commandType);
        return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }
    if(nicknameLength > USER_NAME_LEN || 0 == nicknameLength) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_command: argument 'nodeId' invalid string length: %zu, value:%s", nicknameLength, command->nodeId);
        return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }
    if(session_id_length > SESSION_ID_LEN || 0 ==session_id_length) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_command: argument 'sessionId' invalid string length: %zu, value:%s", session_id_length, command->sessionId);
        return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }
    if(command_id_length > COMMAND_ID_LEN || 0 ==command_id_length) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_command: argument 'commandId' invalid string length: %zu, value:%s", command_id_length, command->commandId);
        return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }
    const size_t body_len = strlen(command->commandBody);
    const size_t part_size = body_len / CHUNK_SIZE;

    for(size_t g = 0 ; g <= part_size ;g++) {
        payment_message_for_sending_t tp_message;
        tp_zero_mem(&tp_message, sizeof(tp_message));
        strlcpy(tp_message.nodeId, command->nodeId, sizeof(tp_message.nodeId));
        strlcpy(tp_message.sessionId, command->sessionId, sizeof(tp_message.sessionId));

        OR_OP_request_t *input = payment_payload_new();
        if (NULL == input)
            break;
        tp_message.message = input;
        input->command = RELAY_COMMAND_PAYMENT_COMMAND_TO_NODE;
        input->command_type = command_type;

        strlcpy(input->nickname, command->nodeId, sizeof(input->nickname));
        input->nicknameLength = nicknameLength;
        strlcpy(input->session_id, command->sessionId, sizeof(input->session_id));
        input->session_id_length = session_id_length;
        strlcpy(input->command_id, command->commandId, sizeof(input->command_id));
        input->command_id_length = command_id_length;
    
        size_t chunck_real_size = 0;
        const char * buf_ptr = tp_get_buffer_part_number(command->commandBody, body_len, CHUNK_SIZE, g, &chunck_real_size);
        strncpy(input->message, buf_ptr, chunck_real_size);
        input->message[chunck_real_size] = '\0';
        input->messageLength = chunck_real_size;
        input->is_last = (g < part_size) ? 0 : 1;
        tp_process_payment_for_sending(&tp_message);
    }
    return TOR_HTTP_RESULT_OK;
}

static int tp_rest_api_command(const char *url_part, tor_http_api_request_t *request)
{
    if (!request)
        return TOR_HTTP_RESULT_UNKNOWN;
    if (!request->body)
        return TOR_HTTP_RESULT_WRONG_BODY;
    log_notice(LD_HTTP, "%s request: %s", url_part, request->body);
    struct json_object *json = NULL;
    enum json_tokener_error jerr = json_tokener_success;
    json = json_tokener_parse_verbose(request->body, &jerr);
    if (jerr != json_tokener_success) {
        log_err(LD_HTTP, "Can't parse json object (reason:%s) from: %s", json_tokener_error_desc(jerr), request->body);
        return TOR_HTTP_RESULT_WRONG_JSON;
    }
    tor_command cmd;
    tp_zero_mem(&cmd, sizeof(cmd));
    cmd.commandBody = get_json_string_value(json, "CommandBody");
    cmd.commandId = get_json_string_value(json, "CommandId");
    cmd.commandType = get_json_string_value(json, "CommandType");
    cmd.nodeId = get_json_string_value(json, "NodeId");
    cmd.sessionId = get_json_string_value(json, "SessionId");
    cmd.json_body = request->body;

    payment_message_for_http_t message;
    tp_zero_mem(&message, sizeof(message));
    message.url_part = url_part;
    message.msg = &cmd;
    int rc = tp_send_http_api_request(&message);
    if (json)
        json_object_put(json);
    return rc;
}

static int tp_process_payment_message_for_response(payment_message_for_http_t *message)
{
    if (!message)
        return TOR_HTTP_RESULT_UNKNOWN;
    tor_command_replay* command = (tor_command_replay* ) message->msg;
    if (!command) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_response: invalid arguments");
        return TOR_HTTP_RESULT_UNKNOWN;
    }

    if (NULL == command->nodeId ||
        NULL == command->sessionId ||
        NULL == command->commandId ||
        NULL == command->commandResponse ||
        NULL == command->commandType) {
            log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_response: invalid arguments");
            return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }

    const size_t nicknameLength = strlen(command->nodeId);
    const size_t session_id_length = strlen(command->sessionId);
    const size_t command_id_length = strlen(command->commandId);
    const size_t command_type_length = strlen(command->commandType);
    const int command_type = atoi(command->commandType);

    if(nicknameLength > USER_NAME_LEN || 0 == nicknameLength) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_response: argument 'nodeId' invalid string length: %zu, value:%s", nicknameLength, command->nodeId);
        return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }
    if(session_id_length > SESSION_ID_LEN || 0 ==session_id_length) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_response: argument 'sessionId' invalid string length: %zu, value:%s", session_id_length, command->sessionId);
        return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }
    if(command_id_length > COMMAND_ID_LEN || 0 ==command_id_length) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_response: argument 'commandId' invalid string length: %zu, value:%s", command_id_length, command->commandId);
        return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }
    if(0 == command_type_length ) {
        log_debug(LD_PROTOCOL, "tp_process_payment_message_for_response: argument 'commandType' empty string");
    }
    if (0 > command_type) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_response: argument 'commandType' invalid string length: %zu, value:%s", command_type_length, command->commandType);
        return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }

    const size_t body_len = strlen(command->commandResponse);
    const size_t part_size = body_len / CHUNK_SIZE;
    for(size_t g = 0 ; g <= part_size ;g++) {
        payment_message_for_sending_t tp_message;
        tp_zero_mem(&tp_message, sizeof(tp_message));
        strlcpy(tp_message.sessionId, command->sessionId, sizeof(tp_message.sessionId));
        strlcpy(tp_message.nodeId, command->nodeId, sizeof(tp_message.nodeId));
    
        OR_OP_request_t *input = payment_payload_new();
        if (NULL == input)
            break;
        tp_message.message = input;
        input->command = RELAY_COMMAND_PAYMENT_COMMAND_TO_NODE;
        input->command_type = command_type;
        input->message_type = 4;

        strlcpy(input->nickname, command->nodeId, sizeof(input->nickname));
        input->nicknameLength = nicknameLength;
        strlcpy(input->session_id, command->sessionId, sizeof(input->session_id));
        input->session_id_length = session_id_length;
        strlcpy(input->command_id, command->commandId, sizeof(input->command_id));
        input->command_id_length = command_id_length;

        size_t chunck_real_size = 0;
        const char * buf_ptr = tp_get_buffer_part_number(command->commandResponse, body_len, CHUNK_SIZE, g, &chunck_real_size);
        strncpy(input->message, buf_ptr, chunck_real_size);
        input->message[chunck_real_size] = '\0';
        input->messageLength = chunck_real_size;
        input->is_last = (g < part_size) ? 0 : 1;
    
        tp_process_payment_for_sending(&tp_message);
    }
    return TOR_HTTP_RESULT_OK;
}

static int tp_rest_api_response(const char *url_part, tor_http_api_request_t *request)
{
    if (!request)
        return TOR_HTTP_RESULT_UNKNOWN;
    if (!request->body)
        return TOR_HTTP_RESULT_WRONG_BODY;
    log_notice(LD_HTTP, "%s request: %s", url_part, request->body);
    struct json_object *json = NULL;
    enum json_tokener_error jerr = json_tokener_success;
    json = json_tokener_parse_verbose(request->body, &jerr);
    if (jerr != json_tokener_success) {
        log_err(LD_HTTP, "Can't parse json object (reason:%s) from: %s", json_tokener_error_desc(jerr), request->body);
        return TOR_HTTP_RESULT_WRONG_JSON;
    }
    tor_command_replay cmd;
    tp_zero_mem(&cmd, sizeof(cmd));
    cmd.commandResponse = get_json_string_value(json, "CommandResponse");
    cmd.commandId = get_json_string_value(json, "CommandId");
    cmd.commandType = get_json_string_value(json, "CommandType");
    cmd.nodeId = get_json_string_value(json, "NodeId");
    cmd.sessionId = get_json_string_value(json, "SessionId");
    cmd.json_body = request->body;

    payment_message_for_http_t message;
    tp_zero_mem(&message, sizeof(message));
    message.url_part = url_part;
    message.msg = &cmd;
    int rc = tp_send_http_api_request(&message);
    if (json)
        json_object_put(json);
    return rc;
}

static int tp_process_payment_message_for_paymentComplete(payment_message_for_http_t *message)
{
    payment_completed* command = (payment_completed* ) message->msg;
    if (NULL == command) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_paymentcomplete: invalid arguments");
        return TOR_HTTP_RESULT_UNKNOWN;
    }

    if (NULL == command->sessionId /*|| 0 > command->status*/) {
            log_notice(LD_PROTOCOL | LD_BUG, "tp_process_payment_message_for_paymentcomplete: invalid sessionId arguments");
            return TOR_HTTP_RESULT_WRONG_PARAMETER;
    }

    payment_message_for_sending_t tp_message;
    tp_zero_mem(&tp_message, sizeof(tp_message));
    strcpy(tp_message.nodeId, "-1");
    strlcpy(tp_message.sessionId, command->sessionId, sizeof(tp_message.sessionId));
    tp_process_payment_for_sending(&tp_message);
    return TOR_HTTP_RESULT_OK;
}

static int tp_rest_api_paymentComplete(const char *url_part, tor_http_api_request_t *request)
{
    if(!request)
        return TOR_HTTP_RESULT_UNKNOWN;
    if (!request->body)
        return TOR_HTTP_RESULT_WRONG_BODY;
    log_notice(LD_HTTP, "%s request: %s", url_part, request->body);
    struct json_object *json = NULL;
    enum json_tokener_error jerr = json_tokener_success;
    json = json_tokener_parse_verbose(request->body, &jerr);
    if (jerr != json_tokener_success) {
        log_err(LD_HTTP, "Can't parse json object (reason:%s) from: %s", json_tokener_error_desc(jerr), request->body);
        return TOR_HTTP_RESULT_WRONG_JSON;
    }
    payment_completed cmd;
    tp_zero_mem(&cmd, sizeof(cmd));
    cmd.status = get_json_int_value(json, "Status");
    cmd.sessionId = get_json_string_value(json, "SessionId");
    cmd.json_body = request->body;

    payment_message_for_http_t message;
    tp_zero_mem(&message, sizeof(message));
    message.url_part = url_part;
    message.msg = &cmd;
    int rc = tp_send_http_api_request(&message);
    if (json)
        json_object_put(json);
    return rc;
}

static const payment_message_for_http_handler_t global_http_api_handlers[] = {
    { "POST", "/api/onehop",
        tp_process_payment_message_for_onehop,
        tp_rest_api_onehop },
    { "POST", "/api/circuit_length",
        tp_process_payment_message_for_circuit_length,
        tp_rest_api_circuit_length },
    { "GET", "/api/version",
        tp_process_payment_message_for_versionex,
        tp_rest_api_direct },
    { "GET", "/api/circuits",
        tp_process_payment_message_for_circuits,
        tp_rest_api_direct },
    { "GET", "/api/sessions",
        tp_process_payment_message_for_sessions,
        tp_rest_api_direct },
    { "GET", "/api/channels",
        tp_process_payment_message_for_channels,
        tp_rest_api_direct },
    { "GET", "/api/paymentRoute/",
        tp_process_payment_message_for_paymentRoute,
        tp_rest_api_paymentRoute },
    { "POST", "/api/command",
        tp_process_payment_message_for_command,
        tp_rest_api_command },
    { "POST", "/api/response",
        tp_process_payment_message_for_response,
        tp_rest_api_response },
    { "POST", "/api/paymentComplete",
        tp_process_payment_message_for_paymentComplete,
        tp_rest_api_paymentComplete }
};

static int tp_rest_handler(tor_http_api_request_t *request)
{
    if (!request || !request->method)
        return TOR_HTTP_RESULT_UNKNOWN;
    if (!request->url)
        return TOR_HTTP_RESULT_WRONG_URL;
    for (size_t i = 0; i < NELEMS(global_http_api_handlers); i++) {
        if (strcasecmp(request->method, global_http_api_handlers[i].method))
            continue;
        if (strncasecmp(request->url, global_http_api_handlers[i].url, strlen(global_http_api_handlers[i].url)))
            continue;
        if (!request->release)
            request->release = tor_free_;
        return global_http_api_handlers[i].request_fn(global_http_api_handlers[i].url, request);
    }
    return TOR_HTTP_RESULT_WRONG_URL;
}

static void tp_timer_callback(periodic_timer_t *timer, void *data)
{
    (void) timer; (void) data;

    tp_circuitmux_refresh_limited_circuits();

    tor_mutex_acquire(&global_payment_mutex);
    int done = 0;
    SMARTLIST_FOREACH_BEGIN(global_payment_api_messsages, payment_message_for_http_t*, http_api_message) {
        if (http_api_message) {
            for ( size_t i = 0; i < NELEMS(global_http_api_handlers); i++ ) {
                if (!strcasecmp(global_http_api_handlers[i].url, http_api_message->url_part)) {
                    http_api_message->result = global_http_api_handlers[i].handler_fn(http_api_message);
                    break;
                }
            }
            http_api_message->done = 1;
            done = 1;
        }
    } SMARTLIST_FOREACH_END(http_api_message);
    smartlist_clear(global_payment_api_messsages);
    tor_mutex_release(&global_payment_mutex);

    if (done)
        tor_cond_signal_all(&global_payment_cond);

    tp_scan_sessions();// TODO: move into slower timer callback
}
