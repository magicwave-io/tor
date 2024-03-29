/* Copyright 2003 Roger Dingledine.
 * Copyright 2004-2007 Roger Dingledine, Nick Mathewson */
/* See LICENSE for licensing information */
/* $Id$ */
const char tortls_c_id[] =
  "$Id$";

/**
 * \file tortls.c
 * \brief Wrapper functions to present a consistent interface to
 * TLS, SSL, and X.509 functions from OpenSSL.
 **/

/* (Unlike other tor functions, these
 * are prefixed with tor_ in order to avoid conflicting with OpenSSL
 * functions and variables.)
 */

#include "orconfig.h"
#include "./crypto.h"
#include "./tortls.h"
#include "./util.h"
#include "./log.h"
#include <string.h>

/* Copied from or.h */
#define LEGAL_NICKNAME_CHARACTERS \
  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"

#include <assert.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/tls1.h>
#include <openssl/asn1.h>
#include <openssl/bio.h>

/** How long do identity certificates live? (sec) */
#define IDENTITY_CERT_LIFETIME  (365*24*60*60)

/** Structure holding the TLS state for a single connection. */
typedef struct tor_tls_context_t {
  SSL_CTX *ctx;
} tor_tls_context_t;

/** Holds a SSL object and its associated data.  Members are only
 * accessed from within tortls.c.
 */
struct tor_tls_t {
  SSL *ssl; /**< An OpenSSL SSL object. */
  int socket; /**< The underlying file descriptor for this TLS connection. */
  enum {
    TOR_TLS_ST_HANDSHAKE, TOR_TLS_ST_OPEN, TOR_TLS_ST_GOTCLOSE,
    TOR_TLS_ST_SENTCLOSE, TOR_TLS_ST_CLOSED
  } state; /**< The current SSL state, depending on which operations have
            * completed successfully. */
  int isServer; /**< True iff this is a server-side connection */
  size_t wantwrite_n; /**< 0 normally, >0 if we returned wantwrite last
                       * time. */
  unsigned long last_write_count;
  unsigned long last_read_count;
};

static X509* tor_tls_create_certificate(crypto_pk_env_t *rsa,
                                        crypto_pk_env_t *rsa_sign,
                                        const char *cname,
                                        const char *cname_sign,
                                        unsigned int lifetime);

/** Global tls context. We keep it here because nobody else needs to
 * touch it. */
static tor_tls_context_t *global_tls_context = NULL;
/** True iff tor_tls_init() has been called. */
static int tls_library_is_initialized = 0;

/* Module-internal error codes. */
#define _TOR_TLS_SYSCALL    (_MIN_TOR_TLS_ERROR_VAL - 2)
#define _TOR_TLS_ZERORETURN (_MIN_TOR_TLS_ERROR_VAL - 1)

/* These functions are declared in crypto.c but not exported. */
EVP_PKEY *_crypto_pk_env_get_evp_pkey(crypto_pk_env_t *env, int private);
crypto_pk_env_t *_crypto_new_pk_env_rsa(RSA *rsa);
DH *_crypto_dh_env_get_dh(crypto_dh_env_t *dh);

/** Log all pending tls errors at level <b>severity</b>.  Use
 * <b>doing</b> to describe our current activities.
 */
static void
tls_log_errors(int severity, const char *doing)
{
  int err;
  const char *msg, *lib, *func;
  while ((err = ERR_get_error()) != 0) {
    msg = (const char*)ERR_reason_error_string(err);
    lib = (const char*)ERR_lib_error_string(err);
    func = (const char*)ERR_func_error_string(err);
    if (!msg) msg = "(null)";
    if (doing) {
      log(severity, LD_NET, "TLS error while %s: %s (in %s:%s)",
          doing, msg, lib,func);
    } else {
      log(severity, LD_NET, "TLS error: %s (in %s:%s)", msg, lib, func);
    }
  }
}

/** Convert an errno (or a WSAerrno on windows) into a TOR_TLS_* error
 * code. */
static int
tor_errno_to_tls_error(int e)
{
#if defined(MS_WINDOWS) && !defined(USE_BSOCKETS)
  switch (e) {
    case WSAECONNRESET: // most common
      return TOR_TLS_ERROR_CONNRESET;
    case WSAETIMEDOUT:
      return TOR_TLS_ERROR_TIMEOUT;
    case WSAENETUNREACH:
    case WSAEHOSTUNREACH:
      return TOR_TLS_ERROR_NO_ROUTE;
    case WSAECONNREFUSED:
      return TOR_TLS_ERROR_CONNREFUSED; // least common
    default:
      return TOR_TLS_ERROR_MISC;
  }
#else
  switch (e) {
    case ECONNRESET: // most common
      return TOR_TLS_ERROR_CONNRESET;
    case ETIMEDOUT:
      return TOR_TLS_ERROR_TIMEOUT;
    case EHOSTUNREACH:
    case ENETUNREACH:
      return TOR_TLS_ERROR_NO_ROUTE;
    case ECONNREFUSED:
      return TOR_TLS_ERROR_CONNREFUSED; // least common
    default:
      return TOR_TLS_ERROR_MISC;
  }
#endif
}

#define CATCH_SYSCALL 1
#define CATCH_ZERO    2

/** Given a TLS object and the result of an SSL_* call, use
 * SSL_get_error to determine whether an error has occurred, and if so
 * which one.  Return one of TOR_TLS_{DONE|WANTREAD|WANTWRITE|ERROR}.
 * If extra&CATCH_SYSCALL is true, return _TOR_TLS_SYSCALL instead of
 * reporting syscall errors.  If extra&CATCH_ZERO is true, return
 * _TOR_TLS_ZERORETURN instead of reporting zero-return errors.
 *
 * If an error has occurred, log it at level <b>severity</b> and describe the
 * current action as <b>doing</b>.
 */
static int
tor_tls_get_error(tor_tls_t *tls, int r, int extra,
                  const char *doing, int severity)
{
  int err = SSL_get_error(tls->ssl, r);
  int tor_error = TOR_TLS_ERROR_MISC;
  switch (err) {
    case SSL_ERROR_NONE:
      return TOR_TLS_DONE;
    case SSL_ERROR_WANT_READ:
      return TOR_TLS_WANTREAD;
    case SSL_ERROR_WANT_WRITE:
      return TOR_TLS_WANTWRITE;
    case SSL_ERROR_SYSCALL:
      if (extra&CATCH_SYSCALL)
        return _TOR_TLS_SYSCALL;
      if (r == 0) {
        log(severity, LD_NET, "TLS error: unexpected close while %s", doing);
        tor_error = TOR_TLS_ERROR_IO;
      } else {
        int e = tor_socket_errno(tls->socket);
        log(severity, LD_NET,
            "TLS error: <syscall error while %s> (errno=%d: %s)",
            doing, e, tor_socket_strerror(e));
        tor_error = tor_errno_to_tls_error(e);
      }
      tls_log_errors(severity, doing);
      return tor_error;
    case SSL_ERROR_ZERO_RETURN:
      if (extra&CATCH_ZERO)
        return _TOR_TLS_ZERORETURN;
      log(severity, LD_NET, "TLS error: Zero return");
      tls_log_errors(severity, doing);
      /* XXXX Actually, a 'zero return' error has a pretty specific meaning:
       * the connection has been closed cleanly.  */
      return TOR_TLS_ERROR_MISC;
    default:
      tls_log_errors(severity, doing);
      return TOR_TLS_ERROR_MISC;
  }
}

/** Initialize OpenSSL, unless it has already been initialized.
 */
static void
tor_tls_init(void)
{
  if (!tls_library_is_initialized) {
    SSL_library_init();
    SSL_load_error_strings();
    crypto_global_init(-1);
    tls_library_is_initialized = 1;
  }
}

/** Free all global TLS structures. */
void
tor_tls_free_all(void)
{
  if (global_tls_context) {
    SSL_CTX_free(global_tls_context->ctx);
    tor_free(global_tls_context);
    global_tls_context = NULL;
  }
}

/** We need to give OpenSSL a callback to verify certificates. This is
 * it: We always accept peer certs and complete the handshake.  We
 * don't validate them until later.
 */
static int
always_accept_verify_cb(int preverify_ok,
                        X509_STORE_CTX *x509_ctx)
{
  /* avoid "unused parameter" warning. */
  preverify_ok = 0;
  x509_ctx = NULL;
  return 1;
}

/** Generate and sign an X509 certificate with the public key <b>rsa</b>,
 * signed by the private key <b>rsa_sign</b>.  The commonName of the
 * certificate will be <b>cname</b>; the commonName of the issuer will be
 * <b>cname_sign</b>. The cert will be valid for <b>cert_lifetime</b> seconds
 * starting from now.  Return a certificate on success, NULL on
 * failure.
 */
static X509 *
tor_tls_create_certificate(crypto_pk_env_t *rsa,
                           crypto_pk_env_t *rsa_sign,
                           const char *cname,
                           const char *cname_sign,
                           unsigned int cert_lifetime)
{
  time_t start_time, end_time;
  EVP_PKEY *sign_pkey = NULL, *pkey=NULL;
  X509 *x509 = NULL;
  X509_NAME *name = NULL, *name_issuer=NULL;
  int nid;

  tor_tls_init();

  start_time = time(NULL);

  tor_assert(rsa);
  tor_assert(cname);
  tor_assert(rsa_sign);
  tor_assert(cname_sign);
  if (!(sign_pkey = _crypto_pk_env_get_evp_pkey(rsa_sign,1)))
    goto error;
  if (!(pkey = _crypto_pk_env_get_evp_pkey(rsa,0)))
    goto error;
  if (!(x509 = X509_new()))
    goto error;
  if (!(X509_set_version(x509, 2)))
    goto error;
  if (!(ASN1_INTEGER_set(X509_get_serialNumber(x509), (long)start_time)))
    goto error;

  if (!(name = X509_NAME_new()))
    goto error;
  if ((nid = OBJ_txt2nid("organizationName")) == NID_undef)
    goto error;
  if (!(X509_NAME_add_entry_by_NID(name, nid, MBSTRING_ASC,
                                   (unsigned char*)"Tor", -1, -1, 0)))
    goto error;
  if ((nid = OBJ_txt2nid("commonName")) == NID_undef) goto error;
  if (!(X509_NAME_add_entry_by_NID(name, nid, MBSTRING_ASC,
                                   (unsigned char*)cname, -1, -1, 0)))
    goto error;
  if (!(X509_set_subject_name(x509, name)))
    goto error;

  if (!(name_issuer = X509_NAME_new()))
    goto error;
  if ((nid = OBJ_txt2nid("organizationName")) == NID_undef)
    goto error;
  if (!(X509_NAME_add_entry_by_NID(name_issuer, nid, MBSTRING_ASC,
                                   (unsigned char*)"Tor", -1, -1, 0)))
    goto error;
  if ((nid = OBJ_txt2nid("commonName")) == NID_undef) goto error;
  if (!(X509_NAME_add_entry_by_NID(name_issuer, nid, MBSTRING_ASC,
                              (unsigned char*)cname_sign, -1, -1, 0)))
    goto error;
  if (!(X509_set_issuer_name(x509, name_issuer)))
    goto error;

  if (!X509_time_adj(X509_get_notBefore(x509),0,&start_time))
    goto error;
  end_time = start_time + cert_lifetime;
  if (!X509_time_adj(X509_get_notAfter(x509),0,&end_time))
    goto error;
  if (!X509_set_pubkey(x509, pkey))
    goto error;
  if (!X509_sign(x509, sign_pkey, EVP_sha1()))
    goto error;

  goto done;
 error:
  if (x509) {
    X509_free(x509);
    x509 = NULL;
  }
 done:
  tls_log_errors(LOG_WARN, "generating certificate");
  if (sign_pkey)
    EVP_PKEY_free(sign_pkey);
  if (pkey)
    EVP_PKEY_free(pkey);
  if (name)
    X509_NAME_free(name);
  if (name_issuer)
    X509_NAME_free(name_issuer);
  return x509;
}

#ifdef EVERYONE_HAS_AES
/* Everybody is running OpenSSL 0.9.7 or later, so no backward compatibility
 * is needed. */
#define CIPHER_LIST TLS1_TXT_DHE_RSA_WITH_AES_128_SHA
#elif defined(TLS1_TXT_DHE_RSA_WITH_AES_128_SHA)
/* Some people are running OpenSSL before 0.9.7, but we aren't.
 * We can support AES and 3DES.
 */
#define CIPHER_LIST (TLS1_TXT_DHE_RSA_WITH_AES_128_SHA ":" \
                     SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA)
#else
/* We're running OpenSSL before 0.9.7. We only support 3DES. */
#define CIPHER_LIST SSL3_TXT_EDH_RSA_DES_192_CBC3_SHA
#endif

/** Create a new TLS context for use with Tor TLS handshakes.
 * <b>identity</b> should be set to the identity key used to sign the
 * certificate, and <b>nickname</b> set to the nickname to use.
 *
 * You can call this function multiple times.  Each time you call it,
 * it generates new certificates; all new connections will use
 * the new SSL context.
 */
int
tor_tls_context_new(crypto_pk_env_t *identity, const char *nickname,
                    unsigned int key_lifetime)
{
  crypto_pk_env_t *rsa = NULL;
  crypto_dh_env_t *dh = NULL;
  EVP_PKEY *pkey = NULL;
  tor_tls_context_t *result = NULL;
  X509 *cert = NULL, *idcert = NULL;
  char nn2[128];
  if (!nickname)
    nickname = "null";
  tor_snprintf(nn2, sizeof(nn2), "%s <identity>", nickname);

  tor_tls_init();

  /* Generate short-term RSA key. */
  if (!(rsa = crypto_new_pk_env()))
    goto error;
  if (crypto_pk_generate_key(rsa)<0)
    goto error;
  /* Create certificate signed by identity key. */
  cert = tor_tls_create_certificate(rsa, identity, nickname, nn2,
                                    key_lifetime);
  /* Create self-signed certificate for identity key. */
  idcert = tor_tls_create_certificate(identity, identity, nn2, nn2,
                                      IDENTITY_CERT_LIFETIME);
  if (!cert || !idcert) {
    log(LOG_WARN, LD_CRYPTO, "Error creating certificate");
    goto error;
  }

  result = tor_malloc_zero(sizeof(tor_tls_context_t));
#ifdef EVERYONE_HAS_AES
  /* Tell OpenSSL to only use TLS1 */
  if (!(result->ctx = SSL_CTX_new(TLSv1_method())))
    goto error;
#else
  /* Tell OpenSSL to use SSL3 or TLS1 but not SSL2. */
  if (!(result->ctx = SSL_CTX_new(SSLv23_method())))
    goto error;
  SSL_CTX_set_options(result->ctx, SSL_OP_NO_SSLv2);
#endif
  SSL_CTX_set_options(result->ctx, SSL_OP_SINGLE_DH_USE);
  if (!SSL_CTX_set_cipher_list(result->ctx, CIPHER_LIST))
    goto error;
  if (cert && !SSL_CTX_use_certificate(result->ctx,cert))
    goto error;
  X509_free(cert); /* We just added a reference to cert. */
  cert=NULL;
  if (idcert && !SSL_CTX_add_extra_chain_cert(result->ctx,idcert))
    goto error;
  idcert=NULL; /* The context now owns the reference to idcert */
  SSL_CTX_set_session_cache_mode(result->ctx, SSL_SESS_CACHE_OFF);
  tor_assert(rsa);
  if (!(pkey = _crypto_pk_env_get_evp_pkey(rsa,1)))
    goto error;
  if (!SSL_CTX_use_PrivateKey(result->ctx, pkey))
    goto error;
  EVP_PKEY_free(pkey);
  pkey = NULL;
  if (!SSL_CTX_check_private_key(result->ctx))
    goto error;
  dh = crypto_dh_new();
  SSL_CTX_set_tmp_dh(result->ctx, _crypto_dh_env_get_dh(dh));
  crypto_dh_free(dh);
  SSL_CTX_set_verify(result->ctx, SSL_VERIFY_PEER,
                     always_accept_verify_cb);
  /* let us realloc bufs that we're writing from */
  SSL_CTX_set_mode(result->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
  /* Free the old context if one exists. */
  if (global_tls_context) {
    /* This is safe even if there are open connections: OpenSSL does
     * reference counting with SSL and SSL_CTX objects. */
    SSL_CTX_free(global_tls_context->ctx);
    tor_free(global_tls_context);
  }
  global_tls_context = result;
  if (rsa)
    crypto_free_pk_env(rsa);
  return 0;

 error:
  tls_log_errors(LOG_WARN, "creating TLS context");
  if (pkey)
    EVP_PKEY_free(pkey);
  if (rsa)
    crypto_free_pk_env(rsa);
  if (dh)
    crypto_dh_free(dh);
  if (result && result->ctx)
    SSL_CTX_free(result->ctx);
  if (result)
    tor_free(result);
  if (cert)
    X509_free(cert);
  if (idcert)
    X509_free(idcert);
  return -1;
}

/** Create a new TLS object from a file descriptor, and a flag to
 * determine whether it is functioning as a server.
 */
tor_tls_t *
tor_tls_new(int sock, int isServer)
{
  BIO *bio = NULL;
  tor_tls_t *result = tor_malloc_zero(sizeof(tor_tls_t));

  tor_assert(global_tls_context); /* make sure somebody made it first */
  if (!(result->ssl = SSL_new(global_tls_context->ctx))) {
    tls_log_errors(LOG_WARN, "generating TLS context");
    tor_free(result);
    return NULL;
  }
  result->socket = sock;
#ifdef USE_BSOCKETS
  bio = BIO_new_bsocket(sock, BIO_NOCLOSE);
#else
  bio = BIO_new_socket(sock, BIO_NOCLOSE);
#endif
  if (! bio) {
    tls_log_errors(LOG_WARN, "opening BIO");
    tor_free(result);
    return NULL;
  }
  SSL_set_bio(result->ssl, bio, bio);
  result->state = TOR_TLS_ST_HANDSHAKE;
  result->isServer = isServer;
  result->wantwrite_n = 0;
  /* Not expected to get called. */
  tls_log_errors(LOG_WARN, "generating TLS context");
  return result;
}

/** Return whether this tls initiated the connect (client) or
 * received it (server). */
int
tor_tls_is_server(tor_tls_t *tls)
{
  tor_assert(tls);
  return tls->isServer;
}

/** Release resources associated with a TLS object.  Does not close the
 * underlying file descriptor.
 */
void
tor_tls_free(tor_tls_t *tls)
{
  tor_assert(tls && tls->ssl);
  SSL_free(tls->ssl);
  tls->ssl = NULL;
  tor_free(tls);
}

/** Underlying function for TLS reading.  Reads up to <b>len</b>
 * characters from <b>tls</b> into <b>cp</b>.  On success, returns the
 * number of characters read.  On failure, returns TOR_TLS_ERROR,
 * TOR_TLS_CLOSE, TOR_TLS_WANTREAD, or TOR_TLS_WANTWRITE.
 */
int
tor_tls_read(tor_tls_t *tls, char *cp, size_t len)
{
  int r, err;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->state == TOR_TLS_ST_OPEN);
  r = SSL_read(tls->ssl, cp, len);
  if (r > 0)
    return r;
  err = tor_tls_get_error(tls, r, CATCH_ZERO, "reading", LOG_DEBUG);
  if (err == _TOR_TLS_ZERORETURN) {
    log_debug(LD_NET,"read returned r=%d; TLS is closed",r);
    tls->state = TOR_TLS_ST_CLOSED;
    return TOR_TLS_CLOSE;
  } else {
    tor_assert(err != TOR_TLS_DONE);
    log_debug(LD_NET,"read returned r=%d, err=%d",r,err);
    return err;
  }
}

/** Underlying function for TLS writing.  Write up to <b>n</b>
 * characters from <b>cp</b> onto <b>tls</b>.  On success, returns the
 * number of characters written.  On failure, returns TOR_TLS_ERROR,
 * TOR_TLS_WANTREAD, or TOR_TLS_WANTWRITE.
 */
int
tor_tls_write(tor_tls_t *tls, const char *cp, size_t n)
{
  int r, err;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->state == TOR_TLS_ST_OPEN);
  if (n == 0)
    return 0;
  if (tls->wantwrite_n) {
    /* if WANTWRITE last time, we must use the _same_ n as before */
    tor_assert(n >= tls->wantwrite_n);
    log_debug(LD_NET,"resuming pending-write, (%d to flush, reusing %d)",
              (int)n, (int)tls->wantwrite_n);
    n = tls->wantwrite_n;
    tls->wantwrite_n = 0;
  }
  r = SSL_write(tls->ssl, cp, n);
  err = tor_tls_get_error(tls, r, 0, "writing", LOG_INFO);
  if (err == TOR_TLS_DONE) {
    return r;
  }
  if (err == TOR_TLS_WANTWRITE || err == TOR_TLS_WANTREAD) {
    tls->wantwrite_n = n;
  }
  return err;
}

/** Perform initial handshake on <b>tls</b>.  When finished, returns
 * TOR_TLS_DONE.  On failure, returns TOR_TLS_ERROR, TOR_TLS_WANTREAD,
 * or TOR_TLS_WANTWRITE.
 */
int
tor_tls_handshake(tor_tls_t *tls)
{
  int r;
  tor_assert(tls);
  tor_assert(tls->ssl);
  tor_assert(tls->state == TOR_TLS_ST_HANDSHAKE);
  check_no_tls_errors();
  if (tls->isServer) {
    r = SSL_accept(tls->ssl);
  } else {
    r = SSL_connect(tls->ssl);
  }
  r = tor_tls_get_error(tls,r,0, "handshaking", LOG_INFO);
  if (ERR_peek_error() != 0) {
    tls_log_errors(tls->isServer ? LOG_INFO : LOG_WARN,
                   "handshaking");
    return TOR_TLS_ERROR_MISC;
  }
  if (r == TOR_TLS_DONE) {
    tls->state = TOR_TLS_ST_OPEN;
  }
  return r;
}

/** Shut down an open tls connection <b>tls</b>.  When finished, returns
 * TOR_TLS_DONE.  On failure, returns TOR_TLS_ERROR, TOR_TLS_WANTREAD,
 * or TOR_TLS_WANTWRITE.
 */
int
tor_tls_shutdown(tor_tls_t *tls)
{
  int r, err;
  char buf[128];
  tor_assert(tls);
  tor_assert(tls->ssl);

  while (1) {
    if (tls->state == TOR_TLS_ST_SENTCLOSE) {
      /* If we've already called shutdown once to send a close message,
       * we read until the other side has closed too.
       */
      do {
        r = SSL_read(tls->ssl, buf, 128);
      } while (r>0);
      err = tor_tls_get_error(tls, r, CATCH_ZERO, "reading to shut down",
                              LOG_INFO);
      if (err == _TOR_TLS_ZERORETURN) {
        tls->state = TOR_TLS_ST_GOTCLOSE;
        /* fall through... */
      } else {
        return err;
      }
    }

    r = SSL_shutdown(tls->ssl);
    if (r == 1) {
      /* If shutdown returns 1, the connection is entirely closed. */
      tls->state = TOR_TLS_ST_CLOSED;
      return TOR_TLS_DONE;
    }
    err = tor_tls_get_error(tls, r, CATCH_SYSCALL|CATCH_ZERO, "shutting down",
                            LOG_INFO);
    if (err == _TOR_TLS_SYSCALL) {
      /* The underlying TCP connection closed while we were shutting down. */
      tls->state = TOR_TLS_ST_CLOSED;
      return TOR_TLS_DONE;
    } else if (err == _TOR_TLS_ZERORETURN) {
      /* The TLS connection says that it sent a shutdown record, but
       * isn't done shutting down yet.  Make sure that this hasn't
       * happened before, then go back to the start of the function
       * and try to read.
       */
      if (tls->state == TOR_TLS_ST_GOTCLOSE ||
         tls->state == TOR_TLS_ST_SENTCLOSE) {
        log(LOG_WARN, LD_NET,
            "TLS returned \"half-closed\" value while already half-closed");
        return TOR_TLS_ERROR_MISC;
      }
      tls->state = TOR_TLS_ST_SENTCLOSE;
      /* fall through ... */
    } else {
      return err;
    }
  } /* end loop */
}

/** Return true iff this TLS connection is authenticated.
 */
int
tor_tls_peer_has_cert(tor_tls_t *tls)
{
  X509 *cert;
  cert = SSL_get_peer_certificate(tls->ssl);
  tls_log_errors(LOG_WARN, "getting peer certificate");
  if (!cert)
    return 0;
  X509_free(cert);
  return 1;
}

/** Warn that a certificate lifetime extends through a certain range. */
static void
log_cert_lifetime(X509 *cert, const char *problem)
{
  BIO *bio = NULL;
  BUF_MEM *buf;
  char *s1=NULL, *s2=NULL;
  char mytime[33];
  time_t now = time(NULL);
  struct tm tm;

  if (problem)
    log_warn(LD_GENERAL,
             "Certificate %s: is your system clock set incorrectly?",
             problem);

  if (!(bio = BIO_new(BIO_s_mem()))) {
    log_warn(LD_GENERAL, "Couldn't allocate BIO!"); goto end;
  }
  if (!(ASN1_TIME_print(bio, X509_get_notBefore(cert)))) {
    tls_log_errors(LOG_WARN, "printing certificate lifetime");
    goto end;
  }
  BIO_get_mem_ptr(bio, &buf);
  s1 = tor_strndup(buf->data, buf->length);

  (void)BIO_reset(bio);
  if (!(ASN1_TIME_print(bio, X509_get_notAfter(cert)))) {
    tls_log_errors(LOG_WARN, "printing certificate lifetime");
    goto end;
  }
  BIO_get_mem_ptr(bio, &buf);
  s2 = tor_strndup(buf->data, buf->length);

  strftime(mytime, 32, "%b %d %H:%M:%S %Y GMT", tor_gmtime_r(&now, &tm));

  log_warn(LD_GENERAL,
           "(certificate lifetime runs from %s through %s. Your time is %s.)",
           s1,s2,mytime);

 end:
  /* Not expected to get invoked */
  tls_log_errors(LOG_WARN, "getting certificate lifetime");
  if (bio)
    BIO_free(bio);
  if (s1)
    tor_free(s1);
  if (s2)
    tor_free(s2);
}

/** If the provided tls connection is authenticated and has a
 * certificate that is currently valid and signed, then set
 * *<b>identity_key</b> to the identity certificate's key and return
 * 0.  Else, return -1 and log complaints with log-level <b>severity</b>.
 */
int
tor_tls_verify(int severity, tor_tls_t *tls, crypto_pk_env_t **identity_key)
{
  X509 *cert = NULL, *id_cert = NULL;
  STACK_OF(X509) *chain = NULL;
  EVP_PKEY *id_pkey = NULL;
  RSA *rsa;
  int num_in_chain;
  int r = -1, i;

  *identity_key = NULL;

  if (!(cert = SSL_get_peer_certificate(tls->ssl)))
    goto done;
  if (!(chain = SSL_get_peer_cert_chain(tls->ssl)))
    goto done;
  num_in_chain = sk_X509_num(chain);
  /* 1 means we're receiving (server-side), and it's just the id_cert.
   * 2 means we're connecting (client-side), and it's both the link
   * cert and the id_cert.
   */
  if (num_in_chain < 1) {
    log_fn(severity,LD_PROTOCOL,
           "Unexpected number of certificates in chain (%d)",
           num_in_chain);
    goto done;
  }
  for (i=0; i<num_in_chain; ++i) {
    id_cert = sk_X509_value(chain, i);
    if (X509_cmp(id_cert, cert) != 0)
      break;
  }
  if (!id_cert) {
    log_fn(severity,LD_PROTOCOL,"No distinct identity certificate found");
    goto done;
  }

  if (!(id_pkey = X509_get_pubkey(id_cert)) ||
      X509_verify(cert, id_pkey) <= 0) {
    log_fn(severity,LD_PROTOCOL,"X509_verify on cert and pkey returned <= 0");
    tls_log_errors(severity,"verifying certificate");
    goto done;
  }

  rsa = EVP_PKEY_get1_RSA(id_pkey);
  if (!rsa)
    goto done;
  *identity_key = _crypto_new_pk_env_rsa(rsa);

  r = 0;

 done:
  if (cert)
    X509_free(cert);
  if (id_pkey)
    EVP_PKEY_free(id_pkey);

  /* This should never get invoked, but let's make sure in case OpenSSL
   * acts unexpectedly. */
  tls_log_errors(LOG_WARN, "finishing tor_tls_verify");

  return r;
}

/** Check whether the certificate set on the connection <b>tls</b> is
 * expired or not-yet-valid, give or take <b>tolerance</b>
 * seconds. Return 0 for valid, -1 for failure.
 *
 * NOTE: you should call tor_tls_verify before tor_tls_check_lifetime.
 */
int
tor_tls_check_lifetime(tor_tls_t *tls, int tolerance)
{
  time_t now, t;
  X509 *cert;
  int r = -1;

  now = time(NULL);

  if (!(cert = SSL_get_peer_certificate(tls->ssl)))
    goto done;

  t = now + tolerance;
  if (X509_cmp_time(X509_get_notBefore(cert), &t) > 0) {
    log_cert_lifetime(cert, "not yet valid");
    goto done;
  }
  t = now - tolerance;
  if (X509_cmp_time(X509_get_notAfter(cert), &t) < 0) {
    log_cert_lifetime(cert, "already expired");
    goto done;
  }

  r = 0;
 done:
  if (cert)
    X509_free(cert);
  /* Not expected to get invoked */
  tls_log_errors(LOG_WARN, "checking certificate lifetime");

  return r;
}

/** Return the number of bytes available for reading from <b>tls</b>.
 */
int
tor_tls_get_pending_bytes(tor_tls_t *tls)
{
  tor_assert(tls);
#if OPENSSL_VERSION_NUMBER < 0x0090700fl
  if (tls->ssl->rstate == SSL_ST_READ_BODY)
    return 0;
  if (tls->ssl->s3->rrec.type != SSL3_RT_APPLICATION_DATA)
    return 0;
#endif
  return SSL_pending(tls->ssl);
}

/** If <b>tls</b> requires that the next write be of a particular size,
 * return that size.  Otherwise, return 0. */
size_t
tor_tls_get_forced_write_size(tor_tls_t *tls)
{
  return tls->wantwrite_n;
}

/** Sets n_read and n_written to the number of bytes read and written,
 * respectivey, on the raw socket used by <b>tls</b> since the last time this
 * function was called on <b>tls</b>. */
void
tor_tls_get_n_raw_bytes(tor_tls_t *tls, size_t *n_read, size_t *n_written)
{
  unsigned long r, w;
  r = BIO_number_read(SSL_get_rbio(tls->ssl));
  w = BIO_number_written(SSL_get_wbio(tls->ssl));

  /* We are ok with letting these unsigned ints go "negative" here:
   * If we wrapped around, this should still give us the right answer, unless
   * we wrapped around by more than ULONG_MAX since the last time we called
   * this function.
   */

  *n_read = (size_t)(r - tls->last_read_count);
  *n_written = (size_t)(w - tls->last_write_count);
  tls->last_read_count = r;
  tls->last_write_count = w;
}

/** Implement check_no_tls_errors: If there are any pending OpenSSL
 * errors, log an error message. */
void
_check_no_tls_errors(const char *fname, int line)
{
  if (ERR_peek_error() == 0)
    return;
  log(LOG_WARN, LD_CRYPTO, "Unhandled OpenSSL errors found at %s:%d: ",
      tor_fix_source_file(fname), line);
  tls_log_errors(LOG_WARN, NULL);
}

