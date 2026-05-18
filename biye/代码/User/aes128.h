#ifndef __AES128_H
#define __AES128_H

#include "sys.h"

typedef struct
{
		u8 rk[176];
}AES128_CTX;

void AES128_KeyInit(AES128_CTX *ctx,const u8 key[16]);
void AES128_EncryptBlock(const AES128_CTX *ctx,const u8 in[16],u8 out[16]);
void AES128_DecryptBlock(const AES128_CTX *ctx,const u8 in[16],u8 out[16]);
void AES128_CTR_XCrypt(const AES128_CTX *ctx,const u8 nonce[16],u32 counter,u8 *buf,u32 len);

/* AES-128-CBC mode. iv is modified in-place to the last ciphertext block.
 * plain_len / cipher_len must be a multiple of 16. */
void AES128_CBC_Encrypt(const u8 *key, u8 *iv, u8 *plain, u16 plain_len);
void AES128_CBC_Decrypt(const u8 *key, u8 *iv, u8 *cipher, u16 cipher_len);

/* HMAC-AES-CBC-MAC: keyed authentication tag (16 bytes) over arbitrary data.
 * Uses AES-CBC-MAC as the hash primitive with ipad/opad key derivation. */
void AES128_ComputeHMAC(const u8 *key, const u8 *data, u32 len, u8 mac_out[16]);

#endif

