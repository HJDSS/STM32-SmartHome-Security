#ifndef __AES128_H
#define __AES128_H

#include "sys.h"

typedef struct
{
		u8 rk[176];
}AES128_CTX;

void AES128_KeyInit(AES128_CTX *ctx,const u8 key[16]);
void AES128_EncryptBlock(const AES128_CTX *ctx,const u8 in[16],u8 out[16]);
void AES128_CTR_XCrypt(const AES128_CTX *ctx,const u8 nonce[16],u32 counter,u8 *buf,u32 len);

#endif

