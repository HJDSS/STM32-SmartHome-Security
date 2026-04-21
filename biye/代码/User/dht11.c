#include "sys.h"
#include "oled.h"
#include "delay.h"
#include "key.h"
#include "tftlcd.h"
#include "usart.h"
#include "dht11.h"
#include "esp8266.h"
#include "onenet.h"
#include <string.h>


#define ESP8266_ONENET_INFO		"AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n" //连接mqtt 服务器
u8 temperature;  	    
u8 humidity; 
_Bool LED_status;
int main(void)
{
	u8 key;
	unsigned char *dataPtr = NULL;
	u8 t=0;	
	 
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	delay_init();	    	 //延时函数初始化	 
 	LED_Init();	
	Usart1_Init(115200);
	Usart2_Init(115200);
	
//	 Lcd_Init();			//初始化OLED  
//	 LCD_Clear(WHITE);
//	 BACK_COLOR=WHITE;
//	 LED_ON;
	LCD_Init();			   	//初始化LCD 
	KEY_Init();
	UsartPrintf(USART_DEBUG, " Hardware init OK\r\n");
	ESP8266_Init();
	
	UsartPrintf(USART_DEBUG, "Connect MQTT Server...\r\n");//先连接mqtt服务
	while(ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT"))
		delay_ms(500);
	UsartPrintf(USART_DEBUG, " MQTT is OK\r\n");
	
	while(OneNet_DevLink())//设备登录
		delay_ms(500);
	UsartPrintf(USART_DEBUG, "设备登录 is OK\r\n");
	
	OneNET_Subscribe();//订阅onenet发布消息的主题
	
	
	LCD_Clear(WHITE);
	//Display_ALIENTEK_LOGO(0, 0);
	POINT_COLOR = RED;
	BACK_COLOR = WHITE;
	LCD_ShowString(30,50,200,16,16, "WarShip STM32F1");
	LCD_ShowString(30,70,200,16,16, "TFTLCD TEST 240*240");

	POINT_COLOR = WHITE;
	BACK_COLOR = BLUE;
	LCD_ShowString(30,90,200,16,16, "ATOM@ALIENTEK123");//第一个16是显示区域高度
	LCD_ShowString(30,110,200,16,16, "2019/1/7");		//第二个16是显示字体大小，而字体大小又以高度判断
	
	
	while(DHT11_Init())	//DHT11初始化	同时检测DHT11 是否存在 0为存在
	{
		LCD_ShowString(30,130,200,16,16,"DHT11 Error");
		delay_ms(200);
		LCD_Fill(30,130,239,130+16,WHITE);
 		delay_ms(200);
	}
	LCD_ShowString(30,130,200,16,16,"DHT11 OK");
	POINT_COLOR=BLUE;//设置字体为蓝色 
	BACK_COLOR = WHITE;
 	LCD_ShowString(30,150,200,16,16,"Temp:  C");	 
 	LCD_ShowString(30,170,200,16,16,"Humi:  %");
	LCD_ShowString(30,190,200,16,16,"WIFI OK MQTT OK");
	LCD_ShowString(30,210,200,16,16,"Device login");
	while(1)
	{
		
		
		/****温度湿度读取并且显示******/
		if(t==10)			//每100ms读取一次
		{									  
			DHT11_Read_Data(&temperature,&humidity);	//读取温湿度值					    
			LCD_ShowNum(30+40,150,temperature,2,16);	//显示温度	   		   
			LCD_ShowNum(30+40,170,humidity,2,16);		//显示湿度	 
			//UsartPrintf(USART_DEBUG, " Hardware init OK and DHT11 is OK\r\n");		
			OneNet_SendData();//上报数据 
			ESP8266_Clear();
		}				   
	 	delay_ms(10);
		t++;
		if(t==20)
		{
			t=0;
		}
		
		
		
		/****按键控制LED闪烁*****/
		key = KEY_Scan(0);
		if(key)
		{
			if(key == WKUP_PRES)
			{
//				LED0=!LED0;
//				LED_status = LED0;
				if(led_info.Led_Status == LED_ON)
				{
						Led_Set(LED_OFF);
				}else{
					Led_Set(LED_ON);
				}
			}
		}
		
		dataPtr = ESP8266_GetIPD(50);
		if(dataPtr != NULL)
			OneNet_RevPro(dataPtr);
		
		
	}//while(1)
	
}

