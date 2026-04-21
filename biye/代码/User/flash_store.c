#include "flash_store.h"
#include "secure_store.h"
#include <stddef.h>

void FlashStore_Init(void)
{
    SECURE_CFG_T cfg;
    if(SecStore_Init() == SECSTORE_OK)
    {
        (void)SecStore_ReadCfg(&cfg);
    }
}

void FlashStore_ReadPasswords(u8 *user_pwd, u8 *admin_pwd)
{
    SECURE_CFG_T cfg;
    u8 i;

    if(user_pwd == NULL || admin_pwd == NULL)
        return;

    if(SecStore_ReadCfg(&cfg) == SECSTORE_OK)
    {
        for(i = 0; i < 6; i++)
        {
            user_pwd[i] = cfg.user_pwd[i];
            admin_pwd[i] = cfg.admin_pwd[i];
        }
    }
}

void FlashStore_WriteUserPassword(const u8 *pwd6)
{
    SECURE_CFG_T cfg;
    u8 i;

    if(pwd6 == NULL)
        return;
    if(SecStore_ReadCfg(&cfg) != SECSTORE_OK)
        return;

    for(i = 0; i < 6; i++)
        cfg.user_pwd[i] = pwd6[i];

    SecStore_WriteCfg(&cfg);
}

void FlashStore_WriteAdminPassword(const u8 *pwd6)
{
    SECURE_CFG_T cfg;
    u8 i;

    if(pwd6 == NULL)
        return;
    if(SecStore_ReadCfg(&cfg) != SECSTORE_OK)
        return;

    for(i = 0; i < 6; i++)
        cfg.admin_pwd[i] = pwd6[i];

    SecStore_WriteCfg(&cfg);
}
