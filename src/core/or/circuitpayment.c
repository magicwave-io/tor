/* Copyright (c) 2017 The Tor Project, Inc. */
/* See LICENSE for licensing information */


//file circuitpayment.c
//brief Circuit-level payment implementation


#include "core/or/or.h"
#include "core/or/circuitpadding.h"
#include "core/or/circuitpayment.h"
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

    return circuit_payment_send_command_to_hop(orig_circ, target_hopnum,
                                               RELAY_COMMAND_PAYMENT_COMMAND_TO_NODE,
                                               cell.payload, len);
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

    return circuit_payment_send_command_to_origin(circ,
                                                  RELAY_COMMAND_PAYMENT_COMMAND_TO_ORIGIN,
                                                  payload, len);
}

node_t* circuit_payment_get_nth_node(origin_circuit_t *circ, int hop) {
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

/* Send the drop command to the second hop */
    ret = relay_send_command_from_edge(0, TO_CIRCUIT(circ), relay_command,
                                       (const char *) payload, payload_len,
                                       target_hop);
    return ret;
}

error_t circuit_payment_send_command_to_origin(circuit_t *circ, uint8_t relay_command, const uint8_t *payload, ssize_t payload_len) {
    error_t ret;

/* Send the drop command to the second hop */
    ret = relay_send_command_from_edge(0, circ, relay_command,
                                       (const char *) payload, payload_len,
                                       NULL);
    return ret;
}


OR_OP_request_t*
circuit_payment_handle_payment_negotiate(cell_t *cell){
    OR_OP_request_t *negotiate;

    if (circuit_payment_negotiate_parse(&negotiate, cell->payload+RELAY_HEADER_SIZE,
                                CELL_PAYLOAD_SIZE-RELAY_HEADER_SIZE) < 0) {
        log_fn(LOG_PROTOCOL_WARN, LD_CIRC,
               "Received malformed PADDING_NEGOTIATE cell; dropping.");
        return NULL;
    }

    return negotiate;
}

ssize_t
circuit_payment_negotiate_parse(OR_OP_request_t **output, const uint8_t *input, const size_t len_in)
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

void
circuit_payment__free(OR_OP_request_t *obj)
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


void divideString(List_of_str_t* output, char *str, int len, int n)
{
    int str_size = len;
    int i;
    int part_size;
    part_size = str_size / n;
    int oddment = str_size % n;


// Check if string can be divided in
// n equal parts
    if (str_size / n == 0)
    {
        initialize_array(output[0].msg, MAX_MESSAGE_LEN);
        strcpy(output[0].msg, str);
    }
// Calculate the size of parts to
// find the division points

    for (i = 0; i < part_size; i++)
    {
        initialize_array(output[i].msg, MAX_MESSAGE_LEN);
        strncpy(output[i].msg, &str[i*n], n);
    }

    initialize_array(output[part_size].msg, MAX_MESSAGE_LEN);
    strncpy(output[part_size].msg, &str[part_size*n], oddment);
}

int
circuit_get_num_by_nickname(origin_circuit_t * circ, char* nickname)
{
    char nickname_array[MAX_HEX_NICKNAME_LEN+1] = {NULL};
    memcpy(nickname_array, nickname, sizeof(nickname));
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
            if(strcmp(cpath_next->extend_info->nickname, nickname_array) == 0)
                return n;
        }
    }
    return 0;
}

extend_info_t*
circuit_get_extended_data_by_nickname(origin_circuit_t * circ, char* nickname)
{
    char nickname_array[MAX_HEX_NICKNAME_LEN+1] = {NULL};
    memcpy(nickname_array, nickname, sizeof(nickname));
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

