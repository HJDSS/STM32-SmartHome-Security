#include "secure_store.h"
#include "stmflash.h"
#include <stddef.h>
#include "board_config.h"

/* Place secure configuration at the end of Flash to avoid crossing
 * capacity boundaries on different chip variants.
 * Record size < 256B; reserve 256B.
 * Relies on correct STM32_FLASH_SIZE (KB) in stmflash.h.
 */
#define SECSTORE_ADDR  (STM32_FLASH_BASE + (u32)(1024u * (u32)STM32_FLASH_SIZE) - 256u)
#define SECSTORE_MAGIC0        0x53
#define SECSTORE_MAGIC1        0x43
#define SECSTORE_MAGIC2        0x46
#define SECSTORE_MAGIC3        0x31

#if APP_SECURE_AES_ENABLE
/* ==================================================================
 * AES-128-CBC + HMAC-AES-CBC-MAC + dual-key window (thesis scheme)
 * ================================================================== */

#include "aes128.h"

#define SECSTORE_VER           0x02  /* CBC+HMAC crypto scheme */

/*
 * On-flash record layout (73 bytes):
 *   magic[4] + ver[1] + iv[16] + counter[4] + mac[16] + cipher[32]
 */
typedef struct
{
		u8 magic[4];
		u8 ver;
		u8 iv[16];       /* CBC initialisation vector */
		u32 counter;      /* replay-protection monotonic counter */
		u8 mac[16];       /* HMAC-AES-CBC-MAC authentication tag over plaintext */
		u8 cipher[32];    /* AES-128-CBC ciphertext of SECURE_CFG_T */
}SECSTORE_RECORD_T;

/* ========== Dual-key window mechanism ========== */
#define SECURE_KEY_SLOTS 2
static u8 s_key[SECURE_KEY_SLOTS][16];  /* slot 0 = current, slot 1 = previous */
static u8 s_key_version;                /* incremented on each rotation       */

static void read_uid(u8 uid12[12])
{
		u32 *UID = (u32*)0x1FFFF7E8;
		u32 a=UID[0],b=UID[1],c=UID[2];
		uid12[0]=(u8)a; uid12[1]=(u8)(a>>8); uid12[2]=(u8)(a>>16); uid12[3]=(u8)(a>>24);
		uid12[4]=(u8)b; uid12[5]=(u8)(b>>8); uid12[6]=(u8)(b>>16); uid12[7]=(u8)(b>>24);
		uid12[8]=(u8)c; uid12[9]=(u8)(c>>8); uid12[10]=(u8)(c>>16); uid12[11]=(u8)(c>>24);
}

/* -------- Key derivation with chip-UID binding -------- */

static void derive_key_version(u8 key_out[16], u8 version)
{
		static const u8 master[16]={0x21,0x33,0x55,0x87,0x19,0xAB,0xCD,0xEF,0x10,0x32,0x54,0x76,0x98,0xBA,0xDC,0xFE};
		u8 uid[12];
		u8 in[16];
		AES128_CTX ctx;

		read_uid(uid);
		in[0]=uid[0]^0xA5;  in[1]=uid[1]^0x5A;  in[2]=uid[2]^0x3C;  in[3]=uid[3]^0xC3;
		in[4]=uid[4]^0xF0;  in[5]=uid[5]^0x0F;  in[6]=uid[6]^0x96;  in[7]=uid[7]^0x69;
		in[8]=uid[8]^0x12;  in[9]=uid[9]^0x34;  in[10]=uid[10]^0x56; in[11]=uid[11]^0x78;
		in[12]=0x01; in[13]=0x02; in[14]=0x03; in[15]=version;

		AES128_KeyInit(&ctx,master);
		AES128_EncryptBlock(&ctx,in,key_out);
}

void SecStore_DeriveKey(u8 key_out[16])
{
		derive_key_version(key_out, 0);
}

static void init_dual_keys(void)
{
		derive_key_version(s_key[0], s_key_version);
		{
				u8 i;
				for(i=0;i<16;i++) s_key[1][i]=s_key[0][i];
		}
}

static void rotate_key_window(void)
{
		u8 i;
		s_key_version++;
		for(i=0;i<16;i++) s_key[1][i]=s_key[0][i];
		derive_key_version(s_key[0], s_key_version);
}

/* -------- IV construction -------- */

static void build_iv(u8 iv[16], u32 counter)
{
		u8 uid[12];
		read_uid(uid);
		iv[0]=uid[0]; iv[1]=uid[1]; iv[2]=uid[2]; iv[3]=uid[3];
		iv[4]=uid[4]; iv[5]=uid[5]; iv[6]=uid[6]; iv[7]=uid[7];
		iv[8]=uid[8]; iv[9]=uid[9]; iv[10]=uid[10]; iv[11]=uid[11];
		iv[12]=(u8)(counter>>24);
		iv[13]=(u8)(counter>>16);
		iv[14]=(u8)(counter>>8);
		iv[15]=(u8)(counter);
}

/* -------- HMAC integrity check (single key) -------- */

static u8 verify_hmac_single(const u8 *key, const u8 plain[32], const u8 mac_tag[16])
{
		u8 computed[16];
		u8 i;
		AES128_ComputeHMAC(key, plain, 32, computed);
		for(i=0;i<16;i++) { if(computed[i]!=mac_tag[i]) return 1; }
		return 0;
}

/* -------- Public API -------- */

u8 SecStore_Init(void)
{
		SECURE_CFG_T cfg;

		s_key_version = 0;
		init_dual_keys();

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
		u8 plain[32];
		u8 iv[16];
		u16 i;
		u16 *p16=(u16*)&rec;

		if(cfg==NULL) return SECSTORE_ERR;

		for(i=0;i<sizeof(SECSTORE_RECORD_T)/2;i++) p16[i]=0;

		STMFLASH_Read(SECSTORE_ADDR,(u16*)&rec,(sizeof(SECSTORE_RECORD_T)+1)/2);

		if(rec.magic[0]!=SECSTORE_MAGIC0 || rec.magic[1]!=SECSTORE_MAGIC1 ||
		   rec.magic[2]!=SECSTORE_MAGIC2 || rec.magic[3]!=SECSTORE_MAGIC3) return SECSTORE_ERR;
		if(rec.ver!=SECSTORE_VER) return SECSTORE_ERR;

		/* Try current key first */
		{
				u8 j;
				for(j=0;j<16;j++) iv[j]=rec.iv[j];
				for(j=0;j<32;j++) plain[j]=rec.cipher[j];
				AES128_CBC_Decrypt(s_key[0], iv, plain, 32);

				if(verify_hmac_single(s_key[0], plain, rec.mac))
				{
						/* Current key failed — try previous key */
						for(j=0;j<16;j++) iv[j]=rec.iv[j];
						for(j=0;j<32;j++) plain[j]=rec.cipher[j];
						AES128_CBC_Decrypt(s_key[1], iv, plain, 32);

						if(verify_hmac_single(s_key[1], plain, rec.mac))
								return SECSTORE_ERR;
				}
		}

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

		rec.magic[0]=SECSTORE_MAGIC0; rec.magic[1]=SECSTORE_MAGIC1;
		rec.magic[2]=SECSTORE_MAGIC2; rec.magic[3]=SECSTORE_MAGIC3;
		rec.ver = SECSTORE_VER;
		rec.counter = counter++;
		build_iv(rec.iv, rec.counter);

		for(i=0;i<32;i++) rec.cipher[i]=plain[i];
		{
				u8 iv_copy[16];
				for(i=0;i<16;i++) iv_copy[i]=rec.iv[i];
				AES128_CBC_Encrypt(s_key[0], iv_copy, rec.cipher, 32);
		}

		AES128_ComputeHMAC(s_key[0], plain, 32, rec.mac);

		STMFLASH_Write(SECSTORE_ADDR,(u16*)&rec,(sizeof(SECSTORE_RECORD_T)+1)/2);
		return SECSTORE_OK;
}

#else  /* !APP_SECURE_AES_ENABLE */
/* ==================================================================
 * Plaintext fallback: no encryption, CRC16 for basic integrity only.
 * Kept minimal for debug builds or when AES is disabled.
 * ================================================================== */

#define SECSTORE_VER           0x10  /* plaintext fallback version */

typedef struct
{
		u8 magic[4];
		u8 ver;
		u16 crc;         /* CRC16-CCITT over plain data */
		u8 data[16];     /* SECURE_CFG_T raw fields */
}SECSTORE_RECORD_T;

static u16 crc16_ccitt(const u8 *data, u32 len)
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

void SecStore_DeriveKey(u8 key_out[16])
{
		/* No-op in plaintext mode: fill with zeros */
		{
				u8 i;
				for(i=0;i<16;i++) key_out[i]=0;
		}
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
		u16 crc;
		u16 i;
		u16 *p16=(u16*)&rec;

		if(cfg==NULL) return SECSTORE_ERR;

		for(i=0;i<sizeof(SECSTORE_RECORD_T)/2;i++) p16[i]=0;

		STMFLASH_Read(SECSTORE_ADDR,(u16*)&rec,(sizeof(SECSTORE_RECORD_T)+1)/2);

		if(rec.magic[0]!=SECSTORE_MAGIC0 || rec.magic[1]!=SECSTORE_MAGIC1 ||
		   rec.magic[2]!=SECSTORE_MAGIC2 || rec.magic[3]!=SECSTORE_MAGIC3) return SECSTORE_ERR;
		if(rec.ver!=SECSTORE_VER) return SECSTORE_ERR;

		crc = crc16_ccitt(rec.data, 16);
		if(crc != rec.crc) return SECSTORE_ERR;

		for(i=0;i<6;i++) cfg->user_pwd[i]=rec.data[i];
		for(i=0;i<6;i++) cfg->admin_pwd[i]=rec.data[6+i];
		cfg->role_enable = rec.data[12];
		cfg->reserved = rec.data[13];
		cfg->admin_flags = (u16)(rec.data[14] | ((u16)rec.data[15]<<8));
		return SECSTORE_OK;
}

u8 SecStore_WriteCfg(const SECURE_CFG_T *cfg)
{
		SECSTORE_RECORD_T rec;
		u16 i;

		if(cfg==NULL) return SECSTORE_ERR;

		for(i=0;i<16;i++) rec.data[i]=0;
		for(i=0;i<6;i++) rec.data[i]=cfg->user_pwd[i];
		for(i=0;i<6;i++) rec.data[6+i]=cfg->admin_pwd[i];
		rec.data[12]=cfg->role_enable;
		rec.data[13]=cfg->reserved;
		rec.data[14]=(u8)(cfg->admin_flags);
		rec.data[15]=(u8)(cfg->admin_flags>>8);

		rec.magic[0]=SECSTORE_MAGIC0; rec.magic[1]=SECSTORE_MAGIC1;
		rec.magic[2]=SECSTORE_MAGIC2; rec.magic[3]=SECSTORE_MAGIC3;
		rec.ver = SECSTORE_VER;
		rec.crc = crc16_ccitt(rec.data, 16);

		STMFLASH_Write(SECSTORE_ADDR,(u16*)&rec,(sizeof(SECSTORE_RECORD_T)+1)/2);
		return SECSTORE_OK;
}

#endif /* APP_SECURE_AES_ENABLE */
