//
// Created by root on 7/22/20.
//
#define CIRCUITLIST_PRIVATE
#define RELAY_PRIVATE
#include "core/or/or.h"
#include "core/or/circuitlist.h"
#include "core/or/relay.h"
#include "test/test.h"
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
#include "tinytest_macros.h"
#include "lib/cc/compat_compiler.h"


static void test_payment_cell(void *arg)
{
    cell_t cell;
    (void)arg;
    ssize_t len;

    memset(&cell, 0, sizeof(cell_t));

    cell.command = CELL_RELAY;

    OR_OP_request_t *input = tor_malloc_(sizeof(OR_OP_request_t));
    strcpy(input->session_id, "session_id");
    input->session_id_length = strlen(input->session_id);
    strcpy(input->nickname, "nickname");
    input->nicknameLength = strlen(input->nickname);
    strcpy(input->message, "message");
    input->messageTotalLength = strlen(input->message);
    strcpy(input->command_id, "command_id");
    input->command_id_length = strlen(input->command_id);

    input->command_type = 101;
    input->command = 201;
    input->version = PAYMENT_MSG_VERSION;
    input->is_last = 1;
    input->message_type = 206;
    input->messageLength = strlen(input->message);

    len = circuit_payment_negotiate_encode(cell.payload, CELL_PAYLOAD_SIZE, input);
    tt_int_op(len, OP_GE, 0);

    OR_OP_request_t *negotiate;

    len = circuit_payment_negotiate_parse(&negotiate, cell.payload,
                                    CELL_PAYLOAD_SIZE-RELAY_HEADER_SIZE);

    tt_int_op(len, OP_GE, 0);

    tt_int_op(input->command_type, OP_EQ, negotiate->command_type);
    tt_int_op(input->command, OP_EQ, negotiate->command);
    tt_int_op(input->version, OP_EQ, negotiate->version);
    tt_int_op(input->is_last, OP_EQ, negotiate->is_last);
    tt_int_op(input->message_type, OP_EQ, negotiate->message_type);
    tt_int_op(input->command_id_length, OP_EQ, negotiate->command_id_length);
    tt_int_op(input->messageTotalLength, OP_EQ, negotiate->messageTotalLength);
    tt_int_op(input->nicknameLength, OP_EQ, negotiate->nicknameLength);
    tt_int_op(input->session_id_length, OP_EQ, negotiate->session_id_length);

    tt_mem_op(input->session_id, OP_EQ, input->session_id, input->session_id_length);
    tt_mem_op(input->nickname, OP_EQ, input->nickname, input->nicknameLength);
    tt_mem_op(input->message, OP_EQ, input->message, input->messageTotalLength);
    tt_mem_op(input->command_id, OP_EQ, input->command_id, input->command_id_length);

    done:
    tor_free_(input);
    return;
}


static void test_payment_session_context(void *arg)
{
    (void)arg;

    tp_init_lists();

    const char* session_id = "sdfgjkhsdfjkh";
    const char* nickname = "nickname";
    uint64_t channel_global_id =101;
    uint32_t circuit_idl = 202;
    tp_store_session_context(session_id, nickname, channel_global_id, circuit_idl);

    payment_session_context_t *session_context = get_from_session_context_by_session_id(
            session_id);

    tt_int_op(session_context->circuit_id, OP_EQ, circuit_idl);
    tt_int_op(session_context->channel_global_id, OP_EQ, channel_global_id);
    tt_mem_op(session_context->nickname, OP_EQ, nickname, strlen(nickname));

    remove_from_session_context(session_context);

    session_context = get_from_session_context_by_session_id(
            session_id);

    if(session_context != NULL) {
        tt_int_op(1, OP_EQ, 0);
    }

    done:
    return;
}

static void test_payment_payment_info(void *arg)
{
    (void)arg;

    tp_init_lists();

    //uint32_t circuit_idl = 202;

    // set_circuit_payment_info(circuit_idl);

    // payment_info_context_t *context = get_circuit_payment_info(circuit_idl);

    // tt_int_op(context->circuit_id, OP_EQ, circuit_idl);

    // tp_remove_circuit_payment_info(context);

    // payment_session_context_t *sess_paym_context = get_from_session_context_by_session_id(NULL); // wrong argument type, what is meaning???
    // tt_ptr_op(sess_paym_context, OP_EQ, NULL);

    //done:
    return;
}

#define TEST(name, flags)                                               \
  { #name, test_payment_ ## name, flags, 0, NULL }

struct testcase_t test_paymets[] = {
        TEST(cell, 0),
        TEST(payment_info, 0),
        TEST(session_context, 0),
//        TEST(create_cells, 0),
//        TEST(created_cells, 0),
//        TEST(extend_cells, TT_FORK),
//        TEST(extended_cells, 0),
//        TEST(resolved_cells, 0),
//        TEST(is_destroy, 0),
        END_OF_TESTCASES
};
