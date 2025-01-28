/* conflux.c -- generated by Trunnel v1.5.3.
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
#include <stdlib.h>
#include "trunnel-impl.h"

#include "conflux.h"

#define TRUNNEL_SET_ERROR_CODE(obj) \
  do {                              \
    (obj)->trunnel_error_code_ = 1; \
  } while (0)

#if defined(__COVERITY__) || defined(__clang_analyzer__)
/* If we're running a static analysis tool, we don't want it to complain
 * that some of our remaining-bytes checks are dead-code. */
int conflux_deadcode_dummy__ = 0;
#define OR_DEADCODE_DUMMY || conflux_deadcode_dummy__
#else
#define OR_DEADCODE_DUMMY
#endif

#define CHECK_REMAINING(nbytes, label)                           \
  do {                                                           \
    if (remaining < (nbytes) OR_DEADCODE_DUMMY) {                \
      goto label;                                                \
    }                                                            \
  } while (0)

trn_cell_conflux_link_t *
trn_cell_conflux_link_new(void)
{
  trn_cell_conflux_link_t *val = trunnel_calloc(1, sizeof(trn_cell_conflux_link_t));
  if (NULL == val)
    return NULL;
  val->version = 1;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
trn_cell_conflux_link_clear(trn_cell_conflux_link_t *obj)
{
  (void) obj;
  TRUNNEL_DYNARRAY_WIPE(&obj->payload);
  TRUNNEL_DYNARRAY_CLEAR(&obj->payload);
}

void
trn_cell_conflux_link_free(trn_cell_conflux_link_t *obj)
{
  if (obj == NULL)
    return;
  trn_cell_conflux_link_clear(obj);
  trunnel_memwipe(obj, sizeof(trn_cell_conflux_link_t));
  trunnel_free_(obj);
}

uint8_t
trn_cell_conflux_link_get_version(const trn_cell_conflux_link_t *inp)
{
  return inp->version;
}
int
trn_cell_conflux_link_set_version(trn_cell_conflux_link_t *inp, uint8_t val)
{
  if (! ((val == 1))) {
     TRUNNEL_SET_ERROR_CODE(inp);
     return -1;
  }
  inp->version = val;
  return 0;
}
size_t
trn_cell_conflux_link_getlen_payload(const trn_cell_conflux_link_t *inp)
{
  return TRUNNEL_DYNARRAY_LEN(&inp->payload);
}

uint8_t
trn_cell_conflux_link_get_payload(trn_cell_conflux_link_t *inp, size_t idx)
{
  return TRUNNEL_DYNARRAY_GET(&inp->payload, idx);
}

uint8_t
trn_cell_conflux_link_getconst_payload(const trn_cell_conflux_link_t *inp, size_t idx)
{
  return trn_cell_conflux_link_get_payload((trn_cell_conflux_link_t*)inp, idx);
}
int
trn_cell_conflux_link_set_payload(trn_cell_conflux_link_t *inp, size_t idx, uint8_t elt)
{
  TRUNNEL_DYNARRAY_SET(&inp->payload, idx, elt);
  return 0;
}
int
trn_cell_conflux_link_add_payload(trn_cell_conflux_link_t *inp, uint8_t elt)
{
  TRUNNEL_DYNARRAY_ADD(uint8_t, &inp->payload, elt, {});
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}

uint8_t *
trn_cell_conflux_link_getarray_payload(trn_cell_conflux_link_t *inp)
{
  return inp->payload.elts_;
}
const uint8_t  *
trn_cell_conflux_link_getconstarray_payload(const trn_cell_conflux_link_t *inp)
{
  return (const uint8_t  *)trn_cell_conflux_link_getarray_payload((trn_cell_conflux_link_t*)inp);
}
int
trn_cell_conflux_link_setlen_payload(trn_cell_conflux_link_t *inp, size_t newlen)
{
  uint8_t *newptr;
  newptr = trunnel_dynarray_setlen(&inp->payload.allocated_,
                 &inp->payload.n_, inp->payload.elts_, newlen,
                 sizeof(inp->payload.elts_[0]), (trunnel_free_fn_t) NULL,
                 &inp->trunnel_error_code_);
  if (newlen != 0 && newptr == NULL)
    goto trunnel_alloc_failed;
  inp->payload.elts_ = newptr;
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}
const char *
trn_cell_conflux_link_check(const trn_cell_conflux_link_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->version == 1))
    return "Integer out of bounds";
  return NULL;
}

ssize_t
trn_cell_conflux_link_encoded_len(const trn_cell_conflux_link_t *obj)
{
  ssize_t result = 0;

  if (NULL != trn_cell_conflux_link_check(obj))
     return -1;


  /* Length of u8 version IN [1] */
  result += 1;

  /* Length of u8 payload[] */
  result += TRUNNEL_DYNARRAY_LEN(&obj->payload);
  return result;
}
int
trn_cell_conflux_link_clear_errors(trn_cell_conflux_link_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
trn_cell_conflux_link_encode(uint8_t *output, const size_t avail, const trn_cell_conflux_link_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = trn_cell_conflux_link_encoded_len(obj);
#endif

  if (NULL != (msg = trn_cell_conflux_link_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u8 version IN [1] */
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->version));
  written += 1; ptr += 1;

  /* Encode u8 payload[] */
  {
    size_t elt_len = TRUNNEL_DYNARRAY_LEN(&obj->payload);
    trunnel_assert(written <= avail);
    if (avail - written < elt_len)
      goto truncated;
    if (elt_len)
      memcpy(ptr, obj->payload.elts_, elt_len);
    written += elt_len; ptr += elt_len;
  }


  trunnel_assert(ptr == output + written);
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
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As trn_cell_conflux_link_parse(), but do not allocate the output
 * object.
 */
static ssize_t
trn_cell_conflux_link_parse_into(trn_cell_conflux_link_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u8 version IN [1] */
  CHECK_REMAINING(1, truncated);
  obj->version = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  if (! (obj->version == 1))
    goto fail;

  /* Parse u8 payload[] */
  TRUNNEL_DYNARRAY_EXPAND(uint8_t, &obj->payload, remaining, {});
  obj->payload.n_ = remaining;
  if (remaining)
    memcpy(obj->payload.elts_, ptr, remaining);
  ptr += remaining; remaining -= remaining;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 trunnel_alloc_failed:
  return -1;
 fail:
  result = -1;
  return result;
}

ssize_t
trn_cell_conflux_link_parse(trn_cell_conflux_link_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = trn_cell_conflux_link_new();
  if (NULL == *output)
    return -1;
  result = trn_cell_conflux_link_parse_into(*output, input, len_in);
  if (result < 0) {
    trn_cell_conflux_link_free(*output);
    *output = NULL;
  }
  return result;
}
trn_cell_conflux_link_payload_v1_t *
trn_cell_conflux_link_payload_v1_new(void)
{
  trn_cell_conflux_link_payload_v1_t *val = trunnel_calloc(1, sizeof(trn_cell_conflux_link_payload_v1_t));
  if (NULL == val)
    return NULL;
  val->desired_ux = CONFLUX_UX_HIGH_THROUGHPUT;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
trn_cell_conflux_link_payload_v1_clear(trn_cell_conflux_link_payload_v1_t *obj)
{
  (void) obj;
}

void
trn_cell_conflux_link_payload_v1_free(trn_cell_conflux_link_payload_v1_t *obj)
{
  if (obj == NULL)
    return;
  trn_cell_conflux_link_payload_v1_clear(obj);
  trunnel_memwipe(obj, sizeof(trn_cell_conflux_link_payload_v1_t));
  trunnel_free_(obj);
}

size_t
trn_cell_conflux_link_payload_v1_getlen_nonce(const trn_cell_conflux_link_payload_v1_t *inp)
{
  (void)inp;  return 32;
}

uint8_t
trn_cell_conflux_link_payload_v1_get_nonce(trn_cell_conflux_link_payload_v1_t *inp, size_t idx)
{
  trunnel_assert(idx < 32);
  return inp->nonce[idx];
}

uint8_t
trn_cell_conflux_link_payload_v1_getconst_nonce(const trn_cell_conflux_link_payload_v1_t *inp, size_t idx)
{
  return trn_cell_conflux_link_payload_v1_get_nonce((trn_cell_conflux_link_payload_v1_t*)inp, idx);
}
int
trn_cell_conflux_link_payload_v1_set_nonce(trn_cell_conflux_link_payload_v1_t *inp, size_t idx, uint8_t elt)
{
  trunnel_assert(idx < 32);
  inp->nonce[idx] = elt;
  return 0;
}

uint8_t *
trn_cell_conflux_link_payload_v1_getarray_nonce(trn_cell_conflux_link_payload_v1_t *inp)
{
  return inp->nonce;
}
const uint8_t  *
trn_cell_conflux_link_payload_v1_getconstarray_nonce(const trn_cell_conflux_link_payload_v1_t *inp)
{
  return (const uint8_t  *)trn_cell_conflux_link_payload_v1_getarray_nonce((trn_cell_conflux_link_payload_v1_t*)inp);
}
uint64_t
trn_cell_conflux_link_payload_v1_get_last_seqno_sent(const trn_cell_conflux_link_payload_v1_t *inp)
{
  return inp->last_seqno_sent;
}
int
trn_cell_conflux_link_payload_v1_set_last_seqno_sent(trn_cell_conflux_link_payload_v1_t *inp, uint64_t val)
{
  inp->last_seqno_sent = val;
  return 0;
}
uint64_t
trn_cell_conflux_link_payload_v1_get_last_seqno_recv(const trn_cell_conflux_link_payload_v1_t *inp)
{
  return inp->last_seqno_recv;
}
int
trn_cell_conflux_link_payload_v1_set_last_seqno_recv(trn_cell_conflux_link_payload_v1_t *inp, uint64_t val)
{
  inp->last_seqno_recv = val;
  return 0;
}
uint8_t
trn_cell_conflux_link_payload_v1_get_desired_ux(const trn_cell_conflux_link_payload_v1_t *inp)
{
  return inp->desired_ux;
}
int
trn_cell_conflux_link_payload_v1_set_desired_ux(trn_cell_conflux_link_payload_v1_t *inp, uint8_t val)
{
  if (! ((val == CONFLUX_UX_HIGH_THROUGHPUT || val == CONFLUX_UX_LOW_MEM_LATENCY || val == CONFLUX_UX_LOW_MEM_THROUGHPUT || val == CONFLUX_UX_MIN_LATENCY || val == CONFLUX_UX_NO_OPINION))) {
     TRUNNEL_SET_ERROR_CODE(inp);
     return -1;
  }
  inp->desired_ux = val;
  return 0;
}
const char *
trn_cell_conflux_link_payload_v1_check(const trn_cell_conflux_link_payload_v1_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->desired_ux == CONFLUX_UX_HIGH_THROUGHPUT || obj->desired_ux == CONFLUX_UX_LOW_MEM_LATENCY || obj->desired_ux == CONFLUX_UX_LOW_MEM_THROUGHPUT || obj->desired_ux == CONFLUX_UX_MIN_LATENCY || obj->desired_ux == CONFLUX_UX_NO_OPINION))
    return "Integer out of bounds";
  return NULL;
}

ssize_t
trn_cell_conflux_link_payload_v1_encoded_len(const trn_cell_conflux_link_payload_v1_t *obj)
{
  ssize_t result = 0;

  if (NULL != trn_cell_conflux_link_payload_v1_check(obj))
     return -1;


  /* Length of u8 nonce[32] */
  result += 32;

  /* Length of u64 last_seqno_sent */
  result += 8;

  /* Length of u64 last_seqno_recv */
  result += 8;

  /* Length of u8 desired_ux IN [CONFLUX_UX_HIGH_THROUGHPUT, CONFLUX_UX_LOW_MEM_LATENCY, CONFLUX_UX_LOW_MEM_THROUGHPUT, CONFLUX_UX_MIN_LATENCY, CONFLUX_UX_NO_OPINION] */
  result += 1;
  return result;
}
int
trn_cell_conflux_link_payload_v1_clear_errors(trn_cell_conflux_link_payload_v1_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
trn_cell_conflux_link_payload_v1_encode(uint8_t *output, const size_t avail, const trn_cell_conflux_link_payload_v1_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = trn_cell_conflux_link_payload_v1_encoded_len(obj);
#endif

  if (NULL != (msg = trn_cell_conflux_link_payload_v1_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u8 nonce[32] */
  trunnel_assert(written <= avail);
  if (avail - written < 32)
    goto truncated;
  memcpy(ptr, obj->nonce, 32);
  written += 32; ptr += 32;

  /* Encode u64 last_seqno_sent */
  trunnel_assert(written <= avail);
  if (avail - written < 8)
    goto truncated;
  trunnel_set_uint64(ptr, trunnel_htonll(obj->last_seqno_sent));
  written += 8; ptr += 8;

  /* Encode u64 last_seqno_recv */
  trunnel_assert(written <= avail);
  if (avail - written < 8)
    goto truncated;
  trunnel_set_uint64(ptr, trunnel_htonll(obj->last_seqno_recv));
  written += 8; ptr += 8;

  /* Encode u8 desired_ux IN [CONFLUX_UX_HIGH_THROUGHPUT, CONFLUX_UX_LOW_MEM_LATENCY, CONFLUX_UX_LOW_MEM_THROUGHPUT, CONFLUX_UX_MIN_LATENCY, CONFLUX_UX_NO_OPINION] */
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->desired_ux));
  written += 1; ptr += 1;


  trunnel_assert(ptr == output + written);
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
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As trn_cell_conflux_link_payload_v1_parse(), but do not allocate
 * the output object.
 */
static ssize_t
trn_cell_conflux_link_payload_v1_parse_into(trn_cell_conflux_link_payload_v1_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u8 nonce[32] */
  CHECK_REMAINING(32, truncated);
  memcpy(obj->nonce, ptr, 32);
  remaining -= 32; ptr += 32;

  /* Parse u64 last_seqno_sent */
  CHECK_REMAINING(8, truncated);
  obj->last_seqno_sent = trunnel_ntohll(trunnel_get_uint64(ptr));
  remaining -= 8; ptr += 8;

  /* Parse u64 last_seqno_recv */
  CHECK_REMAINING(8, truncated);
  obj->last_seqno_recv = trunnel_ntohll(trunnel_get_uint64(ptr));
  remaining -= 8; ptr += 8;

  /* Parse u8 desired_ux IN [CONFLUX_UX_HIGH_THROUGHPUT, CONFLUX_UX_LOW_MEM_LATENCY, CONFLUX_UX_LOW_MEM_THROUGHPUT, CONFLUX_UX_MIN_LATENCY, CONFLUX_UX_NO_OPINION] */
  CHECK_REMAINING(1, truncated);
  obj->desired_ux = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  if (! (obj->desired_ux == CONFLUX_UX_HIGH_THROUGHPUT || obj->desired_ux == CONFLUX_UX_LOW_MEM_LATENCY || obj->desired_ux == CONFLUX_UX_LOW_MEM_THROUGHPUT || obj->desired_ux == CONFLUX_UX_MIN_LATENCY || obj->desired_ux == CONFLUX_UX_NO_OPINION))
    goto fail;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 fail:
  result = -1;
  return result;
}

ssize_t
trn_cell_conflux_link_payload_v1_parse(trn_cell_conflux_link_payload_v1_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = trn_cell_conflux_link_payload_v1_new();
  if (NULL == *output)
    return -1;
  result = trn_cell_conflux_link_payload_v1_parse_into(*output, input, len_in);
  if (result < 0) {
    trn_cell_conflux_link_payload_v1_free(*output);
    *output = NULL;
  }
  return result;
}
trn_cell_conflux_linked_t *
trn_cell_conflux_linked_new(void)
{
  trn_cell_conflux_linked_t *val = trunnel_calloc(1, sizeof(trn_cell_conflux_linked_t));
  if (NULL == val)
    return NULL;
  val->version = 1;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
trn_cell_conflux_linked_clear(trn_cell_conflux_linked_t *obj)
{
  (void) obj;
  TRUNNEL_DYNARRAY_WIPE(&obj->payload);
  TRUNNEL_DYNARRAY_CLEAR(&obj->payload);
}

void
trn_cell_conflux_linked_free(trn_cell_conflux_linked_t *obj)
{
  if (obj == NULL)
    return;
  trn_cell_conflux_linked_clear(obj);
  trunnel_memwipe(obj, sizeof(trn_cell_conflux_linked_t));
  trunnel_free_(obj);
}

uint8_t
trn_cell_conflux_linked_get_version(const trn_cell_conflux_linked_t *inp)
{
  return inp->version;
}
int
trn_cell_conflux_linked_set_version(trn_cell_conflux_linked_t *inp, uint8_t val)
{
  if (! ((val == 1))) {
     TRUNNEL_SET_ERROR_CODE(inp);
     return -1;
  }
  inp->version = val;
  return 0;
}
size_t
trn_cell_conflux_linked_getlen_payload(const trn_cell_conflux_linked_t *inp)
{
  return TRUNNEL_DYNARRAY_LEN(&inp->payload);
}

uint8_t
trn_cell_conflux_linked_get_payload(trn_cell_conflux_linked_t *inp, size_t idx)
{
  return TRUNNEL_DYNARRAY_GET(&inp->payload, idx);
}

uint8_t
trn_cell_conflux_linked_getconst_payload(const trn_cell_conflux_linked_t *inp, size_t idx)
{
  return trn_cell_conflux_linked_get_payload((trn_cell_conflux_linked_t*)inp, idx);
}
int
trn_cell_conflux_linked_set_payload(trn_cell_conflux_linked_t *inp, size_t idx, uint8_t elt)
{
  TRUNNEL_DYNARRAY_SET(&inp->payload, idx, elt);
  return 0;
}
int
trn_cell_conflux_linked_add_payload(trn_cell_conflux_linked_t *inp, uint8_t elt)
{
  TRUNNEL_DYNARRAY_ADD(uint8_t, &inp->payload, elt, {});
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}

uint8_t *
trn_cell_conflux_linked_getarray_payload(trn_cell_conflux_linked_t *inp)
{
  return inp->payload.elts_;
}
const uint8_t  *
trn_cell_conflux_linked_getconstarray_payload(const trn_cell_conflux_linked_t *inp)
{
  return (const uint8_t  *)trn_cell_conflux_linked_getarray_payload((trn_cell_conflux_linked_t*)inp);
}
int
trn_cell_conflux_linked_setlen_payload(trn_cell_conflux_linked_t *inp, size_t newlen)
{
  uint8_t *newptr;
  newptr = trunnel_dynarray_setlen(&inp->payload.allocated_,
                 &inp->payload.n_, inp->payload.elts_, newlen,
                 sizeof(inp->payload.elts_[0]), (trunnel_free_fn_t) NULL,
                 &inp->trunnel_error_code_);
  if (newlen != 0 && newptr == NULL)
    goto trunnel_alloc_failed;
  inp->payload.elts_ = newptr;
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}
const char *
trn_cell_conflux_linked_check(const trn_cell_conflux_linked_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->version == 1))
    return "Integer out of bounds";
  return NULL;
}

ssize_t
trn_cell_conflux_linked_encoded_len(const trn_cell_conflux_linked_t *obj)
{
  ssize_t result = 0;

  if (NULL != trn_cell_conflux_linked_check(obj))
     return -1;


  /* Length of u8 version IN [1] */
  result += 1;

  /* Length of u8 payload[] */
  result += TRUNNEL_DYNARRAY_LEN(&obj->payload);
  return result;
}
int
trn_cell_conflux_linked_clear_errors(trn_cell_conflux_linked_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
trn_cell_conflux_linked_encode(uint8_t *output, const size_t avail, const trn_cell_conflux_linked_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = trn_cell_conflux_linked_encoded_len(obj);
#endif

  if (NULL != (msg = trn_cell_conflux_linked_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u8 version IN [1] */
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->version));
  written += 1; ptr += 1;

  /* Encode u8 payload[] */
  {
    size_t elt_len = TRUNNEL_DYNARRAY_LEN(&obj->payload);
    trunnel_assert(written <= avail);
    if (avail - written < elt_len)
      goto truncated;
    if (elt_len)
      memcpy(ptr, obj->payload.elts_, elt_len);
    written += elt_len; ptr += elt_len;
  }


  trunnel_assert(ptr == output + written);
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
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As trn_cell_conflux_linked_parse(), but do not allocate the output
 * object.
 */
static ssize_t
trn_cell_conflux_linked_parse_into(trn_cell_conflux_linked_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u8 version IN [1] */
  CHECK_REMAINING(1, truncated);
  obj->version = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  if (! (obj->version == 1))
    goto fail;

  /* Parse u8 payload[] */
  TRUNNEL_DYNARRAY_EXPAND(uint8_t, &obj->payload, remaining, {});
  obj->payload.n_ = remaining;
  if (remaining)
    memcpy(obj->payload.elts_, ptr, remaining);
  ptr += remaining; remaining -= remaining;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 trunnel_alloc_failed:
  return -1;
 fail:
  result = -1;
  return result;
}

ssize_t
trn_cell_conflux_linked_parse(trn_cell_conflux_linked_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = trn_cell_conflux_linked_new();
  if (NULL == *output)
    return -1;
  result = trn_cell_conflux_linked_parse_into(*output, input, len_in);
  if (result < 0) {
    trn_cell_conflux_linked_free(*output);
    *output = NULL;
  }
  return result;
}
trn_cell_conflux_linked_ack_t *
trn_cell_conflux_linked_ack_new(void)
{
  trn_cell_conflux_linked_ack_t *val = trunnel_calloc(1, sizeof(trn_cell_conflux_linked_ack_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
trn_cell_conflux_linked_ack_clear(trn_cell_conflux_linked_ack_t *obj)
{
  (void) obj;
  TRUNNEL_DYNARRAY_WIPE(&obj->payload);
  TRUNNEL_DYNARRAY_CLEAR(&obj->payload);
}

void
trn_cell_conflux_linked_ack_free(trn_cell_conflux_linked_ack_t *obj)
{
  if (obj == NULL)
    return;
  trn_cell_conflux_linked_ack_clear(obj);
  trunnel_memwipe(obj, sizeof(trn_cell_conflux_linked_ack_t));
  trunnel_free_(obj);
}

size_t
trn_cell_conflux_linked_ack_getlen_payload(const trn_cell_conflux_linked_ack_t *inp)
{
  return TRUNNEL_DYNARRAY_LEN(&inp->payload);
}

uint8_t
trn_cell_conflux_linked_ack_get_payload(trn_cell_conflux_linked_ack_t *inp, size_t idx)
{
  return TRUNNEL_DYNARRAY_GET(&inp->payload, idx);
}

uint8_t
trn_cell_conflux_linked_ack_getconst_payload(const trn_cell_conflux_linked_ack_t *inp, size_t idx)
{
  return trn_cell_conflux_linked_ack_get_payload((trn_cell_conflux_linked_ack_t*)inp, idx);
}
int
trn_cell_conflux_linked_ack_set_payload(trn_cell_conflux_linked_ack_t *inp, size_t idx, uint8_t elt)
{
  TRUNNEL_DYNARRAY_SET(&inp->payload, idx, elt);
  return 0;
}
int
trn_cell_conflux_linked_ack_add_payload(trn_cell_conflux_linked_ack_t *inp, uint8_t elt)
{
  TRUNNEL_DYNARRAY_ADD(uint8_t, &inp->payload, elt, {});
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}

uint8_t *
trn_cell_conflux_linked_ack_getarray_payload(trn_cell_conflux_linked_ack_t *inp)
{
  return inp->payload.elts_;
}
const uint8_t  *
trn_cell_conflux_linked_ack_getconstarray_payload(const trn_cell_conflux_linked_ack_t *inp)
{
  return (const uint8_t  *)trn_cell_conflux_linked_ack_getarray_payload((trn_cell_conflux_linked_ack_t*)inp);
}
int
trn_cell_conflux_linked_ack_setlen_payload(trn_cell_conflux_linked_ack_t *inp, size_t newlen)
{
  uint8_t *newptr;
  newptr = trunnel_dynarray_setlen(&inp->payload.allocated_,
                 &inp->payload.n_, inp->payload.elts_, newlen,
                 sizeof(inp->payload.elts_[0]), (trunnel_free_fn_t) NULL,
                 &inp->trunnel_error_code_);
  if (newlen != 0 && newptr == NULL)
    goto trunnel_alloc_failed;
  inp->payload.elts_ = newptr;
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}
const char *
trn_cell_conflux_linked_ack_check(const trn_cell_conflux_linked_ack_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  return NULL;
}

ssize_t
trn_cell_conflux_linked_ack_encoded_len(const trn_cell_conflux_linked_ack_t *obj)
{
  ssize_t result = 0;

  if (NULL != trn_cell_conflux_linked_ack_check(obj))
     return -1;


  /* Length of u8 payload[] */
  result += TRUNNEL_DYNARRAY_LEN(&obj->payload);
  return result;
}
int
trn_cell_conflux_linked_ack_clear_errors(trn_cell_conflux_linked_ack_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
trn_cell_conflux_linked_ack_encode(uint8_t *output, const size_t avail, const trn_cell_conflux_linked_ack_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = trn_cell_conflux_linked_ack_encoded_len(obj);
#endif

  if (NULL != (msg = trn_cell_conflux_linked_ack_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u8 payload[] */
  {
    size_t elt_len = TRUNNEL_DYNARRAY_LEN(&obj->payload);
    trunnel_assert(written <= avail);
    if (avail - written < elt_len)
      goto truncated;
    if (elt_len)
      memcpy(ptr, obj->payload.elts_, elt_len);
    written += elt_len; ptr += elt_len;
  }


  trunnel_assert(ptr == output + written);
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
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As trn_cell_conflux_linked_ack_parse(), but do not allocate the
 * output object.
 */
static ssize_t
trn_cell_conflux_linked_ack_parse_into(trn_cell_conflux_linked_ack_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u8 payload[] */
  TRUNNEL_DYNARRAY_EXPAND(uint8_t, &obj->payload, remaining, {});
  obj->payload.n_ = remaining;
  if (remaining)
    memcpy(obj->payload.elts_, ptr, remaining);
  ptr += remaining; remaining -= remaining;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 trunnel_alloc_failed:
  return -1;
}

ssize_t
trn_cell_conflux_linked_ack_parse(trn_cell_conflux_linked_ack_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = trn_cell_conflux_linked_ack_new();
  if (NULL == *output)
    return -1;
  result = trn_cell_conflux_linked_ack_parse_into(*output, input, len_in);
  if (result < 0) {
    trn_cell_conflux_linked_ack_free(*output);
    *output = NULL;
  }
  return result;
}
trn_cell_conflux_switch_t *
trn_cell_conflux_switch_new(void)
{
  trn_cell_conflux_switch_t *val = trunnel_calloc(1, sizeof(trn_cell_conflux_switch_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
trn_cell_conflux_switch_clear(trn_cell_conflux_switch_t *obj)
{
  (void) obj;
}

void
trn_cell_conflux_switch_free(trn_cell_conflux_switch_t *obj)
{
  if (obj == NULL)
    return;
  trn_cell_conflux_switch_clear(obj);
  trunnel_memwipe(obj, sizeof(trn_cell_conflux_switch_t));
  trunnel_free_(obj);
}

uint32_t
trn_cell_conflux_switch_get_seqnum(const trn_cell_conflux_switch_t *inp)
{
  return inp->seqnum;
}
int
trn_cell_conflux_switch_set_seqnum(trn_cell_conflux_switch_t *inp, uint32_t val)
{
  inp->seqnum = val;
  return 0;
}
const char *
trn_cell_conflux_switch_check(const trn_cell_conflux_switch_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  return NULL;
}

ssize_t
trn_cell_conflux_switch_encoded_len(const trn_cell_conflux_switch_t *obj)
{
  ssize_t result = 0;

  if (NULL != trn_cell_conflux_switch_check(obj))
     return -1;


  /* Length of u32 seqnum */
  result += 4;
  return result;
}
int
trn_cell_conflux_switch_clear_errors(trn_cell_conflux_switch_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
trn_cell_conflux_switch_encode(uint8_t *output, const size_t avail, const trn_cell_conflux_switch_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = trn_cell_conflux_switch_encoded_len(obj);
#endif

  if (NULL != (msg = trn_cell_conflux_switch_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u32 seqnum */
  trunnel_assert(written <= avail);
  if (avail - written < 4)
    goto truncated;
  trunnel_set_uint32(ptr, trunnel_htonl(obj->seqnum));
  written += 4; ptr += 4;


  trunnel_assert(ptr == output + written);
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
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As trn_cell_conflux_switch_parse(), but do not allocate the output
 * object.
 */
static ssize_t
trn_cell_conflux_switch_parse_into(trn_cell_conflux_switch_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u32 seqnum */
  CHECK_REMAINING(4, truncated);
  obj->seqnum = trunnel_ntohl(trunnel_get_uint32(ptr));
  remaining -= 4; ptr += 4;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
}

ssize_t
trn_cell_conflux_switch_parse(trn_cell_conflux_switch_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = trn_cell_conflux_switch_new();
  if (NULL == *output)
    return -1;
  result = trn_cell_conflux_switch_parse_into(*output, input, len_in);
  if (result < 0) {
    trn_cell_conflux_switch_free(*output);
    *output = NULL;
  }
  return result;
}
