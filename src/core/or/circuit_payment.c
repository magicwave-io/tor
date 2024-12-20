/* Copyright (c) 2017 The Tor Project, Inc. */
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

#define TRUNNEL_SET_ERROR_CODE(obj) \
  do {                              \
    (obj)->trunnel_error_code_ = 1; \
  } while (0)

#if defined(__COVERITY__) || defined(__clang_analyzer__)
/* If we're running a static analysis tool, we don't want it to complain
 * that some of our remaining-bytes checks are dead-code. */
int sendmecell_deadcode_dummy__ = 0;
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

static smartlist_t *global_payment_session_list = NULL;
static smartlist_t *global_payment_info_list = NULL;

//sem_t mutex;
//
//void init_semaphore(){
//    sem_init(&mutex, 0, 1);
//}
//
//void wait_semaphore(){
//    sem_wait(&mutex);
//}
//
//void destroy_semaphore(){
//    sem_destroy(&mutex);
//}
//
//void post_semaphore(){
//    sem_post(&mutex);
//}

error_t circuit_payment_send_OP(circuit_t *circ, uint8_t target_hopnum, OR_OP_request_t* input)
{


    cell_t cell;
    ssize_t len;

    origin_circuit_t* orig_circ = TO_ORIGIN_CIRCUIT(circ);

    if (!circuit_payment_get_nth_node(orig_circ, target_hopnum)) {
        return 0;
    }

    memset(&cell, 0, sizeof(cell_t));

    cell.command = CELL_RELAY;

    if ((len = circuit_payment_negotiate_encode(cell.payload, CELL_PAYLOAD_SIZE, input)) < 0)
        return 0;

    error_t res = circuit_payment_send_command_to_hop(orig_circ, target_hopnum,
                                               RELAY_COMMAND_PAYMENT_COMMAND_TO_NODE,
                                               cell.payload, len);


    return res;
}

error_t circuit_payment_send_OR(circuit_t *circ, OR_OP_request_t* input)
{

    uint8_t payload[RELAY_PAYLOAD_SIZE];
    ssize_t len;

    if (CIRCUIT_IS_ORIGIN(circ)) {
        return 0;
    }

    if ((len = circuit_payment_negotiate_encode(payload, CELL_PAYLOAD_SIZE,  input)) < 0)
        return 0;

    error_t res = circuit_payment_send_command_to_origin(circ,
                                                  RELAY_COMMAND_PAYMENT_COMMAND_TO_ORIGIN,
                                                  payload, len);


    return res;
}

const node_t* circuit_payment_get_nth_node(origin_circuit_t *circ, int hop) {
    crypt_path_t *iter = circuit_get_cpath_hop(circ, hop);

    if (!iter || iter->state != CPATH_STATE_OPEN)
        return NULL;

    return node_get_by_id(iter->extend_info->identity_digest);
}

error_t circuit_payment_send_command_to_hop(origin_circuit_t *circ, uint8_t hopnum,uint8_t relay_command, const uint8_t *payload, ssize_t payload_len) {
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

error_t circuit_payment_send_command_to_origin(circuit_t *circ, uint8_t relay_command, const uint8_t *payload, ssize_t payload_len) {
    error_t ret;

/* Send the drop command to the origin */
    ret = relay_send_command_from_edge(0, circ, relay_command,
                                       (void*)payload, (size_t)payload_len,
                                       NULL);
    return ret;
}


OR_OP_request_t*
circuit_payment_handle_payment_negotiate(const cell_t *cell){
    OR_OP_request_t *negotiate;

    if (circuit_payment_negotiate_parse(&negotiate, cell->payload+RELAY_HEADER_SIZE,
                                        CELL_PAYLOAD_SIZE-RELAY_HEADER_SIZE) < 0) {
        log_fn(LOG_PROTOCOL_WARN, LD_CIRC,
               "Received malformed PADDING_NEGOTIATE cell; dropping.");
        return NULL;
    }

    return negotiate;
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

void circuit_payment__free(OR_OP_request_t *obj)
{
    if (obj == NULL)
        return;
    circuit_payment_negotiate_clear(obj);
    trunnel_memwipe(obj, sizeof(circpad_negotiate_t));
    trunnel_free_(obj);
}

static void
circuit_payment_negotiate_clear(OR_OP_request_t *obj)
{
    (void) obj;
}


OR_OP_request_t * payment_payload_new(void)
{
    OR_OP_request_t *val = tor_malloc_(sizeof(OR_OP_request_t));
    if (NULL == val)
        return NULL;
    val->command = CELL_PAYMENT;
    return val;
}



ssize_t
payment_into(OR_OP_request_t *obj, const uint8_t *input, const size_t len_in)
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
    check_failed:
    result = -1;
    goto fail;
    fail:
    trunnel_assert(result < 0);
    return result;
}


int
circuit_get_num_by_nickname(origin_circuit_t * circ, char* nickname)
{
    char nickname_array[USER_NAME_LEN];
    strcpy(nickname_array, nickname);

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
            if(strcmp(cpath_next->extend_info->nickname, nickname_array) == 0)
                return n;
        }
    }
    return 0;
}

int
circuit_get_length(origin_circuit_t * circ)
{
    int n = 0;
    if (circ != NULL && circ->cpath != NULL) {
        crypt_path_t *cpath, *cpath_next = NULL;
        for (cpath = circ->cpath;
             cpath->state == CPATH_STATE_OPEN
             && cpath_next != circ->cpath;
             cpath = cpath_next) {
            cpath_next = cpath->next;
            ++n;
            if(cpath_next->extend_info == NULL) return n;
        }
    }
    return n;
}

extend_info_t*
circuit_get_extended_data_by_nickname(origin_circuit_t * circ, char* nickname)
{
    char nickname_array[USER_NAME_LEN];
    strcpy(nickname_array, nickname);
    int n = 1;
    if(strcmp(circ->cpath->extend_info->nickname, nickname) == 0)
        return circ->cpath->extend_info;
    if (circ != NULL && circ->cpath != NULL) {
        crypt_path_t *cpath, *cpath_next = NULL;
        for (cpath = circ->cpath;
             cpath->state == CPATH_STATE_OPEN
             && cpath_next != circ->cpath;
             cpath = cpath_next) {
            cpath_next = cpath->next;
            ++n;
            if(strcmp(cpath_next->extend_info->nickname, nickname_array) == 0)
                return cpath_next->extend_info;
        }
    }
    return NULL;
}

void set_to_session_context(const char* session, const char* nickname, uint64_t channel_global_id, uint32_t circuit_id) {

    if (NULL == global_payment_session_list)
        global_payment_session_list = smartlist_new();

    payment_session_context_t *origin = NULL;

    SMARTLIST_FOREACH_BEGIN(global_payment_session_list, payment_session_context_t *, element)
                            {
                                if (strcmp(element->session_id, session) == 0) {
                                    origin = element;
                                }
                            }
    SMARTLIST_FOREACH_END(element);

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

void set_circuit_payment_info(uint32_t circuit_id) {

    if (NULL == global_payment_info_list)
        global_payment_info_list = smartlist_new();

    payment_info_context_t *origin = NULL;

    SMARTLIST_FOREACH_BEGIN(global_payment_info_list, payment_info_context_t *, element)
                            {
                                if (element->circuit_id == circuit_id) {
                                    origin = element;
                                }
                            }
    SMARTLIST_FOREACH_END(element);

    if (origin == NULL) {
        payment_info_context_t *ent = (payment_info_context_t *) tor_malloc_zero_(
                sizeof(payment_info_context_t));
        ent->circuit_id = circuit_id;
        ent->delay_payments_counter = 1;
        origin = ent;
        smartlist_add(global_payment_info_list, ent);
    } else {
        origin->delay_payments_counter++;
    }
}

payment_session_context_t* get_from_session_context_by_session_id(const char* session) {

    if (NULL == global_payment_session_list)
        global_payment_session_list = smartlist_new();

    payment_session_context_t *origin = NULL;

    SMARTLIST_FOREACH_BEGIN(global_payment_session_list, payment_session_context_t *, element)
                            {
                                if (strcmp(element->session_id, session) == 0) {
                                    return element;
                                }
                            }
    SMARTLIST_FOREACH_END(element);

    return origin;
}


payment_info_context_t* get_circuit_payment_info(int circuit_id) {

    if (NULL == global_payment_info_list)
        global_payment_info_list = smartlist_new();

    payment_info_context_t *origin = NULL;

    SMARTLIST_FOREACH_BEGIN(global_payment_info_list, payment_info_context_t *, element)
                            {
                                if (element->circuit_id == circuit_id) {
                                    return element;
                                }
                            }
    SMARTLIST_FOREACH_END(element);

    return origin;
}

void remove_from_session_context(payment_session_context_t* element) {

    smartlist_remove(global_payment_session_list, element);
    tor_free_(element);
}

void remove_circuit_payment_info(payment_info_context_t* element) {
    smartlist_remove(global_payment_info_list, element);
    tor_free_(element);
}

