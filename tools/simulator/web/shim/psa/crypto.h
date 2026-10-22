#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
int sim_pbkdf2(const unsigned char*,size_t,const unsigned char*,size_t,unsigned,unsigned char*,size_t);
typedef int psa_status_t;
typedef struct{unsigned iterations;uint8_t salt[16],password[5];size_t salt_len,password_len;} psa_key_derivation_operation_t;
#define PSA_SUCCESS 0
#define PSA_KEY_DERIVATION_OPERATION_INIT {0}
#define PSA_ALG_SHA_256 1
#define PSA_ALG_PBKDF2_HMAC(x) 1
#define PSA_KEY_DERIVATION_INPUT_COST 1
#define PSA_KEY_DERIVATION_INPUT_SALT 2
#define PSA_KEY_DERIVATION_INPUT_PASSWORD 3
static inline int psa_crypto_init(void){return 0;}
static inline int psa_key_derivation_setup(psa_key_derivation_operation_t *p,int a){(void)a;memset(p,0,sizeof(*p));return 0;}
static inline int psa_key_derivation_input_integer(psa_key_derivation_operation_t*p,int key,unsigned n){(void)key;p->iterations=n;return 0;}
static inline int psa_key_derivation_input_bytes(psa_key_derivation_operation_t*p,int key,const uint8_t*b,size_t n){if(key==2){if(n>16)return -1;memcpy(p->salt,b,n);p->salt_len=n;}else{if(n>5)return -1;memcpy(p->password,b,n);p->password_len=n;}return 0;}
static inline int psa_key_derivation_output_bytes(psa_key_derivation_operation_t*p,uint8_t*out,size_t n){return sim_pbkdf2(p->password,p->password_len,p->salt,p->salt_len,p->iterations,out,n);}
static inline int psa_key_derivation_abort(psa_key_derivation_operation_t*p){memset(p,0,sizeof(*p));return 0;}
