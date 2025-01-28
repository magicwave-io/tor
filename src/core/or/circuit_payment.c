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
#include "core/or/crypt_path_st.h"
#include "core/or/circuit_st.h"
#include "core/or/origin_circuit_st.h"
#include "feature/nodelist/routerstatus_st.h"
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
#define CHECK_EXPRESSION(expsn, label)                           \
  do {                                                           \
    if ((!(expsn)) OR_DEADCODE_DUMMY) {                \
      goto label;                                                \
    }                                                            \
  } while (0)

#define CHUNK_SIZE (MAX_MESSAGE_LEN - 1)

static smartlist_t *global_payment_session_list = NULL;
static smartlist_t *global_chunks_list = NULL;

static smartlist_t *global_payment_messsages = NULL;
static pthread_rwlock_t global_payment_messsages_rwlock = PTHREAD_RWLOCK_INITIALIZER;

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

// HTTP CALLBACK
static void tp_get_route(const char* sessionId, tor_route *route)
{
    if (NULL == sessionId || NULL == route) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_get_route: invalid arguments");
        return;
    }

    char nodes_str[10000];
    const size_t resp_size = sizeof(nodes_str) - 1;
    tp_zero_mem(nodes_str, sizeof(nodes_str));

    route->call_back_url = s_cb_url_api_command;
    route->status_call_back_url = s_cb_url_api_paymentComplete;
    route->nodes = NULL;

    smartlist_t *list = circuit_get_global_origin_circuit_list(); // there are data races!!!!
    if (list != NULL) {
        SMARTLIST_FOREACH_BEGIN(list, origin_circuit_t *, origin_circuit) {
            if (origin_circuit != NULL) {
                if (TO_CIRCUIT(origin_circuit)->state != CIRCUIT_STATE_OPEN) {
                    log_warn(LD_CHANNEL, "circuit (%d) - was not opened:",
                            TO_CIRCUIT(origin_circuit)->n_circ_id);
                    continue;
                }
                if (TO_CIRCUIT(origin_circuit)->purpose != CIRCUIT_PURPOSE_C_GENERAL) {
                    log_warn(LD_CHANNEL, "circuit (%d) - purpose was not general:",
                            TO_CIRCUIT(origin_circuit)->n_circ_id);
                    continue;
                }
                if (origin_circuit->path_state != PATH_STATE_BUILD_SUCCEEDED) {
                    log_warn(LD_CHANNEL, "circuit (%d) - path was not use succeeded:",
                            TO_CIRCUIT(origin_circuit)->n_circ_id);
                    continue;
                }

                crypt_path_t *next = origin_circuit->cpath;
                route->nodes_len = circuit_get_length(origin_circuit);
                route->nodes = (rest_node_t *) tor_malloc_(route->nodes_len * sizeof(rest_node_t));
                for (size_t i = 0; i < route->nodes_len; ++i) {
                    if (is_invalid_stellar_address(next->extend_info->stellar_address)) {
                        log_notice(LD_PROTOCOL | LD_BUG, "tp_get_route: Some nodes without stellar address. nodes_count: %zu, failed_num: %zu", route->nodes_len, i);
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

                tp_store_session_context(sessionId, "nickname",
                    TO_CIRCUIT(origin_circuit)->n_chan->global_identifier,
                    TO_CIRCUIT(origin_circuit)->n_circ_id);

                for (size_t i = 0; i < route->nodes_len; i++) {
                    strncat(nodes_str, (i > 0) ? ",[" : "[", resp_size);
                    strncat(nodes_str, route->nodes[i].node_id, resp_size);
                    strncat(nodes_str, ":", resp_size);
                    strncat(nodes_str, route->nodes[i].address, resp_size);
                    strncat(nodes_str, "]", resp_size);
                }
                break;
            }
        } SMARTLIST_FOREACH_END(origin_circuit);
    }
    char url[100];
    strcat(strcat(strcpy(url, "/api/paymentRoute"), "/"), sessionId);
    ship_log(PAYMENT_CALLBACK, url, "", nodes_str);
}

// HTTP CALLBACK
static int tp_payment_chain_completed(payment_completed* command)
{
    if (NULL == command) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_payment_chain_completed: invalid arguments");
        return -1;
    }

    ship_log(PAYMENT_CALLBACK, "/api/paymentComplete",  command->json_body ? command->json_body : "(NULL)", "");

    if (NULL == command->sessionId /*|| 0 > command->status*/) {
            log_notice(LD_PROTOCOL | LD_BUG, "tp_payment_chain_completed: invalid sessionId arguments");
            return -2;
    }

    payment_message_for_sending_t* message = tor_calloc_(1, sizeof(payment_message_for_sending_t));
    strcpy(message->nodeId, "-1");
    strlcpy(message->sessionId, command->sessionId, sizeof(message->sessionId));
    message->message = NULL;
    pthread_rwlock_wrlock(&global_payment_messsages_rwlock);
    smartlist_add(global_payment_messsages, message);
    pthread_rwlock_unlock(&global_payment_messsages_rwlock);
    return 0;
}

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

// HTTP CALLBACK
static int tp_process_command(tor_command* command)
{
    if (NULL == command) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command: invalid arguments");
        return -1;
    }

    ship_log(PAYMENT_CALLBACK, "/api/command", command->json_body ? command->json_body : "(NULL)", "");

    if (NULL == command->nodeId ||
        NULL == command->commandType ||
        NULL == command->commandId ||
        NULL == command->sessionId ||
        NULL == command->commandBody) {
            log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command: invalid arguments");
            return -2;
    }
    
    const size_t command_type_length = strlen(command->commandType);
    const int command_type = atoi(command->commandType);
    const size_t nicknameLength = strlen(command->nodeId);
    const size_t session_id_length = strlen(command->sessionId);
    const size_t command_id_length = strlen(command->commandId);

    if(0 == command_type_length || 0 > command_type) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command: argument 'commandType' invalid string length: %zu, value:%s", command_type_length, command->commandType);
        return -3;
    }
    if(nicknameLength > USER_NAME_LEN || 0 == nicknameLength) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command: argument 'nodeId' invalid string length: %zu, value:%s", nicknameLength, command->nodeId);
        return -3;
    }
    if(session_id_length > SESSION_ID_LEN || 0 ==session_id_length) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command: argument 'sessionId' invalid string length: %zu, value:%s", session_id_length, command->sessionId);
        return -3;
    }
    if(command_id_length > COMMAND_ID_LEN || 0 ==command_id_length) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command: argument 'commandId' invalid string length: %zu, value:%s", command_id_length, command->commandId);
        return -3;
    }
    const size_t body_len = strlen(command->commandBody);
    const size_t part_size = body_len / CHUNK_SIZE;

    pthread_rwlock_wrlock(&global_payment_messsages_rwlock);

    for(size_t g = 0 ; g <= part_size ;g++) {
        payment_message_for_sending_t* message = tor_calloc_(1, sizeof(payment_message_for_sending_t));
        strlcpy(message->nodeId, command->nodeId, sizeof(message->nodeId));
        strlcpy(message->sessionId, command->sessionId, sizeof(message->sessionId));

        OR_OP_request_t *input = payment_payload_new();
        message->message = input;
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

        smartlist_add(global_payment_messsages, message);
    }
    pthread_rwlock_unlock(&global_payment_messsages_rwlock);
    return 0;
}

// HTTP CALLBACK
static int tp_process_command_replay(tor_command_replay* command)
{
    if (NULL == command) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command_replay: invalid arguments");
        return -1;
    }

    ship_log(PAYMENT_CALLBACK, "/api/response", command->json_body ? command->json_body : "(NULL)", "");

    if (NULL == command->nodeId ||
        NULL == command->sessionId ||
        NULL == command->commandId ||
        NULL == command->commandResponse ||
        NULL == command->commandType) {
            log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command_replay: invalid arguments");
            return -2;
    }

    const size_t nicknameLength = strlen(command->nodeId);
    const size_t session_id_length = strlen(command->sessionId);
    const size_t command_id_length = strlen(command->commandId);
    const size_t command_type_length = strlen(command->commandType);
    const int command_type = atoi(command->commandType);

    if(nicknameLength > USER_NAME_LEN || 0 == nicknameLength) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command_replay: argument 'nodeId' invalid string length: %zu, value:%s", nicknameLength, command->nodeId);
        return -3;
    }
    if(session_id_length > SESSION_ID_LEN || 0 ==session_id_length) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command_replay: argument 'sessionId' invalid string length: %zu, value:%s", session_id_length, command->sessionId);
        return -3;
    }
    if(command_id_length > COMMAND_ID_LEN || 0 ==command_id_length) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command_replay: argument 'commandId' invalid string length: %zu, value:%s", command_id_length, command->commandId);
        return -3;
    }
    if(0 == command_type_length ) {
        log_debug(LD_PROTOCOL, "tp_process_command_replay: argument 'commandType' empty string");
    }
    if (0 > command_type) {
        log_notice(LD_PROTOCOL | LD_BUG, "tp_process_command_replay: argument 'commandType' invalid string length: %zu, value:%s", command_type_length, command->commandType);
        return -3;
    }

    const size_t body_len = strlen(command->commandResponse);
    const size_t part_size = body_len / CHUNK_SIZE;

    pthread_rwlock_wrlock(&global_payment_messsages_rwlock);
    for(size_t g = 0 ; g <= part_size ;g++) {
        payment_message_for_sending_t* message = tor_calloc_(1, sizeof(payment_message_for_sending_t));
        strlcpy(message->sessionId, command->sessionId, sizeof(message->sessionId));
        strlcpy(message->nodeId, command->nodeId, sizeof(message->nodeId));
    
        OR_OP_request_t *input = payment_payload_new();
        message->message = input;
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
    
        smartlist_add(global_payment_messsages, message);
    }
    pthread_rwlock_unlock(&global_payment_messsages_rwlock);
    return 0;
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
        if (!isupper(c) || !isdigit(c))
            return -4;
    }
    return 0;
}

int tp_validate_stellar_address(char *addr)
{
    if (!addr)
        return -1;
    const int rc = is_invalid_stellar_address(addr);
    if (rc)
        tp_zero_mem(addr, STELLAR_ADDRESS_LEN);
    return rc;
}

int tp_copy_and_check_stellar_address(crypt_path_t *cpath, const char *src)
{
    strlcpy(cpath->extend_info->stellar_address, src, STELLAR_ADDRESS_LEN);
    return tp_validate_stellar_address(cpath->extend_info->stellar_address);
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
    log_notice(LD_GENERAL,"Payment REST_LOG:\n%s", message);
}

void tp_init_lists(void)
{
    global_payment_messsages = smartlist_new();
    global_payment_session_list = smartlist_new();
    global_chunks_list = smartlist_new();
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
    cpu_init();

    const int ppcb_port = get_options()->PPChannelCallbackPort;
    if ( ppcb_port != -1 ) {
        const char *server_version_string = get_version();
        runServer(ppcb_port, tp_get_route, tp_process_command, tp_process_command_replay, tp_payment_chain_completed, tp_rest_log, server_version_string);
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
    CHECK_EXPRESSION(obj->version == PAYMENT_MSG_VERSION, fail);
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
    CHECK_EXPRESSION((obj->session_id_length > 0) && (obj->session_id_length < SESSION_ID_LEN), fail);

    /* Parse char name[len] */
    CHECK_REMAINING(SESSION_ID_LEN, fail);
    memcpy(obj->session_id, ptr, SESSION_ID_LEN);
    remaining -= SESSION_ID_LEN; ptr += SESSION_ID_LEN;
    CHECK_EXPRESSION((obj->session_id_length == strnlen(obj->session_id, sizeof(obj->session_id))), fail);

    CHECK_REMAINING(2, truncated);
    obj->command_id_length = (trunnel_get_uint16(ptr));
    remaining -= 2; ptr += 2;
    CHECK_EXPRESSION((obj->command_id_length >= 0) && (obj->command_id_length < COMMAND_ID_LEN), fail); // TODO: >=0 ?????

    /* Parse char name[len] */
    CHECK_REMAINING(COMMAND_ID_LEN, fail);
    memcpy(obj->command_id, ptr, COMMAND_ID_LEN);
    remaining -= COMMAND_ID_LEN; ptr += COMMAND_ID_LEN;
    CHECK_EXPRESSION((obj->command_id_length == strnlen(obj->command_id, sizeof(obj->command_id))), fail);

    CHECK_REMAINING(2, truncated);
    obj->nicknameLength = (trunnel_get_uint16(ptr));
    remaining -= 2; ptr += 2;
    CHECK_EXPRESSION((obj->nicknameLength > 0) && (obj->nicknameLength < USER_NAME_LEN), fail);

    /* Parse char name[len] */
    CHECK_REMAINING(USER_NAME_LEN, fail);
    memcpy(obj->nickname, ptr, USER_NAME_LEN);
    remaining -= USER_NAME_LEN; ptr += USER_NAME_LEN;
    CHECK_EXPRESSION((obj->nicknameLength == strnlen(obj->nickname, sizeof(obj->nickname))), fail);

    CHECK_REMAINING(2, truncated);
    obj->messageLength = (trunnel_get_uint16(ptr));
    remaining -= 2; ptr += 2;
    CHECK_EXPRESSION((obj->messageLength > 0) && (obj->messageLength < MAX_MESSAGE_LEN), fail);

    /* Parse char name[len] */
    CHECK_REMAINING(MAX_MESSAGE_LEN, fail);
    memcpy(obj->message, ptr, MAX_MESSAGE_LEN);
    remaining -= MAX_MESSAGE_LEN; ptr += MAX_MESSAGE_LEN;
    CHECK_EXPRESSION((obj->messageLength == strnlen(obj->message, sizeof(obj->message))), fail);

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
    payment_session_context_t *origin = NULL;

    SMARTLIST_FOREACH_BEGIN(global_payment_session_list, payment_session_context_t *, element) {
        if (strcmp(element->session_id, session) == 0) {
            origin = element;
            break;
        }
    } SMARTLIST_FOREACH_END(element);

    if (origin == NULL) {
        payment_session_context_t *ent = (payment_session_context_t *) tor_malloc_zero_(
                sizeof(payment_session_context_t));
        strcpy(ent->session_id, session);
        strcpy(ent->nickname, nickname);
        ent->channel_global_id = channel_global_id;
        ent->circuit_id = circuit_id;
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

void remove_from_session_context(payment_session_context_t* element)
{
    smartlist_remove(global_payment_session_list, element);
    tor_free_(element);
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
    circuit_t *circ;
    char *response;
    struct json_object *json;
    char *session;
} send_payment_request_to_client_t;

void free_send_payment_request_to_client_job(send_payment_request_to_client_t *job)
{
    if (job) {
        tor_free(job->response);
        tor_free(job->session);
        job->circ = NULL;
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

static void send_payment_request_to_client_replyfn(void * work_)
{
    send_payment_request_to_client_t *job = work_;
    if (NULL == job ||
        NULL == job->session ||
        NULL == job->response) {
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
        circuit_payment_send_OR(job->circ, &input);
    }
    free_send_payment_request_to_client_job(job);
}

void tp_send_payment_request_to_client_async(circuit_t *circ, int message_number)
{
    (void)message_number;
    circ->total_package_received = 0;
    circ->total_package_sent = 0;

    tp_update_circ_counters(TO_OR_CIRCUIT(circ));

    send_payment_request_to_client_t *job = tor_calloc(1, sizeof(send_payment_request_to_client_t));
    tor_assert(NULL != job);
    job->circ = circ;
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
    circuit_t *circ;
} process_payment_cell_t;

static workqueue_reply_t process_payment_cell_threadfn(void *state_, void *work_)
{
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
        tp_store_session_context(payment_request_payload->session_id,
            payment_request_payload->nickname,
            TO_OR_CIRCUIT(circ)->p_chan->global_identifier,
            TO_OR_CIRCUIT(circ)->p_circ_id);

        process_payment_cell_t *job = tor_calloc(1, sizeof(process_payment_cell_t));
        job->chunk = origin;
        job->circ = circ;
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
    circuit_t *circ;
    routing_node_t *nodes;
    size_t hop_num;
} process_payment_command_cell_to_node_t;

void free_payment_command_cell_to_node_job(process_payment_command_cell_to_node_t *job)
{
    tor_free(job->chunk);
    tor_free(job->request);
    tor_free(job->nodes);
    tor_free(job);
}

static workqueue_reply_t process_payment_command_cell_to_node_threadfn(void *state_, void *work_)
{
  (void)state_;
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
    job->circ = circ;
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
            for (int i = 0; i < job->hop_num - 1; ++i) {
                job->nodes[i].node_id = next->extend_info->nickname;
                job->nodes[i].address = next->extend_info->stellar_address;
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

static void tp_timer_callback(periodic_timer_t *timer, void *data)
{
    (void) timer; (void) data;

    tp_circuitmux_refresh_limited_circuits();

    pthread_rwlock_wrlock(&global_payment_messsages_rwlock);
    SMARTLIST_FOREACH_BEGIN(global_payment_messsages, payment_message_for_sending_t*, message) {
        payment_session_context_t *session_context = get_from_session_context_by_session_id(message->sessionId);
        if(session_context != NULL) {
            /* Get the channel */
            channel_t *chan = channel_find_by_global_id(session_context->channel_global_id);
            /* Get the circuit */
            circuit_t *circ = circuit_get_by_circid_channel_even_if_marked(session_context->circuit_id, chan);
            if(message->message == NULL) { // from tp_payment_chain_completed
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
        if (NULL != message->message){
            tor_free_(message->message);
        }
        tor_free_(message);
    } SMARTLIST_FOREACH_END(message);

    smartlist_clear(global_payment_messsages);
    pthread_rwlock_unlock(&global_payment_messsages_rwlock);
}
