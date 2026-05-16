#include "as608.h"
#include "usart1.h"
#include "delay.h"

extern void AS608_PollYield(void);
extern volatile unsigned char g_as608_user_abort;

static void delay_ms_yield(unsigned short ms)
{
    while(ms--)
    {
#if USE_FREERTOS
        extern void vTaskDelay(unsigned int xTicksToDelay);
        vTaskDelay(1u);
#else
        delay_ms(1);
#endif
        AS608_PollYield();
    }
}

#define STM32_RX1_BUF       Usart1RecBuf
#define STM32_Rx1Counter    RxCounter
#define STM32_RX1BUFF_SIZE  USART1_RXBUFF_SIZE

unsigned char AS608_RECEICE_BUFFER[24];

unsigned char AS608_Pack_Head[6] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF};
unsigned char AS608_Get_Img[6] = {0x01,0x00,0x03,0x01,0x0,0x05};
unsigned char FP_Search[11]= {0x01,0x0,0x08,0x04,0x01,0x0,0x0,0x03,0xA1,0x0,0xB2};
unsigned char FP_Img_To_Buffer1[7]= {0x01,0x0,0x04,0x02,0x01,0x0,0x08};
unsigned char FP_Img_To_Buffer2[7]= {0x01,0x0,0x04,0x02,0x02,0x0,0x09};
unsigned char FP_Reg_Model[6]= {0x01,0x0,0x03,0x05,0x0,0x09};
unsigned char FP_Delet_All_Model[6]= {0x01,0x0,0x03,0x0d,0x00,0x11};
unsigned char FP_Save_Finger[9]= {0x01,0x00,0x06,0x06,0x01,0x00,0x0B,0x00,0x19};
unsigned char FP_Delete_Model[10]= {0x01,0x00,0x07,0x0C,0x0,0x0,0x0,0x1,0x0,0x0};


void PS_StaGPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(BOARD_AS608_PS_STA_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Pin = BOARD_AS608_PS_STA_PIN;
#if AS608_WAKE_ACTIVE_HIGH
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
#else
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
#endif
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BOARD_AS608_PS_STA_PORT, &GPIO_InitStructure);
}

unsigned char uart_recv(unsigned char *bufs, unsigned short timeout)
{
    unsigned char i;
    unsigned short t = timeout;
    unsigned int last_cnt = 0;
    unsigned char idle_ms = 0;

    while(t > 0u)
    {
#if USE_FREERTOS
        extern void vTaskDelay(unsigned int xTicksToDelay);
        vTaskDelay(1u);
#else
        delay_ms(1);
#endif
        AS608_PollYield();
        t--;

        if(STM32_Rx1Counter != last_cnt)
        {
            last_cnt = STM32_Rx1Counter;
            idle_ms = 0u;
        }
        else if(STM32_Rx1Counter >= AS608_RX_MIN_BYTES)
        {
            idle_ms++;
            if(idle_ms >= AS608_RX_IDLE_MS)
                break;
        }
    }

    {
        unsigned char ret = STM32_Rx1Counter;
        if(STM32_Rx1Counter > 0)
        {
            for(i = 0; i < STM32_Rx1Counter; i++)
            {
                *bufs = STM32_RX1_BUF[i];
                bufs++;
            }
        }
        STM32_Rx1Counter = 0;
        return ret;
    }
}

unsigned char AS608_Receive_Data(void)
{
    memset(AS608_RECEICE_BUFFER,0,24);
    return uart_recv(AS608_RECEICE_BUFFER,AS608_UART_RECV_TIMEOUT_MS);
}

void AS608_Cmd_Send_Pack_Head(void)
{
    uart1_send(AS608_Pack_Head,6);
}

unsigned char AS608_Cmd_Get_Img(void)
{
    AS608_Cmd_Send_Pack_Head();
    uart1_send(AS608_Get_Img,6);
    if (AS608_Receive_Data() > 0)
    {
        return AS608_RECEICE_BUFFER[9];
    }
    else
    {
        return 0xFE;
    }
}

unsigned char FINGERPRINT_Cmd_Img_To_Buffer1(void)
{
    AS608_Cmd_Send_Pack_Head();
    uart1_send(FP_Img_To_Buffer1,7);
    if (AS608_Receive_Data() > 0)
    {
        return AS608_RECEICE_BUFFER[9];
    }
    else
    {
        return 0xFE;
    }
}

unsigned char FINGERPRINT_Cmd_Img_To_Buffer2(void)
{
    AS608_Cmd_Send_Pack_Head();
    uart1_send(FP_Img_To_Buffer2,7);
    if (AS608_Receive_Data() > 0)
    {
        return AS608_RECEICE_BUFFER[9];
    }
    else
    {
        return 0xFE;
    }
}

unsigned char FINGERPRINT_Cmd_Reg_Model(void)
{
    AS608_Cmd_Send_Pack_Head();
    uart1_send(FP_Reg_Model,6);
    if (AS608_Receive_Data() > 0)
    {
        return AS608_RECEICE_BUFFER[9];
    }
    else
    {
        return 0xFE;
    }
}

unsigned char FINGERPRINT_Cmd_Delete_All_Model(void)
{
    AS608_Cmd_Send_Pack_Head();
    uart1_send(FP_Delet_All_Model,6);
    if (AS608_Receive_Data() > 0)
    {
        return AS608_RECEICE_BUFFER[9];
    }
    else
    {
        return 0xFE;
    }
}

void FINGERPRINT_Cmd_Delete_Model(unsigned short uiID_temp)
{
    unsigned short uiSum_temp = 0;
    unsigned char i;

    FP_Delete_Model[4]=(uiID_temp&0xFF00)>>8;
    FP_Delete_Model[5]=(uiID_temp&0x00FF);

    for(i=0; i<8; i++)
        uiSum_temp = uiSum_temp + FP_Delete_Model[i];

    FP_Delete_Model[8]=(uiSum_temp&0xFF00)>>8;
    FP_Delete_Model[9]=uiSum_temp&0xFF;


    AS608_Cmd_Send_Pack_Head();
    uart1_send(FP_Delete_Model,10);
}

unsigned short FINGERPRINT_Cmd_Search_Finger(void)
{
    AS608_Cmd_Send_Pack_Head();
    uart1_send(FP_Search,11);
    if (AS608_Receive_Data() > 0 && AS608_RECEICE_BUFFER[9] == 0)
    {
        return (AS608_RECEICE_BUFFER[10]*256 + AS608_RECEICE_BUFFER[11]);
    }
    else
    {
        return 0xFFFE;
    }
}

unsigned char FINGERPRINT_Cmd_Save_Finger(unsigned short storeID)
{
    unsigned short temp = 0;
    unsigned char i;

    FP_Save_Finger[5] =(storeID&0xFF00)>>8;
    FP_Save_Finger[6] = (storeID&0x00FF);

    for(i=0; i<7; i++)
        temp = temp + FP_Save_Finger[i];

    FP_Save_Finger[7]=(temp & 0xFF00) >> 8;
    FP_Save_Finger[8]= temp & 0xFF;

    AS608_Cmd_Send_Pack_Head();

    uart1_send(FP_Save_Finger,9);
    if (AS608_Receive_Data() > 0)
    {
        return AS608_RECEICE_BUFFER[9];
    }
    else
    {
        return 0xFE;
    }
}

unsigned short AS608_Find_Fingerprint(void)
{
    g_as608_user_abort = 0;
    if(AS608_Cmd_Get_Img() == 0)
    {
        delay_ms_yield(AS608_FIND_IMG_GAP_MS);
        if(g_as608_user_abort) return 0xFFFE;
        if (FINGERPRINT_Cmd_Img_To_Buffer1() == 0)
        {
            if(g_as608_user_abort) return 0xFFFE;
            return FINGERPRINT_Cmd_Search_Finger();
        }
    }
    return 0xFFFE;
}

unsigned short AS608_Add_Fingerprint(unsigned short ID)
{
    if(AS608_Cmd_Get_Img() == 0)
    {
        if (FINGERPRINT_Cmd_Img_To_Buffer1() == 0)
        {
            if(AS608_Cmd_Get_Img() == 0)
            {
                if (FINGERPRINT_Cmd_Img_To_Buffer2() == 0)
                {
                    if (FINGERPRINT_Cmd_Reg_Model() == 0)
                    {
                        return FINGERPRINT_Cmd_Save_Finger(ID) ;
                    }
                }
            }
        }
    }
    return 0xFFFE;
}
