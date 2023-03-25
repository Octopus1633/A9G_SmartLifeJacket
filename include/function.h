#ifndef _FUNCTION_H_
#define _FUNCTION_H_

/***************************************************
 底层相关头文件
 ***************************************************/
#include "api_os.h"
#include "api_event.h"
#include "api_debug.h"
#include "api_hal_uart.h"
#include "api_hal_gpio.h"
#include "api_hal_adc.h"
#include "api_hal_pm.h"
#include "api_fs.h"

/***************************************************
 GPS相关头文件
 ***************************************************/
#include "api_gps.h" 
#include "gps_parse.h"   
#include "gps.h"
#include "math.h"
#include "buffer.h"

/***************************************************
 SMS相关头文件
 ***************************************************/
#include "api_sms.h"

/***************************************************
 指示灯宏定义
 ***************************************************/
#define GPSR_LED_PIN GPIO_PIN27//GPSR提示灯 灯亮表示不正常
#define GPS_LED_PIN  GPIO_PIN28//GPS提示灯 灯亮表示不正常
#define LASER_PIN    GPIO_PIN26//激光灯控制引脚
#define KEY_PIN      GPIO_PIN6//连接报警按键引脚（可触发中断的引脚有GPIO0~GPIO7）
#define PM_PIN       GPIO_PIN3//电源指示灯控制引脚

/***************************************************
 其他宏定义
 ***************************************************/
#define CONFIG_FILE_NAME "/phone.txt"

/***************************************************
 函数声明
 ***************************************************/
void UartInit(void);//UART1初始化函数
void ADCInit(void);//ADC初始化函数
void GPIOInit(void);//GPIO初始化函数
void GPSInit(void);//GPS相关初始化函数
void SMSInit(void);//SMS相关初始化函数
void SendUtf8(void);//短信内容以utf-8编码方式发送
void SendSMS(void);//发送短信函数
void GetGPS(void);//获取GPS位置数据，无GPS信号则为基站定位
void save_Main(void);//入口函数
void SaveMainTask(VOID *pData);//主任务函数
void Second_Task(void *pData);//副任务函数
void EventDispatch(API_Event_t* pEvent);//事件处理函数
void Error(void);//系统错误处理
void ModeChange(void);//工作模式更新
void ChangePhoneNumber(uint8_t* content,uint32_t contentLength);//修改接收信息号码
bool ReadData(uint8* data);//从flash中读取数据
bool SaveData(uint8* data);//保存数据到flash
void OnPinFalling(GPIO_INT_callback_param_t* param);//按键中断处理函数

#endif // !_FUNCTION_H