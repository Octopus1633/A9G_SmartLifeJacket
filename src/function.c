/***************************************************************************
文件名 : function.c
修改时间 : 2023.02.11
描述 : 系统所需功能对应函数文件
***************************************************************************/

#include "stdint.h"
#include "stdbool.h"
#include "string.h"
#include "stdlib.h"
#include "function.h"

/***************************************************
 GPS相关全局变量定义
 ***************************************************/
bool GPS_flag = false;          //GPS初始化标志位
GPS_Info_t* gpsInfo = NULL;     //GPS数据指针
uint8_t buffer[300];            //存储字符串
/***************************************************
 SMS相关全局变量定义
 ***************************************************/
uint8_t PHONE_NUMBER[12];       //联系人号码
uint8_t send_buf[300];          //短信字符串
/***************************************************
 其他全局变量定义
 ***************************************************/
uint8_t saveFlag_old = 0;       //系统上一状态标志位
uint8_t saveFlag_new = 0;       //系统当前状态标志位
extern HANDLE adcTaskHandle;

void OnPinFalling(GPIO_INT_callback_param_t* param)
{
    switch(param->pin)
    {
        case GPIO_PIN6:
            OS_StopTask(adcTaskHandle);//停止ADC检测任务
            saveFlag_new = 1;
            break;
        default:
            break;
    }
}

void UartInit(void)
{
    UART_Config_t config = {
        .baudRate = UART_BAUD_RATE_115200,
        .dataBits = UART_DATA_BITS_8,
        .stopBits = UART_STOP_BITS_1,
        .parity   = UART_PARITY_NONE,
        .rxCallback = NULL,
    };
    UART_Init(UART1,config);
}

void ADCInit(void)
{
    ADC_Config_t config = {
        .channel = ADC_CHANNEL_0,//ADC通道0
        .samplePeriod = ADC_SAMPLE_PERIOD_100MS
    };
    ADC_Init(config);
}

void GPIOInit(void)
{
    /*打开GPIO0 ~ GPIO7 and GPIO25 ~ GPIO36电源*/
    PM_PowerEnable(POWER_TYPE_VPAD,true);

    /*初始化结构体*/
    GPIO_config_t gpio_config_t = {
        .mode         = GPIO_MODE_OUTPUT
    };

    gpio_config_t.pin = LASER_PIN;//激光灯控制引脚
    gpio_config_t.defaultLevel = GPIO_LEVEL_LOW;//默认低电平->激光灯灭
    GPIO_Init(gpio_config_t);

    gpio_config_t.pin = GPSR_LED_PIN;//GPRS网络状态指示灯
    gpio_config_t.defaultLevel = GPIO_LEVEL_HIGH;//默认高电平->LED亮
    GPIO_Init(gpio_config_t);

    gpio_config_t.pin = GPS_LED_PIN;//GPS定位状态指示灯
    gpio_config_t.defaultLevel = GPIO_LEVEL_HIGH;//默认高电平->LED亮
    GPIO_Init(gpio_config_t);

    // gpio_config_t.pin = PM_PIN;//电源状态指示灯
    // gpio_config_t.defaultLevel = GPIO_LEVEL_LOW;//默认低电平
    // GPIO_Init(gpio_config_t);

    gpio_config_t.pin = KEY_PIN;//一键报警
    gpio_config_t.mode = GPIO_MODE_INPUT_INT;//配置为中断方式
    gpio_config_t.intConfig.type = GPIO_INT_TYPE_FALLING_EDGE;//下降沿捕获
    gpio_config_t.intConfig.debounce = 15;//15ms消抖
    gpio_config_t.intConfig.callback = OnPinFalling;//指定回调函数
    GPIO_Init(gpio_config_t);

    
}

void GPSInit(void)
{
    gpsInfo = Gps_GetInfo();
    //wait for gprs register complete
    //The process of GPRS registration network may cause the power supply voltage of GPS to drop,
    //which resulting in GPS restart.
    while(!GPS_flag)
    {
        Trace(1,"wait for gprs regiter complete");
        OS_Sleep(2000);
    }

    //open GPS hardware(UART2 open either)
    GPS_Init();
    GPS_Open(NULL);

    //wait for gps start up, or gps will not response command
    while(gpsInfo->rmc.latitude.value == 0)
        OS_Sleep(1000);
    
    // set gps nmea output interval
    for(uint8_t i = 0;i<5;++i)
    {
        bool ret = GPS_SetOutputInterval(10000);
        Trace(1,"set gps ret:%d",ret);
        if(ret)
            break;
        OS_Sleep(1000);
    }
    
    if(!GPS_GetVersion(buffer,150))
        Trace(1,"get gps firmware version fail");
    else
        Trace(1,"gps firmware version:%s",buffer);

    if(!GPS_SetOutputInterval(1000))
        Trace(1,"set nmea output interval fail");
    
    Trace(1,"init ok");
    GPIO_Set(GPS_LED_PIN,GPIO_LEVEL_LOW);//GPS提示灯灭 表示GPS正常
}

void SMSInit(void)
{
    if(!SMS_SetFormat(SMS_FORMAT_TEXT,SIM0))
    {
        Trace(1,"sms set format error");
        return;
    }
    SMS_Parameter_t smsParam = {
        .fo = 17 ,
        .vp = 167,
        .pid= 0  ,
        .dcs= 8  ,//0:English 7bit, 4:English 8 bit, 8:Unicode 2 Bytes
    };
    if(!SMS_SetParameter(&smsParam,SIM0))
    {
        Trace(1,"sms set parameter error");
        return;
    }
    if(!SMS_SetNewMessageStorage(SMS_STORAGE_SIM_CARD))
    {
        Trace(1,"sms set message storage fail");
        return;
    }
}

void Error(void)
{
    Trace(1,"Something went wrong");
    UART_Write(UART1,"Something went wrong\r\n",strlen("Something went wrong\r\n"));

    /*俩灯都亮 可能为均异常或其他异常*/
    GPIO_Set(GPSR_LED_PIN,GPIO_LEVEL_HIGH);
    GPIO_Set(GPS_LED_PIN,GPIO_LEVEL_HIGH);
}

void SendUtf8(void)
{
    uint8_t* unicode = NULL;
    uint32_t unicodeLen;

    Trace(1,"sms start send UTF-8 message");

    /*本地语言转unicode编码 UTF_8->unicode*/
    if(!SMS_LocalLanguage2Unicode(send_buf,strlen(send_buf),CHARSET_UTF_8,&unicode,&unicodeLen))
    {
        Trace(1,"local to unicode fail!");
        Error();
        return;
    }

    /*发送短信*/
    if(!SMS_SendMessage(PHONE_NUMBER,unicode,unicodeLen,SIM0))
    {
        Trace(1,"sms send message fail");
        Error();
    }
    OS_Free(unicode);
}

void SendSMS(void)
{
    // SendSmsUnicode();
    // SendSmsGbk();
    SendUtf8();
}

void GetGPS(void)
{
    /*show fix info*/
    uint8_t isFixed = gpsInfo->gsa[0].fix_type > gpsInfo->gsa[1].fix_type ?gpsInfo->gsa[0].fix_type:gpsInfo->gsa[1].fix_type;
    char* isFixedStr = NULL;            
    if(isFixed == 2)
        isFixedStr = "2D fix";
    else if(isFixed == 3)
    {
        if(gpsInfo->gga.fix_quality == 1)
            isFixedStr = "3D fix";
        else if(gpsInfo->gga.fix_quality == 2)
            isFixedStr = "3D/DGPS fix";
    }
    else
        isFixedStr = "no fix";

    /*convert unit ddmm.mmmm to degree(°) */
    int temp = (int)(gpsInfo->rmc.latitude.value/gpsInfo->rmc.latitude.scale/100);
    double latitude = temp+(double)(gpsInfo->rmc.latitude.value - temp*gpsInfo->rmc.latitude.scale*100)/gpsInfo->rmc.latitude.scale/60.0;
    temp = (int)(gpsInfo->rmc.longitude.value/gpsInfo->rmc.longitude.scale/100);
    double longitude = temp+(double)(gpsInfo->rmc.longitude.value - temp*gpsInfo->rmc.longitude.scale*100)/gpsInfo->rmc.longitude.scale/60.0;

    /*you can copy ` latitude,longitude ` to http://www.gpsspg.com/maps.htm check location on map*/
    snprintf(buffer,sizeof(buffer),"GPS fix mode:%d, BDS fix mode:%d, fix quality:%d, satellites tracked:%d, gps sates total:%d, is fixed:%s, coordinate:WGS84, Latitude:%f, Longitude:%f, unit:degree,altitude:%f",gpsInfo->gsa[0].fix_type, gpsInfo->gsa[1].fix_type,
                                                        gpsInfo->gga.fix_quality,gpsInfo->gga.satellites_tracked, gpsInfo->gsv[0].total_sats, isFixedStr, latitude,longitude,gpsInfo->gga.altitude);
    
    /*编辑短信发送内容*/
    snprintf(send_buf,sizeof(send_buf),"驾驶员落水警告!所在WGS84坐标:%f,%f",longitude,latitude);

    /*show in tracer*/
    Trace(2,buffer);

    /*send to UART1*/
    UART_Write(UART1,buffer,strlen(buffer));
    UART_Write(UART1,"\r\n",2);
    
    /*close led*/
    GPIO_Set(GPS_LED_PIN,GPIO_LEVEL_LOW);
}

void ModeChange(void)
{
    // uint8_t percent;
    // PM_Voltage(&percent);
    // if(percent < 5)//电池电量少于5%
    //     GPIO_Set(PM_PIN,GPIO_LEVEL_HIGH);//电源指示灯亮
    // else
    //     GPIO_Set(PM_PIN,GPIO_LEVEL_LOW);//电源指示灯灭

    /*状态更新*/
    if(saveFlag_new != saveFlag_old)
    {
        saveFlag_old = saveFlag_new;
        if(saveFlag_new==1)//落水
        {
            UART_Write(UART1,"Exit sleep mode now\r\n",strlen("Exit sleep mode now\r\n"));
            PM_SleepMode(false);//退出低功耗模式
        }
        else//无
        {
            UART_Write(UART1,"Sleep now\r\n",strlen("Sleep now\r\n"));
            PM_SleepMode(true);//进入低功耗模式 降低主频到32kHz 程序依然会运行
        }
    }
}

void ChangePhoneNumber(uint8_t* content,uint32_t contentLength)
{
    uint8_t receive_buf[30];
    uint8_t change_flag = 0;
    uint8_t check_buf[100];
    uint8_t Read_buf[30];
    uint8_t ret;

    /*指针转数组 strtok拆分字符串是直接在原串上操作，所以要求参1必须，可读可写（char *str = "xxx" 不行！！！）*/
    strcpy(receive_buf,content);

    /*确认是否为修改号码的短信 change@号码 strtok函数用于分割字符串 第一次调用strtok返回@前的字符串*/
    strcpy(check_buf,strtok(receive_buf,"@"));
    
    /*检测短信长度是否为17 @前的内容是否为change*/
    if(contentLength != 17 && strcmp("change",check_buf) != 0)
    {
        change_flag = 1;//其他短信或短信格式有误 表示第一种出错情况
        goto error;
    }

    /*再次以@字符分割*/
    strcpy(check_buf,strtok(NULL,"@"));
    
    /*判断号码长短是否为11位数*/
    if(strlen(check_buf) != 11)
    {
        change_flag = 2;
        goto error;
    }

    /*向原号码发送短信 先查看文件下是否有号码信息 判断是否为初次使用 初次使用的话PHONE_NUMBER中无号码 需要先修改才能发送*/
    if(ReadData(Read_buf)==false)//文件为空 初次使用
    {
        snprintf(send_buf,sizeof(send_buf),"初始化成功!设置的联系号码为:%s!",check_buf);
        strcpy(PHONE_NUMBER,check_buf);//修改号码
    }
    else//非初次使用
    {
        snprintf(send_buf,sizeof(send_buf),"您已成功修改联系号码为:%s!",check_buf);
    }
    SendSMS();

    /*修改号码*/
    strcpy(PHONE_NUMBER,check_buf);

    /*修改flash中存储的号码*/
    ret = SaveData(PHONE_NUMBER);
    if(ret != true)
    {
        change_flag = 3;
        Error();
    }

    /*错误处理 正常运行change_flag为0*/
    error:
    if(change_flag != 0)
    {
        switch(change_flag)
        {
            case 1:
                sprintf(send_buf,"修改格式有误,格式为change@号码,请您重新发送!");
                break;
            case 2:
                sprintf(send_buf,"号码有误,请您更正号码后重新发送!");
                break;
            case 3:
                sprintf(send_buf,"储存号码失败!");
                break;
        }
        SendSMS();
    }
}

bool SaveData(uint8_t* data)
{
    int32_t fd;
    int32_t ret;
    uint8_t *path = CONFIG_FILE_NAME;

    /*以只写方式打开文件 若无则创建 若有则清空原内容*/
    fd = API_FS_Open(path, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, 0);
	if ( fd < 0)
	{
        Trace(1,"Open file failed:%d",fd);
		return false;
	}

    /*向文件写入长度为11字节的数据*/
    ret = API_FS_Write(fd,data,11);
    if(ret <= 0)
    {
        API_FS_Close(fd);
        return false;
    }
    Trace(1,"write ret:%d",ret);

    /*关闭文件*/
    API_FS_Close(fd);

	return true;
}

bool ReadData(uint8_t *data)
{
    int32_t fd;
    int32_t ret;
    uint8_t *path = CONFIG_FILE_NAME;

    /*以只读方式打开文件 若不存在则创建*/
    fd = API_FS_Open(path, (FS_O_RDONLY|FS_O_CREAT), 0);
	if ( fd < 0)
	{
        Trace(1,"Open file failed:%d",fd);
		return false;
    }

    /*从文件读取长度为11字节的数据*/
    ret = API_FS_Read(fd,data,11) ;
    if(ret <= 0)
    {
        API_FS_Close(fd);
        return false;
    }
    Trace(1,"read ret:%d",ret);

    /*关闭文件*/
    API_FS_Close(fd);
    
	return true;
}
