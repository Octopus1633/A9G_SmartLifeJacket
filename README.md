# A9G_SmartLifeJacket

---
# 前言
`本篇分享：`

对**安信可A9G模组**二次开发，使其能满足智能救生衣的功能，如，**检测车辆是否浸水**、**获取地理坐标并向家属号码发送短信提醒**、**通过短信修改修改通讯号码**等功能。


`环境介绍：`

系统：Windows
SDK：CSDTK4.2
IDE ：VSCode
硬件：安信可A9G开发板

---

# 效果展示
![在这里插入图片描述](https://img-blog.csdnimg.cn/0cb643f2ee2f4c1a8230e96b9d4af301.png#pic_center)

# 项目代码编写

## 1.硬件原理图

![在这里插入图片描述](https://img-blog.csdnimg.cn/b630f4c215224fcea0fb1c6645a18d75.png#pic_center)



## 2.创建多任务处理

**类似Linux多线程机制**，每个任务中都可以使用while(1)循环检测。在该项目中包括主任务在内共有**四个任务**，分别是：**==主任务==**（循环检测处理各种事件，如信息发送、接收事件）、**==副任务==**（获取地理坐标、发送短信）、**==ADC检测任务==**（检测车辆是否浸水）、**==激光灯闪烁任务==**（控制激光灯周期性闪烁）

## 3.控制激光灯闪烁

### 打开GPIO电源

要保证引脚功能可以正常使用，在配置IO之前必须 **==将对应IO口的电源打开==** ，使用下面函数即可实现

```c
函数原型：
bool PM_PowerEnable(Power_Type_t powerType, bool isOn)；
 
参数：
powerType -- IO选择，POWER_TYPE_VPAD(GPIO0~GPIO17 & GPIO25~GPIO36),POWER_TYPE_MMC(GPIO8~GPIO13) & POWER_TYPE_LCD(GPIO14~GPIO18) & POWER_TYPE_CAM(GPIO19~24)
isOn -- true or flase   
```

 代码（打开GPIO26电源）

```c
/*打开GPIO0 ~ GPIO7 and GPIO25 ~ GPIO36电源*/
PM_PowerEnable(POWER_TYPE_VPAD,true);
```

### GPIO初始化配置

函数

```c
函数原型：
bool GPIO_Init(GPIO_config_t config);

参数：
config -- 初始化配置结构体
```

代码

```c
/*宏定义*/
#define LASER_PIN    GPIO_PIN26//激光灯控制引脚

/*配置*/
GPIO_config_t gpio_config_t;
gpio_config_t.mode = GPIO_MODE_OUTPUT;//输出模式
gpio_config_t.pin = LASER_PIN;//激光灯控制引脚
gpio_config_t.defaultLevel = GPIO_LEVEL_LOW;//默认低电平->激光灯灭

/*初始化*/
GPIO_Init(gpio_config_t);    
```

### 控制GPIO电平

```c
/*引脚输出高电平*/
GPIO_Set(LASER_PIN,GPIO_LEVEL_HIGH);//控制灯亮
/*引脚输出低电平*/
GPIO_Set(LASER_PIN,GPIO_LEVEL_LOW);//控制灯灭
```

### 延时函数

```c
/*参数单位：毫秒*/
OS_Sleep(500);//500ms
```

## 5.ADC采集 

### ADC初始化配置

```c
ADC_Config_t config = {
    .channel = ADC_CHANNEL_0,//ADC通道0
    .samplePeriod = ADC_SAMPLE_PERIOD_100MS
};
ADC_Init(config);
```

### 读取ADC电压值以及后续操作

```c
/*变量定义*/
uint16_t value = 0, mV = 0;//记录电压值
uint8_t adc_flag=0;//记录adc检测值低于阈值次数    

/*ADC任务循环体*/
while(1)
{
    /*读取电压值*/
    if(ADC_Read(ADC_CHANNEL_0, &value, &mV))
    {
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
            if(adc_flag>0)adc_flag--;//计数值减少 不直接清零的目的是防止遇水后电压值波动过大
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
```

## 6.获取地理坐标

根据例程代码，将初始化和数据获取封装成 **==两个函数==**。

函数

```c
/* GPS相关初始化 */
GPSInit();

/*获取GPS数据*/
GetGPS();
```

在 **==GetGPS()==** 函数中，提取坐标并保存至数组，后续发送短信使用

```c
/*编辑短信发送内容*/
snprintf(send_buf,sizeof(send_buf),"驾驶员落水警告!所在WGS84坐标:%f,%f",longitude,latitude);
```

## 7.发送短信

参考例程中的 **==三个函数==** 实现。

```c
/*SMS相关初始化*/
SMSInit();

/*发送短信*/
SendSMS();

/*Utf8格式*/
SendUtf8();
```

代码

```c
/*SMS相关全局变量定义*/
uint8_t PHONE_NUMBER[12];//联系人号码
uint8_t send_buf[300];//短信字符串

/*SMS相关初始化*/
SMSInit();
/*发送短信*/
SendSMS();//内部调用SendUtf8()

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
```

## 8.接收短信

接受短信的目的是使客户可以通过发送短信的方式**==更改通讯号码(接收驾驶员坐标的号码)。==**该系统接收到短信后系统会触发`API_EVENT_ID_SMS_RECEIVED`事件，并在**==主任务==**进行处理。

```c
/*事件处理函数*/
void EventDispatch(API_Event_t* pEvent)
{
    /*信息接收 设定联系号码 短信格式为 change@号码*/
    case API_EVENT_ID_SMS_RECEIVED:
    PM_SleepMode(false);//退出低功耗模式
    SMS_Encode_Type_t encodeType = pEvent->param1;
    uint32_t contentLength = pEvent->param2;
    uint8_t* header = pEvent->pParam1;
    uint8_t* content = pEvent->pParam2;

    if(encodeType == SMS_ENCODE_TYPE_ASCII)
    {
        ChangePhoneNumber(content,contentLength);//修改通讯号码
    }
    PM_SleepMode(true);//进入低功耗模式
    break;
}
```

## 9.更改通讯号码

步骤：

- 判断接收到的短信是否为修改通讯号码的短信(屏蔽其他干扰信息)
- 判断通讯号码长度是否正确
- 修改程序缓冲区中的通讯号码
- 修改flash中存储的通讯号码(上电自动读取)
- 向客户返回信息(是否修改成功,否的话是什么原因?)

使用：

- 将一张可以用于发送接收短信、可以上网的SIM卡插入模块中
- 向该卡发送修改通讯号码的短信(格式为change@手机号码)
- 修改成功

代码

```c
/*修改通讯号码*/
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
```

## 10.存储更改的号码到flash

函数

```c
函数原型：
int32_t  API_FS_Open(
    PCSTR    fileName,
    uint32_t operationFlag,
    uint32_t mode
);

参数：
fileName -- 文件名
operationFlag -- 操作标记，比如只读、只写等
mode -- 模式，文件属性
    
返回值：
如果成功返回大于等于零的文件描述符，否则返回错误代码
 
作用：
打开文件
```

```c
函数原型：
int32_t  API_FS_Close(
    int32_t fd
);

参数：
fd -- 文件描述符

返回值：
成功返回0，否则返回错误代码

作用：
关闭文件
```

```c
函数原型：
int32_t  API_FS_Write(
    int32_t  fd,
    uint8_t* pBuffer,
    uint32_t length
);

参数：
fd -- 文件描述符
pBuffer -- 待写入数据存放的位置
length -- 写入数据的长度

返回值：
成功写入的数据字节，错误则返回错误代码（负数）

作用：
向文件写入数据
```

代码

```c
/*修改flash中存储的号码*/
ret = SaveData(PHONE_NUMBER);
if(ret != true)
{
    change_flag = 3;
    Error();
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
```

## 11.上电读取flash中的手机号码

函数

```c
函数原型：
int32_t  API_FS_Read(
    int32_t  fd,
    uint8_t* pBuffer,
    uint32_t length
);

参数：
fd -- 文件描述符
pBuffer -- 读取到的数据存放的位置
length -- 读取数据的长度

返回值：
成功写入的数据字节，错误则返回错误代码（负数）

作用：
向文件写入数据
```

代码

```c
/* 初次使用作用为创建文件 之后为读取flash号码 */
ret = ReadData(PHONE_NUMBER);  
if(ret != true)
{
    Error();
    UART_Write(UART1,"read flash faild\r\n",strlen("read flash faild\r\n"));
}
```

## 12.低功耗模式

若ADC检测当前为非浸水状态，并且未触发一键报警，则系统进入低功耗模式，否则退出低功耗模式，进行地理坐标的获取以及短信的发送等操作。

函数

```c
/*进入低功耗模式*/
PM_SleepMode(true); 
/*退出低功耗模式*/
PM_SleepMode(flase); 
```

代码

```c
void ModeChange(void)
{
    /*状态更新*/
    if(saveFlag_new != saveFlag_old)
    {
        saveFlag_old = saveFlag_new;
        if(saveFlag_new==1)//落水
        {
            PM_SleepMode(false);//退出低功耗模式
        }
        else//无
        {
            PM_SleepMode(true);//进入低功耗模式 降低主频到32kHz 程序依然会运行
        }
    }
}
```

## 13.一键报警

为了检测到一键报警按键按下后（按键需外接），系统能迅速响应，本系统使用 **==外部中断==** 的方式检测按键是否按下。当检测到按键按下时，**==停止ADC的采集==**，并周期性闪烁激光灯，向家属手机号码发送位置信息。

### 停止ADC采集

函数

```c
函数原型：
void OS_StopTask(
    HANDLE pHTask);

参数：
pHTask -- 任务句柄

作用：
停止任务执行
```

### 按键检测

#### 初始化

```c
/*宏定义*/
#define KEY_PIN  GPIO_PIN6//连接报警按键引脚（可触发中断的引脚有GPIO0~GPIO7）

/*配置*/
GPIO_config_t gpio_config_t;
gpio_config_t.pin = KEY_PIN;//一键报警
gpio_config_t.mode = GPIO_MODE_INPUT_INT;//配置为中断方式
gpio_config_t.intConfig.type = GPIO_INT_TYPE_FALLING_EDGE;//下降沿捕获
gpio_config_t.intConfig.debounce = 15;//15ms消抖
gpio_config_t.intConfig.callback = OnPinFalling;//指定回调函数

/*初始化*/
GPIO_Init(gpio_config_t);
```

#### 回调函数

```c
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
```

# 安信可官方文档记录

（该内容源于安信可官方文档，仅用于记录方便查看）

## 开发环境搭建

SDK下载：[GPRS_C_SDK_V2112.7z](https://github.com/Ai-Thinker-Open/GPRS_C_SDK/releases/download/V2.112/GPRS_C_SDK_V2112.7z)  

安装（**==完成后请确保环境变量配置正确！==**）：
![在这里插入图片描述](https://img-blog.csdnimg.cn/244d5476010c4949841936e631dfa802.png#pic_center)

## 目录结构

![在这里插入图片描述](https://img-blog.csdnimg.cn/f992b907ecba4536b1dd4019d7bb60e8.png#pic_center)

## 新建工程

新建的工程结构如下图所示：

![在这里插入图片描述](https://img-blog.csdnimg.cn/709964c9845d41a3aa05af4360f71f2b.png#pic_center)


### 1.新建工程文件夹，并新建Makefile文件

注意Makefile只是首字母大写，或者全是小写，填入以下内容：

```makefile
# Name of the module
LOCAL_NAME := demo/first

# List of submodules which contain code we need to include in the final lib
LOCAL_API_DEPENDS := \

LOCAL_ADD_INCLUDE := include\
                    include/std_inc \
                    include/api_inc \


# Set this to any non-null string to signal a module which 
# generates a binary (must contain a "main" entry point). 
# If left null, only a library will be generated.
IS_ENTRY_POINT := no

## ------------------------------------ ##
##     Add your custom flags here          ##
## ------------------------------------ ##
MYCFLAGS += 

## ------------------------------------- ##
##    List all your sources here           ##
## ------------------------------------- ##
C_SRC := ${notdir ${wildcard src/*.c}}

## ------------------------------------- ##
##  Do Not touch below this line         ##
## ------------------------------------- ##
include ${SOFT_WORKDIR}/platform/compilation/cust_rules.mk
```

这里`LOCAL_NAME := demo/first`填**==文件夹路径==**，`LOCAL_ADD_INCLUDE`是包含的**==头文件路径==**

### 2.新建src文件夹,并新建.c文件

HelloWorld示例代码可以在工程目录`demo/first/src/demo_first.c`中找到，主要要做的事情是:**==编写程序入口，并创建主任务，用来接收来自底层的事件==**,同时建立一个任务,不停打印信息

代码入口及主任务：

```c
#include "stdbool.h"
#include "stdint.h"

#include "api_os.h"
#include "api_debug.h"
#include "api_event.h"

#define MAIN_TASK_STACK_SIZE    (2048 * 2)
#define MAIN_TASK_PRIORITY      0
#define MAIN_TASK_NAME          "Main Test Task"

static HANDLE mainTaskHandle = NULL;

void first_Main(void)
{
    mainTaskHandle = OS_CreateTask(MainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&mainTaskHandle);
}
```

每个程序都有一个入口，这里的入口是一个函数： `void first_Main(void)`，返回值和参数都是void，下划线左边 **==必须==** 是文件夹的名字，这里是first，下划线右半边 **==必须==** 是Main，首字母大写。 

调用`OS_CreateTask`来创建一个任务，具体参数含义可以见OS部分，这里先不阐述。调用`OS_SetUserMainHandle`将刚刚创建的任务指针（句柄）传给底层，让底层知道给那个任务发送事件，**==这个是必须的==**。

```c
#define SECOND_TASK_STACK_SIZE    (2048 * 2)
#define SECOND_TASK_PRIORITY      1
#define SECOND_TASK_NAME          "Second Test Task"

static HANDLE secondTaskHandle = NULL;

void SecondTask(void *pData)
{
    while(1)
    {
        Trace(1,"Hello GPRS ");
        OS_Sleep(3000);
    }
}

void EventDispatch(API_Event_t* pEvent)
{
    switch(pEvent->id)
    {
        default:
            break;
    }
}

void MainTask(void *pData)
{
    API_Event_t* event=NULL;

    secondTaskHandle = OS_CreateTask(SecondTask,
        NULL, NULL, SECOND_TASK_STACK_SIZE, SECOND_TASK_PRIORITY, 0, 0, SECOND_TASK_NAME);

    while(1)
    {
        if(OS_WaitEvent(mainTaskHandle, (void**)&event, OS_TIME_OUT_WAIT_FOREVER))
        {
            EventDispatch(event);
            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}
```

主任务`MainTask`中又新建了一个任务`SecondTask`，这个任务中做的事情就是每间隔3秒钟打印一句`Hello GPRS`，打印信息可以在coolwatcher的tracer工具中看到；

然后主任务阻塞等待来自底层的事件，并在`EventDispatch`函数中处理，这里暂时没有处理任何事件，所有事件可以在`api_event.h`中找到，每个事件的参数也有注释

## 编译和烧入

保证环境可以使用后，在GPRS_C_SDC目录下，Ctrl+Shift+鼠标右键打开powershell窗口，在命令行中使用`build.bat`脚本来编译工程，有以下参数： 

- 使用 `./build.bat $PROJ`来编译你的应用模块，如 `./build.batsh app` 则是编译app目录下的源码
- 使用 `./build.bat demo $PROJ` 来编译demo目录下的特定例程，比如`./build.bat demo gpio`
- 使用 `./build.bat clean $PROJ` 清除`$PROJ`目录的中间文件
- 使用 `./build.bat clean all` 清除所有中间文件
- 使用 `./build.bat demo $PROJ release` 来生成release版本，比如`./build.bat demo gpio release`， 如果最后一个参数不是`release`，则默认是`debug`版本， `debug`版本在死机后会停止运行并可以使用GDB调试，而**release版本加入了看门狗功能，在死机时会自动重启系统**，所以**实际投入使用时请使用release版本以防止出现bug时死机**，测试时使用debug版本

**比如：**

```
./build.bat demo gpio
```

进行编译，编译会生成一个`build`目录，编译完成会在`hex`目录下生成两个格式为`lod`的文件，这就是我们用来下载到开发板的目标文件

**目标文件说明：**

两个文件一大一小，在第一次下载时需要下载大文件(`*_B*.lod`)即可，第二次和以后为了减少下载时间都只需要下载小文件(`*_flash.lod`)即可，更新SDK后需要重新下载大文件。

**烧入：**

![在这里插入图片描述](https://img-blog.csdnimg.cn/e69d4f4551f6467781ba2b0255a87c20.png#pic_center)

![在这里插入图片描述](https://img-blog.csdnimg.cn/bd2d42e265d44f9b99ab8c95999df555.png#pic_center)


![在这里插入图片描述](https://img-blog.csdnimg.cn/683179439b224fe89ad056bc0415ddb7.png#pic_center)


- 如果第一次使用，需要点击DRY图标，选择名字包含`8955 ramrun spi32m .lod`的文件（在工程`platform/ramrun`下或者coolwatcher `cooltools\chipgen\Modem2G\toolpool\plugins\fastpf\flash_programmers`目录下）。 只需要设置一次，后面都会记住的
- 点击下图中的lod图标，选择编译生成的固件，在工程hex目录下`.lod`格式的文件，首次下载下载文件体积较大的`.lod`格式文件，后面为了加快下载速度只需要下载体积较小的固件文件即可
- 点击download图标，进行下载，正常效果如下图，右下角Command栏会有进度标志

![在这里插入图片描述](https://img-blog.csdnimg.cn/ee32c0857edc484ebdac0aef5a5b4a6a.png#pic_center)


## 代码风格

统一的代码风格方便阅读，对于任何一份代码来说都十分重要，每个人的代码风格不尽相同，但是多人在开发同一份代码时一定要统一风格，一份简洁干净的代码会让编码更加顺利舒畅~

CSDK使用统一的代码风格，简要概括为以下几个点：

### 1. 函数命名

模块API使用`大写功能模块_大驼峰命名`，比如`AUDIO_MicOpen`，`SSL_Init`；普通函数使用大驼峰命名，比如：`EventDispatch`

入口函数特殊，使用`文件夹名字_Main`，比如`imei_Main`

### 2. 变量命名

小驼峰命名，比如：`int ballNumber;`

### 3. 宏定义

大写+下划线，比如：`#define BUFFER_MAX_LENGTH 10`

### 4. 文件编码格式

文件编码统一为`UTF-8`无BOM格式

### 5. tab还是空格？

不使用tab，编辑器设置tab为4空格替换

### 6. 单词缩写

- 对于一些业界熟知的单词可以尽量使用缩写，比如`src`、`tmp`等，
- 除了这些之外，为保证代码的可读性，务必使用全称，特别是一些专业术语，尽量让没有接触过这个单词的人通过搜索引擎能够搜到这个单词的含义。

比如： `NetworkLocation`，如果缩写成`NL`、`NetLo`，相信就算是对这个单词非常熟悉的人也得去看代码的上下文才能慢慢才到其中的含义！如果去网上搜索这几个缩写，也不可能得到`NetworkLocation`！

**所以，为了让代码更简洁清晰易读，不一定是要让代码单词缩写为更简单的形式，相反的是全称的单词往往更易读。**

- 另外，有些单词需要进行解释，需要适量进行注释

### 7. 注释

- 注释可以使用`/**/`或者`//`进行注释，失效的代码尽量使用`/**/`注释
- 需要加入到自动生成文档的说明用

```
//////////////
///这里写注释
//////////////
```

或者

```
/**
  *这里写注释
  *
  */
```

- 文件注释或函数等注释可以适当加相关信息，比如文件：

```
/**
  *@file header.h
  *@author nick name
  */
```

或函数

```
/**
  *@brief 这里写简要说明，@brief可以省略,省略则将简要说明写在注释开头
  *@param a 求和参数1
  *@param b 求和参数2
  *@return  返回和
  */
```



## 部分系统函数介绍

### OS_SetUserMainHandle

```c
函数原型：
void OS_SetUserMainHandle(HANDLE* appMainHandle);

参数：
appMainHandle -- 创建的主任务返回的指针（句柄）

返回值：
无

作用：
设置主任务，主任务用来接收来自底层的消息，这个函数必须在开机的时候创建主任务后立即调用，否则可能导致底层无法向应用层发布消息
```

### OS_CreateTask

```c
函数原型：
HANDLE OS_CreateTask(
    PTASK_FUNC_T pTaskEntry,
    PVOID pParameter,
    PVOID pStackAddr,
    UINT16 nStackSize,
    UINT8 nPriority,
    UINT16 nCreationFlags,
    UINT16 nTimeSlice,
    PCSTR pTaskName);
    
参数：
pTaskEntry -- 任务执行函数，PTASK_FUNC_T 类型
pParameter -- 需要传递给执行函数的参数
pStackAddr -- 自定义栈的地址，暂时不支持自定义，即值只能为`NULL`
nStackSize -- 栈大小
nPriority -- 任务优先级，>=0，每个任务优先级不同，值越小任务优先级越大
nCreationFlags -- OS_CREATE_DEFAULT/0：默认，创建任务后开始执行任务；OS_CREATE_SUSPENDED -- 创建任务后不执行任务，需要手动调用start函数开启任务运行
nTimeSlice -- 保留，值为0
pTaskName -- 任务名称

返回值：
创建的任务的指针（句柄），如果创建任务失败，则返回`NULL`

作用：
创建一个新的任务
```

### OS_StartTask

```c
函数原型：
void OS_StartTask(
    HANDLE pHTask,
    PVOID pParameter);
    
参数：
pHTask -- 任务句柄，OS_CreateTask的返回值
pParameter -- 需要传给任务函数的参数

返回值：
无

作用：
开始执行任务
```

### OS_StopTask

```c
函数原型：
void OS_StopTask(
    HANDLE pHTask);
    
参数：
pHTask -- 任务句柄，OS_CreateTask的返回值

返回值：
无

作用：
停止任务执行
```

### OS_Free

```c
函数原型：
bool  OS_Free   (PVOID pMemBlock);

参数：
pMemBlock -- 动态分配的内存块首地址

返回值：
是否释放成功

作用：
释放动态分配的内存
```

# 使用过程中遇到的问题

## 1.模块无法连网

官方AT指令:

```c
附着网络:
AT+CGATT=1 

设置PDP参数:
AT+CGDCONT=1,"IP","CMNET"

激活PDP，正确激活以后就可以上网了:
AT+CGACT=1,1

报错:
+CME ERROR: 3
```

解决：

- 用下列AT 指令

	```c
	设置自动选择APN:
	AT+CSTT="","","" \r
	
	激活移动场景，激活后能获取到IP:
	AT+CIICR\r
	
	查询IP，只有获取到IP后才能上网:
	AT+CIFSR\r
	```

## 2.无法发送中文短信

```c
AT+CMGS=20
\> 0001000D91685110365210F800080660A8597DFF01
+CMS ERROR: 500
```

原因：

- 未设置为PDU模式

	```c
	AT+CMGF=1                //设置为Text模式
	AT+CMGF=0                //设置为PDU模式
	```

解决：
*	用下列AT 指令
	```c
	AT+CMGF=0                //设置为PDU模式
	AT+CMGS=20              //发送指令，更改为对应PDU编码的长度
	 >0011000D91685150800576F70008C4044F60597D(+^z,十六进制的1A结束)//返回字符	串中有OK 发送成功
	```

PDU在线编码工具：

- <http://www.sendsms.cn/pdu/>

## 3.无法下载程序

原因：

- 文档中所说运行CSDTk目录下的config_env_admin.bat文件来设置环境变量，检查发现设置	的环境变量对应的路径是错的

解决：

- 手动更新环境变量后重启电脑

常规操作：

- 重启下载软件
- 检查串口是否被占用
- **==检查串口线是否接错！（UART1与UART_Download引脚不同）==**

## 4. 烧入程序后AT指令失效

原因：

- 烧入程序后固件被更改

解决：

- 重新烧入原厂固件

## 5.GPS定位报错

原因：

- GPS能够看得到卫星才能够返回，否则会返回GPS NOT FIX NOW

解决：

- 去室外等一段时间

## 6. GPS定位不精确

原因：

- 模组输出的位置是 **==WGS84坐标==**，如果使用百度地图，需要转换成 **==BD-09坐标==**，如果使用的腾讯地图、高德地图、google地图等，请先转换到 **==GCJ-02坐标==**（火星坐标），否则显示结果可能会产生巨大误差

解决：

- 坐标系在线转换：<https://tool.lu/coordinate>

  转换前GPS定位(误差2km左右)：
		![在这里插入图片描述](https://img-blog.csdnimg.cn/8f0a67a8a4bd43d78c351928a956f41c.jpeg#pic_center)
	
	转换后GPS定位(误差极小)：
		![在这里插入图片描述](https://img-blog.csdnimg.cn/6997cb8d4f05422d85217ab6e561c9aa.jpeg#pic_center)


	转换前4G基站定位(误差2km左右)：
			![在这里插入图片描述](https://img-blog.csdnimg.cn/d529c8b9b6d34afbbebedf737bc2255f.jpeg#pic_center)

	转换后4G基站定位(误差极小)：

	![在这里插入图片描述](https://img-blog.csdnimg.cn/5e61fedc081e4fc9bb9dfd38647c466b.jpeg#pic_center)


## 7. 代码内使用sin,cos,atan2,sqrt,fabs等c语言math库相关函数编译不通过

**==（无法解决）==**

尝试：

- 定位工程引用的头文件，发现是已经打包好的放在include目录下的，可能与原头文件有些差别，尝试引用原文件 **（无效、客服回答SDK不包含该库）**
- 查找如何用c语言实现相关函数 **（与使用原函数输出不相符）（未找到atan2实现）**

## 8.将原本发送的地球坐标更改为百度坐标

使用C语音代码转换（**==需要math库，无法解决==**）

调用API接口？(**==非盈利项目可以随意调用==**) 

使用云平台解决？（**==未解决==**）

## 9. 发送短信长度有限

尝试：

- 验证发现单次发送最大限度为69个字符

解决：

- 未找到有效方法，只好减少字符串长度，发送关键信息

## 10.串口输出英文正常，中文变乱码

**==（未解决，原本中文也正常）==**

尝试：

- 烧入仅有串口的例程依然是乱码
- 重烧出厂固件后烧入串口例程依旧乱码

## 11.接收到短信，串口断开，但模块无重启

极少出现，疑似供电问题：

![在这里插入图片描述](https://img-blog.csdnimg.cn/091bb33444fb470d8e60a2fc94a5398f.jpeg#pic_center)
## 12.代码中多任务时，程序卡住

现象：

- 当代码中为多任务时，有多个while(1)循环，如果while(1)循环内只有一些if的条件判	断，ADC检测，串口输出之类的函数，while会卡住不执行(串口无输出)。

解决：

- 当各循环中加入OS_Sleep(100)即睡眠100ms时，程序正常运行。Second任务函数中	while循环加入睡眠100ms函数，ADC任务函数中while循环不添加，则ADC中的串口不能正常打印，添加即可正常打印。

猜测：

- 和烧入的文件版本有关？有debug版本和release版本。

- 官方文档中提到的相关描述：

![在这里插入图片描述](https://img-blog.csdnimg.cn/12ec76f0410743e4a07ba43a7e63cd6a.jpeg#pic_center)

## 13.烧入release版本存在问题

(注释掉的串口打印，编译后烧入依然存在。并且和debug版本一样，while循环中不添加睡眠函数无法正常串口打印，执行程序)

烧入release版本现象：

![在这里插入图片描述](https://img-blog.csdnimg.cn/0c6dac6acd8f4ccc8769f5593eb72ae4.png#pic_center)


同样的代码debug版本现象：

![在这里插入图片描述](https://img-blog.csdnimg.cn/80f13fcc1a8d454d8910b6d5bf9c2edc.png#pic_center)


---
# 重要链接

[**安信可官方开发资料**](https://docs.ai-thinker.com/a9开发资料)
[**安信可A9G模块资料**](https://docs.ai-thinker.com/gprs/a9g)
[**A9G使用常见问题**](https://docs.ai-thinker.com/_media/gprs/docs/%E5%AE%89%E4%BF%A1%E5%8F%AFgps_gprs_a9g%E5%B8%B8%E8%A7%81%E9%97%AE%E9%A2%98faq.pdf)
[**GSM/GPRS AT指令操作示例**](https://docs.ai-thinker.com/gprs/examples)


---
# 源代码(转载请注明出处)
## GitHub
https://github.com/Octopus1633/A9G_SmartLifeJacket.git
## 百度云盘
![在这里插入图片描述](https://img-blog.csdnimg.cn/84e7f5607bcf4fad962c98477107dace.png#pic_center)

