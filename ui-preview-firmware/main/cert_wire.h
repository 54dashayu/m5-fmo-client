#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cJSON.h"
// Portable wire validation, also built on the host for adversarial tests.
void wire_b64(const uint8_t *in,size_t n,char *out);
bool wire_unb64(const char *in,uint8_t *out,size_t expected);
bool wire_callsign(const char *s);
size_t wire_request_tbs(uint8_t *out,size_t cap,const char *call,const uint8_t pk[32],const char *nonce);
const char *wire_validate_bundle(const cJSON *bundle,const uint8_t own_pk[32],const char *own_call,int64_t now);
size_t wire_cert_tbs(const cJSON *cert,uint8_t *out,size_t cap);
size_t wire_login_tbs(uint8_t *out,size_t cap,int64_t server_uid,const char *server_call,const char *host,int port,const uint8_t server_fp[32],int64_t now,const uint8_t user_fp[32]);
