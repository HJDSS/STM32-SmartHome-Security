#ifndef __FLASH_STORE_H
#define __FLASH_STORE_H

#include "app_types.h"

void FlashStore_Init(void);
void FlashStore_ReadPasswords(u8 *user_pwd, u8 *admin_pwd);
void FlashStore_WriteUserPassword(const u8 *pwd6);
void FlashStore_WriteAdminPassword(const u8 *pwd6);

#endif
