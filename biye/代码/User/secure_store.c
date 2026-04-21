#include "secure_store.h"
#include "stmflash.h"
#include "aes128.h"
/* NULL 定义（用于判断空指针） */
#include <stddef.h>

#include "stmflash.h"
/* 将安全配置放在 Flash 末尾，避免固定地址在不同容量芯片上越界读导致 HardFault。
 * 记录大小 < 256B，预留 256B 空间即可。
 * 依赖 stmflash.h 中 STM32_FLASH_SIZE（单位 KB）配置正确。
 */
#define SECSTORE_ADDR  (STM32_FLASH_BASE + (u32)(1024u * (u32)STM32_FLASH_SIZE) - 256u)
#define SECSTORE_MAGIC0        0x53
#define SECSTORE_MAGIC1        0x43
#define SECSTORE_MAGIC2        0x46
#define SECSTORE_MAGIC3        0x31
#define SECSTORE_VER           0x01

typedef struct
{
		u8 magic[4];
		u8 ver;
		u8 nonce[16];
		u32 counter;
		u16 crc;
		u8 cipher[32];
}SECSTORE_RECORD_T;

static u16 crc16_ccitt(const u8 *data,u32 len)
{
		u16 crc=0xFFFF;
		u32 i;
		u8 j;
		for(i=0;i<len;i++)
		{
				crc ^= ((u16)data[i])<<8;
				for(j=0;j<8;j++)
				{
						if(crc & 0x8000) crc = (crc<<1) ^ 0x1021;
						else crc <<= 1;
				}
		}
		return crc;
}

static void read_uid(u8 uid12[12])
{
		u32 *UID = (u32*)0x1FFFF7E8;
		u32 a=UID[0],b=UID[1],c=UID[2];
		uid12[0]=(u8)a; uid12[1]=(u8)(a>>8); uid12[2]=(u8)(a>>16); uid12[3]=(u8)(a>>24);
		uid12[4]=(u8)b; uid12[5]=(u8)(b>>8); uid12[6]=(u8)(b>>16); uid12[7]=(u8)(b>>24);
		uid12[8]=(u8)c; uid12[9]=(u8)(c>>8); uid12[10]=(u8)(c>>16); uid12[11]=(u8)(c>>24);
}

void SecStore_DeriveKey(u8 key_out[16])
{
		static const u8 master[16]={0x21,0x33,0x55,0x87,0x19,0xAB,0xCD,0xEF,0x10,0x32,0x54,0x76,0x98,0xBA,0xDC,0xFE};
		u8 uid[12];
		u8 in[16];
		AES128_CTX ctx;

		read_uid(uid);
		in[0]=uid[0]^0xA5;  in[1]=uid[1]^0x5A;  in[2]=uid[2]^0x3C;  in[3]=uid[3]^0xC3;
		in[4]=uid[4]^0xF0;  in[5]=uid[5]^0x0F;  in[6]=uid[6]^0x96;  in[7]=uid[7]^0x69;
		in[8]=uid[8]^0x12;  in[9]=uid[9]^0x34;  in[10]=uid[10]^0x56; in[11]=uid[11]^0x78;
		in[12]=0x01; in[13]=0x02; in[14]=0x03; in[15]=0x04;

		AES128_KeyInit(&ctx,master);
		AES128_EncryptBlock(&ctx,in,key_out);
}

static void build_nonce(u8 nonce[16],u32 counter)
{
		u8 uid[12];
		read_uid(uid);
		nonce[0]=uid[0]; nonce[1]=uid[1]; nonce[2]=uid[2]; nonce[3]=uid[3];
		nonce[4]=uid[4]; nonce[5]=uid[5]; nonce[6]=uid[6]; nonce[7]=uid[7];
		nonce[8]=uid[8]; nonce[9]=uid[9]; nonce[10]=uid[10]; nonce[11]=uid[11];
		nonce[12]=(u8)(counter>>24);
		nonce[13]=(u8)(counter>>16);
		nonce[14]=(u8)(counter>>8);
		nonce[15]=(u8)(counter);
}

u8 SecStore_Init(void)
{
		SECURE_CFG_T cfg;
		if(SecStore_ReadCfg(&cfg)==SECSTORE_OK) return SECSTORE_OK;

		cfg.user_pwd[0]=1;cfg.user_pwd[1]=2;cfg.user_pwd[2]=6;cfg.user_pwd[3]=4;cfg.user_pwd[4]=8;cfg.user_pwd[5]=7;
		cfg.admin_pwd[0]=2;cfg.admin_pwd[1]=0;cfg.admin_pwd[2]=0;cfg.admin_pwd[3]=1;cfg.admin_pwd[4]=3;cfg.admin_pwd[5]=6;
		cfg.role_enable=1;
		cfg.reserved=0;
		cfg.admin_flags=0xFFFF;
		return SecStore_WriteCfg(&cfg);
}

u8 SecStore_ReadCfg(SECURE_CFG_T *cfg)
{
		SECSTORE_RECORD_T rec;
		u8 key[16];
		AES128_CTX ctx;
		u16 crc;
		u8 plain[32];
		u16 *p16=(u16*)&rec;
		u16 i;

		if(cfg==NULL) return SECSTORE_ERR;
		for(i=0;i<sizeof(SECSTORE_RECORD_T)/2;i++) p16[i]=0;
		STMFLASH_Read(SECSTORE_ADDR,(u16*)&rec,(sizeof(SECSTORE_RECORD_T)+1)/2);
		if(rec.magic[0]!=SECSTORE_MAGIC0 || rec.magic[1]!=SECSTORE_MAGIC1 || rec.magic[2]!=SECSTORE_MAGIC2 || rec.magic[3]!=SECSTORE_MAGIC3) return SECSTORE_ERR;
		if(rec.ver!=SECSTORE_VER) return SECSTORE_ERR;

		for(i=0;i<32;i++) plain[i]=rec.cipher[i];
		SecStore_DeriveKey(key);
		AES128_KeyInit(&ctx,key);
		AES128_CTR_XCrypt(&ctx,rec.nonce,rec.counter,plain,32);

		crc = crc16_ccitt(plain,32);
		if(crc != rec.crc) return SECSTORE_ERR;

		for(i=0;i<6;i++) cfg->user_pwd[i]=plain[i];
		for(i=0;i<6;i++) cfg->admin_pwd[i]=plain[6+i];
		cfg->role_enable = plain[12];
		cfg->reserved = plain[13];
		cfg->admin_flags = (u16)(plain[14] | ((u16)plain[15]<<8));
		return SECSTORE_OK;
}

u8 SecStore_WriteCfg(const SECURE_CFG_T *cfg)
{
		SECSTORE_RECORD_T rec;
		u8 key[16];
		AES128_CTX ctx;
		u8 plain[32];
		u16 i;
		static u32 counter = 0;

		if(cfg==NULL) return SECSTORE_ERR;

		for(i=0;i<32;i++) plain[i]=0;
		for(i=0;i<6;i++) plain[i]=cfg->user_pwd[i];
		for(i=0;i<6;i++) plain[6+i]=cfg->admin_pwd[i];
		plain[12]=cfg->role_enable;
		plain[13]=cfg->reserved;
		plain[14]=(u8)(cfg->admin_flags);
		plain[15]=(u8)(cfg->admin_flags>>8);

		rec.magic[0]=SECSTORE_MAGIC0; rec.magic[1]=SECSTORE_MAGIC1; rec.magic[2]=SECSTORE_MAGIC2; rec.magic[3]=SECSTORE_MAGIC3;
		rec.ver = SECSTORE_VER;
		rec.counter = counter++;
		build_nonce(rec.nonce,rec.counter);

		SecStore_DeriveKey(key);
		AES128_KeyInit(&ctx,key);
		for(i=0;i<32;i++) rec.cipher[i]=plain[i];
		AES128_CTR_XCrypt(&ctx,rec.nonce,rec.counter,rec.cipher,32);
		rec.crc = crc16_ccitt(plain,32);

		STMFLASH_Write(SECSTORE_ADDR,(u16*)&rec,(sizeof(SECSTORE_RECORD_T)+1)/2);
		return SECSTORE_OK;
}

