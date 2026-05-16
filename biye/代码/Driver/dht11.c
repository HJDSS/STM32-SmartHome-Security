#include "dht11.h"
#include "delay.h"
#include <stddef.h>

static volatile u8 dht11_last_err = 0;
static volatile u8 dht11_last_dq = 1;
static volatile u8 dht11_last_resp = 0;
static volatile u8 dht11_dbg_vals[8] = {0};

static void DHT11_EnterCritical(void)
{
    __disable_irq();
}

static void DHT11_ExitCritical(void)
{
    __enable_irq();
}

void DHT11_Set_IO_Mode(GPIOMode_TypeDef mode)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = DHT11_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = mode;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_GPIO_PORT, &GPIO_InitStructure);
}

void DHT11_Rst(void)
{
    DHT11_IO_OUT();
    DHT11_DQ_OUT(1);
    delay_us(30);
    DHT11_DQ_OUT(0);
    delay_ms(BOARD_DHT_RST_LOW_MS);
    DHT11_DQ_OUT(1);
    delay_us(BOARD_DHT_RST_RELEASE_US);
    DHT11_IO_IN();
}

u8 DHT11_Check(void)
{
    u16 timeout;

    DHT11_IO_IN();
    dht11_last_resp = 0;

    timeout = DHT11_TIMEOUT_US;
    while(DHT11_DQ_IN && timeout--)
    {
        delay_us(1);
    }
    dht11_dbg_vals[0] = (u8)(DHT11_DQ_IN ? 1u : 0u);
    if(timeout == 0) return 1;

    timeout = DHT11_TIMEOUT_US;
    while(!DHT11_DQ_IN && timeout--)
    {
        delay_us(1);
    }
    dht11_dbg_vals[1] = (u8)(DHT11_DQ_IN ? 1u : 0u);
    if(timeout == 0) return 1;

    timeout = DHT11_TIMEOUT_US;
    while(DHT11_DQ_IN && timeout--)
    {
        delay_us(1);
    }
    dht11_dbg_vals[2] = (u8)(DHT11_DQ_IN ? 1u : 0u);
    if(timeout == 0) return 1;

    dht11_last_resp = 1;
    return 0;
}

u8 DHT11_Read_Bit(void)
{
    u16 timeout;

    timeout = DHT11_TIMEOUT_US;
    while(!DHT11_DQ_IN && timeout--)
    {
        delay_us(1);
    }
    if(timeout == 0)
    {
        dht11_last_err = 2;
        return 0;
    }

    delay_us(BOARD_DHT_BIT_SAMPLE_US);
    dht11_last_dq = DHT11_DQ_IN;
    return (dht11_last_dq != 0) ? 1u : 0u;
}

volatile u8 dht11_byte_timeout = 0u;

u8 DHT11_Read_Byte(void)
{
    u8 i;
    u8 dat = 0;
    u16 timeout;

    dht11_byte_timeout = 0u;

    for(i = 0; i < 8; i++)
    {
        timeout = DHT11_TIMEOUT_US;
        while(DHT11_DQ_IN == 0)
        {
            if(timeout-- == 0u)
            {
                dht11_byte_timeout = 1u;
                dht11_last_err = 2;
                return 0;
            }
            delay_us(1);
        }
        delay_us(BOARD_DHT_BIT_SAMPLE_US);
        dat <<= 1;
        if(DHT11_DQ_IN == 1)
        {
            dat |= 1;
        }
        timeout = DHT11_TIMEOUT_US;
        while(DHT11_DQ_IN == 1)
        {
            if(timeout-- == 0u)
            {
                dht11_byte_timeout = 1u;
                dht11_last_err = 2;
                return dat;
            }
            delay_us(1);
        }
    }

    return dat;
}

u8 DHT11_Read_Data(u8 *temp, u8 *humi)
{
    u8 buf[5] = {0};
    u8 try_cnt;
    u8 i;
    u8 checksum;

    dht11_last_err = 0;

    for(try_cnt = 0; try_cnt < BOARD_DHT11_INTERNAL_RETRY; try_cnt++)
    {
        DHT11_Rst();
        if(DHT11_Check() != 0)
        {
            dht11_last_err = 1;
            delay_ms(2);
            continue;
        }

        dht11_byte_timeout = 0u;
        DHT11_EnterCritical();
        for(i = 0; i < 5; i++)
        {
            buf[i] = DHT11_Read_Byte();
            dht11_dbg_vals[3 + i] = buf[i];
            if(dht11_byte_timeout)
                break;
        }
        DHT11_ExitCritical();

        if(dht11_byte_timeout)
        {
            dht11_last_err = 2;
            delay_ms(2);
            continue;
        }

        checksum = (u8)(buf[0] + buf[1] + buf[2] + buf[3]);
        if(checksum != buf[4])
        {
            dht11_last_err = 3;
            delay_ms(2);
            continue;
        }

        *humi = buf[0];
        *temp = buf[2];
        return 0;
    }

    return 1;
}

u8 DHT11_Init(void)
{
    u8 ok;

    RCC_APB2PeriphClockCmd(DHT11_GPIO_CLK, ENABLE);
    DHT11_IO_OUT();
    DHT11_DQ_OUT(1);
    DHT11_Rst();
    ok = DHT11_Check();

    dht11_last_err = (ok == 0) ? 0 : 1;
    return ok;
}

u8 DHT11_GetLastError(void)
{
    return dht11_last_err;
}

u8 DHT11_GetLastDQ(void)
{
    return dht11_last_dq;
}

u8 DHT11_GetLastResp(void)
{
    return dht11_last_resp;
}

void DHT11_Debug_ReadFrame(char *out)
{
    static const char hx[] = "0123456789ABCDEF";
    u8 i;
    if(out == NULL) return;
    for(i = 0; i < 8; i++)
    {
        out[i * 3 + 0] = hx[(dht11_dbg_vals[i] >> 4) & 0x0F];
        out[i * 3 + 1] = hx[dht11_dbg_vals[i] & 0x0F];
        out[i * 3 + 2] = ' ';
    }
    out[24] = '\0';
}
