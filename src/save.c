/***************************************************************************************
文件名 : save.c
文件描述 : 智能救生衣项目文件
修改时间 : 2023.02.11
个人主页 : https://blog.csdn.net/Octopus1633
主动功能 : 用户点击一键报警，每10s向联系人发送一次车辆位置信息，并开启激光灯闪烁，闪烁周期为1s
被动功能 : 当检测到落水后，每10s向联系人发送一次车辆位置信息，并开启激光灯闪烁，闪烁周期为1s
***************************************************************************************/

#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "stdlib.h"
#include "save.h"

/***************************************************
 系统任务相关宏定义
 ***************************************************/
#define SaveMain_TASK_STACK_SIZE    (1024 * 2)
#define SaveMain_TASK_PRIORITY      0
#define SaveMain_TASK_NAME          "SaveMainTask"

#define Second_TASK_STACK_SIZE      (2048 * 2) 
#define Second_TASK_PRIORITY        1 
#define Second_TASK_NAME            "Second_Task"

#define Laser_TASK_STACK_SIZE       (1024 * 2) 
#define Laser_TASK_PRIORITY         2 
#define Laser_TASK_NAME             "Laser_Task"

#define ADC_TASK_STACK_SIZE         (1024 * 2) 
#define ADC_TASK_PRIORITY           3 
#define ADC_TASK_NAME               "ADC_Task"

/***************************************************
 系统任务句柄定义
 ***************************************************/
HANDLE mainTaskHandle = NULL;
HANDLE secondTaskHandle = NULL;
HANDLE adcTaskHandle = NULL;
HANDLE laserTaskHandle = NULL;

/***************************************************
 全局变量引用
 ***************************************************/
extern uint8_t buffer[300];
extern uint8_t PHONE_NUMBER[12];
extern uint8_t saveFlag_old;
extern uint8_t saveFlag_new;
extern bool GPS_flag;

void Laser_Task(void *pData)
{
    /*激光灯任务循环体*/
    while(1)
    {
        /*如果车辆遇水 则控制激光灯闪烁*/
        if(saveFlag_new == 1)
        {
            GPIO_Set(LASER_PIN,GPIO_LEVEL_HIGH);//控制灯亮
            OS_Sleep(500);//500ms
            GPIO_Set(LASER_PIN,GPIO_LEVEL_LOW);//控制灯灭
            OS_Sleep(400);//400ms
        }
        OS_Sleep(100);//saveFlag_new=1时为灯灭100ms saveFlag_new=0时为while的延时等待
    }
}

void ADC_Task(void *pData)
{
    uint16_t value = 0, mV = 0;//记录电压值
    uint8_t adc_flag=0;//记录adc检测值低于阈值次数

    /*创建激光灯闪烁任务*/
    laserTaskHandle = OS_CreateTask(Laser_Task ,
            NULL, NULL, Laser_TASK_STACK_SIZE, Laser_TASK_PRIORITY, 0, 0, Laser_TASK_NAME);

    /*ADC任务循环体*/
    while(1)
    {
        /*读取电压值*/
        if(ADC_Read(ADC_CHANNEL_0, &value, &mV))
        {
            Trace(1,"ADC value:%d, %dmV",value,mV);
            snprintf(buffer,sizeof(buffer),"ADC value:%d, %dmV\r\n",value,mV);
            UART_Write(UART1,buffer,strlen(buffer));
            /*电压值<1300mV 车辆可能遇水*/
            if(mV<1300)
            {
                adc_flag++; 
                if(adc_flag>=5)//连续五次低于1300为遇水 防止误报
                {
                    adc_flag=5;//设上限为5
                    saveFlag_new = 1;//标志位置1，表示遇水
                }
            }
            else
            {
                if(adc_flag>0)
                    adc_flag--;//计数值减少 不直接清零的目的是防止遇水后电压值偶然的跳动
                saveFlag_new = 0; //标志位清零，表示非落水
                ModeChange();//查看是否需要改变电源工作模式
            }    
        }
        else
        {
            /*读取ADC出错*/
            Error();
        }
        /*1s读取一次adc数据*/
        OS_Sleep(1000);
    }
}

void Second_Task(void *pData)
{
    uint8_t ret;

    /* ADC初始化 */
    ADCInit();

    /* GPIO初始化 */
    GPIOInit();

    /* UART1初始化 */
    UartInit();

    /* GPS相关初始化 */
    GPSInit();

    /* SMS相关初始化 */
    SMSInit();

    /* 初次使用作用为创建文件 之后为读取flash号码 */
    ret = ReadData(PHONE_NUMBER);  
    if(ret != true)
    {
        Error();
        UART_Write(UART1,"read flash faild\r\n",strlen("read flash faild\r\n"));
    }

    /*初始化完成提示*/
    UART_Write(UART1,"Device init ok\r\n",strlen("Device init ok\r\n"));

    /*创建ADC检测任务*/
    adcTaskHandle = OS_CreateTask(ADC_Task ,
        NULL, NULL, ADC_TASK_STACK_SIZE, ADC_TASK_PRIORITY, 0, 0, ADC_TASK_NAME);

    /* 进入低功耗模式 */
    OS_Sleep(100);//等待100ms
    PM_SleepMode(true); 
    
    /*副任务循环体-获取位置数据、发送短信*/
    while(1)
    {
        if(saveFlag_new == 1)
        {
            /*查看是否需要改变系统工作模式：遇水-正常 非遇水-低功耗*/
            ModeChange();

            /*获取GPS数据*/
            UART_Write(UART1,"Check water in this car\r\n",strlen("Check water in this car\r\n"));
            UART_Write(UART1,"Get location data:\r\n",strlen("Get location data:\r\nn"));
            GetGPS();

            /*发送短信*/
            UART_Write(UART1,"Send message now...\r\n",strlen("Send message now...\r\n"));
            SendSMS();

            UART_Write(UART1,"Send message sucessfully\r\n",strlen("Send message sucessfully\r\n"));

            /*发送短信时间间隔 10s*/
            OS_Sleep(10000);
        }
        /*等待100ms*/
        OS_Sleep(100);
    }
}

void EventDispatch(API_Event_t* pEvent)//事件处理函数
{
    /*判断事件类型*/
    switch(pEvent->id)
    {
        /*GPS相关*/
        case API_EVENT_ID_GPS_UART_RECEIVED:
            GPS_Update(pEvent->pParam1,pEvent->param1);
            break;

        /*无SIM卡*/
        case API_EVENT_ID_NO_SIMCARD:
            Trace(10,"!!NO SIM CARD%d!!!!",pEvent->param1);
            Error();
            break;

        /*系统就绪*/
        case API_EVENT_ID_SYSTEM_READY:
            Trace(1,"system initialize complete");
            break;

        /*网络相关*/
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING:
            Trace(1,"gprs register complete");
            Trace(2,"network register success");
            GPS_flag = true;
            GPIO_Set(GPSR_LED_PIN,GPIO_LEVEL_LOW);//GPSR提示灯灭 表示GPSR正常
            break;

        /*信息发送成功*/
        case API_EVENT_ID_SMS_SENT:
            Trace(2,"Send Message Success");
            GPIO_Set(GPSR_LED_PIN,GPIO_LEVEL_LOW);//GPSR提示灯灭 表示GPSR正常
            break; 

        /*信息接收 设定联系号码 短信格式为 change@号码*/
        case API_EVENT_ID_SMS_RECEIVED:
            PM_SleepMode(false);//退出低功耗模式
            Trace(2,"received message");
            SMS_Encode_Type_t encodeType = pEvent->param1;
            uint32_t contentLength = pEvent->param2;
            uint8_t* header = pEvent->pParam1;
            uint8_t* content = pEvent->pParam2;

            Trace(2,"message header:%s",header);
            Trace(2,"message content length:%d",contentLength);
            if(encodeType == SMS_ENCODE_TYPE_ASCII)
            {
                Trace(2,"message content:%s",content);
                ChangePhoneNumber(content,contentLength);//修改通讯号码
            }
            PM_SleepMode(true);//进入低功耗模式
            break;

        /*发送信息失败*/
        case API_EVENT_ID_SMS_ERROR:
            Trace(10,"SMS error occured! cause:%d",pEvent->param1);
            Error();
            break;

        default:
            break;
    }
}

void SaveMainTask(VOID *pData)//主任务函数
{
    API_Event_t* event=NULL;
    
    /*创建副任务*/
    secondTaskHandle = OS_CreateTask(Second_Task ,
        NULL, NULL, Second_TASK_STACK_SIZE, Second_TASK_PRIORITY, 0, 0, Second_TASK_NAME);

    /*主任务循环体 处理事件*/
    while(1)
    {
        /*阻塞等待事件发生*/
        if(OS_WaitEvent(mainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
        {
            EventDispatch(event);//有事件发生，执行事件处理函数
            OS_Free(event->pParam1);//释放动态分配的内存
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}

void save_Main(void)//入口函数
{
    /*创建主任务*/
    mainTaskHandle = OS_CreateTask(SaveMainTask ,
        NULL, NULL, SaveMain_TASK_STACK_SIZE, SaveMain_TASK_PRIORITY, 0, 0, SaveMain_TASK_NAME);

    /*设置主任务，主任务用来接收来自底层的消息，这个函数必须在开机的时候创建主任务后立即调用，否则可能导致底层无法向应用层发布消息*/
    OS_SetUserMainHandle(&mainTaskHandle);
}