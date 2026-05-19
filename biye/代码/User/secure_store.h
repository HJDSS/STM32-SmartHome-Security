#ifndef __SECURE_STORE_H
#define __SECURE_STORE_H

#include "sys.h"

#define SECSTORE_OK            0
#define SECSTORE_ERR           1

typedef struct
{
		u8 user_pwd[6];
		u8 admin_pwd[6];
		u8 role_enable;      // 1=启用权限控制
		u8 reserved;
		u16 admin_flags;     // 预留：指纹管理/系统配置权限位
}SECURE_CFG_T;

u8 SecStore_Init(void);
u8 SecStore_ReadCfg(SECURE_CFG_T *cfg);
u8 SecStore_WriteCfg(const SECURE_CFG_T *cfg);
void SecStore_DeriveKey(u8 key_out[16]);

#endif

