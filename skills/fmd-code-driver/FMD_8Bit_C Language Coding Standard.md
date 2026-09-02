# FMD 辉芒微 8 位单片机 C 语言编码规范

> **版本**: V1.0  
> **日期**: 2024-06-24  
> **适用范围**: FMD 辉芒微 FT60E21x 系列及同类 8 位 MCU 的 C 语言开发  
> **参考文档**: FT60E21x-C语言_V1.1 官方示例代码、FT60E21x 数据手册 V1.04

---

## 目录

1. [前言](#1-前言)
2. [文件组织规范](#2-文件组织规范)
3. [源文件结构](#3-源文件结构)
4. [命名规范](#4-命名规范)
5. [注释规范](#5-注释规范)
6. [数据类型规范](#6-数据类型规范)
7. [函数规范](#7-函数规范)
8. [寄存器操作规范](#8-寄存器操作规范)
9. [特殊关键字与编译器语法](#9-特殊关键字与编译器语法)
10. [外设编程模板](#10-外设编程模板)
11. [代码格式规范](#11-代码格式规范)
12. [最佳实践与禁忌](#12-最佳实践与禁忌)

---

## 1. 前言

### 1.1 目的

本规范旨在统一 FMD 辉芒微 8 位单片机（如 FT60E21x 系列）的 C 语言编码风格，提高代码的可读性、可维护性和团队协作效率。规范内容来源于 FMD 官方 SDK 示例代码（FT60E21x-C语言_V1.1）的编码实践总结。

### 1.2 适用范围

- FMD FT60E21x 系列 8 位 RISC MCU
- FMD IDE 集成开发环境
- 适用于其他 FMD 8 位 MCU 系列（FT60Fxxx、FT61Exxx 等）

### 1.3 约束等级说明

| 标记 | 含义 |
|------|------|
| **【必须】** | 必须严格遵守，违反将导致编译错误或运行时异常 |
| **【推荐】** | 强烈建议遵循，有助于代码一致性和可维护性 |
| **【可选】** | 可根据实际情况选择，但同一项目内应保持统一 |

### 1.4 FMD 编译器与标准 C 的主要差异

| 差异点 | FMD 编译器 | 标准 ANSI C |
|--------|-----------|-------------|
| `#include` 语法 | `#include "xxx.h";`（**尾部带分号**） | `#include "xxx.h"`（无分号） |
| 二进制常量 | 支持 `0B01110001` | 不支持（C23前） |
| 位类型 | 支持 `bit` 关键字 | 不支持 |
| 中断函数 | `void interrupt ISR(void)` | 编译器特定 |
| 内建函数 | `NOP()`, `SLEEP()`, `CLRWDT()` | 无 |
| 源文件后缀 | `.C`（大写） | `.c`（小写） |

---

## 2. 文件组织规范

### 2.1 文件命名

**【必须】** 源文件使用大写 `.C` 后缀：

```
test_ft60e21x_IO.C        // 正确
test_ft60e21x_Timer0.C    // 正确
test_ft60e21x_io.c        // 错误：小写后缀
```

**【推荐】** 文件名格式：`<项目前缀>_<MCU型号>_<模块名>.C`

```
test_ft60e21x_IO.C        // IO 模块
test_ft60e21x_Timer0.C    // Timer0 模块
test_ft60e21x_PWM.C       // PWM 模块
test_ft60e21x_UART.C      // UART 模块
```

### 2.2 项目文件构成

**【必须】** 一个完整的 FMD 项目至少包含以下文件：

| 文件 | 说明 |
|------|------|
| `<name>.prj` | 项目文件（定义 MCU 型号、源文件列表） |
| `<name>.ini` | IDE 配置文件（编译器选项、字体等） |
| `<name>.C` | 主源文件 |

**【推荐】** 多文件项目目录结构：

```
project/
├── project.prj              // 项目文件
├── project.ini              // IDE 配置
├── main.C                   // 主程序
├── sys_init.C               // 系统初始化
├── timer.C                  // 定时器驱动
├── uart.C                   // 串口驱动
├── iic.C                    // IIC 驱动
└── spi.C                    // SPI 驱动
```

### 2.3 头文件引用

**【必须】** 每个源文件引用以下系统头文件（带分号）：

```c
#include    "SYSCFG.h";
#include    "FT60F21X.h";
```

> ⚠️ **FMD 编译器特有语法**：`#include` 指令末尾必须带分号 `;`，这是与标准 C 的重大差异。

---

## 3. 源文件结构

### 3.1 标准文件头

**【必须】** 每个源文件必须以标准文件头开始，格式如下：

```c
// Project: FT60E21X_IO.prj
// Device:  FT60E21X
// Memory:  PROM=1KX14, SRAM=64B, EEPROM=128
// Description: 当DemoPortIn为高电平时,DemoPortOut输出50Hz占空比50%的波形
//              当DemoPortIn为低电平时,DemoPortOut输出高电平

// RELEASE HISTORY
// VERSION     DATE         DESCRIPTION
// 1.1        24-03-04        修改文件头
//*****************************************************
```

文件头必须包含：
- **Project**: 项目名称
- **Device**: MCU 型号
- **Memory**: 存储器资源（PROM、SRAM、EEPROM）
- **Description**: 功能描述
- **RELEASE HISTORY**: 版本历史（VERSION / DATE / DESCRIPTION）
- **分隔线**: `//***...***`

### 3.2 源文件内容顺序

**【必须】** 源文件按以下顺序组织：

```
1. 文件头注释
2. #include 指令
3. 宏定义区（#define）
4. 全局变量 / 静态变量定义
5. 函数实现（按功能分组，被调用者在前）
6. main() 主函数（放在文件末尾）
```

**【推荐】** 各区域之间用注释分隔线标识：

```c
//***********************宏定义****************************
#define     DemoPortOut     PA4
#define     DemoPortIn      PA2

//***********************全局变量**************************
unsigned char   FCount;
unsigned char   ReadAPin;

//***********************函数实现**************************
```

### 3.3 完整文件结构示例

```c
// Project: FT60E21X_IO.prj
// Device:  FT60E21X
// Memory:  PROM=1KX14, SRAM=64B, EEPROM=128
// Description: IO输入输出控制演示
// RELEASE HISTORY
// VERSION     DATE         DESCRIPTION
// 1.1        24-03-04        修改文件头
//*****************************************************
#include    "SYSCFG.h";
#include    "FT60F21X.h";

//***********************宏定义****************************
#define     DemoPortOut     PA4
#define     DemoPortIn      PA2

//***********************全局变量**************************
// (如有需要)

/*-------------------------------------------------
 * 函数名：POWER_INITIAL
 * 功能：  上电系统初始化
 * 输入：  无
 * 输出：  无
 --------------------------------------------------*/
void POWER_INITIAL(void)
{
    OSCCON = 0B01110001;    // 16MHz 1:1
    INTCON = 0;             // 暂禁止所有中断
    // ...
}

/*-------------------------------------------------
 * 函数名：main
 * 功能：  主函数
 * 输入：  无
 * 输出：  无
 --------------------------------------------------*/
void main()
{
    POWER_INITIAL();        // 系统初始化

    while(1)
    {
        // 主循环
    }
}
```

---

## 4. 命名规范

### 4.1 宏定义命名

**【推荐】** 宏定义采用以下两种风格之一，同一项目内保持一致：

**风格一：大写下划线（传统风格）**
```c
#define     TXIO            PA4
#define     RXIO            PA2
#define     LED             PA4
#define     IRRIO           PA2
```

**风格二：大驼峰（官方示例常用风格）**
```c
#define     DemoPortOut     PA4
#define     DemoPortIn      PA2
#define     IRSendIO        PA4
```

**【推荐】** 状态/模式常量使用大写下划线带前缀：
```c
#define     Status_NOSend   0       // 不发送的状态
#define     Status_Head     1       // 发送引导码的状态
#define     Status_Data     2       // 发送数据的状态
```

**【推荐】** 时序常量使用大写下划线带模块前缀：
```c
#define     IRSend_HIGH_1   1       // 560uS
#define     IRSend_LOW_1    3       // 1680uS
#define     IRSend_HIGH_0   1       // 560uS
#define     IRSend_LOW_0    1       // 560uS
```

### 4.2 类型别名命名

**【推荐】** 类型别名使用小写：
```c
#define     uchar           unsigned char
#define     uint            unsigned int
#define     unchar          unsigned char
```

> **注意**：官方示例中同时存在 `uchar` 和 `unchar` 两种写法，建议项目中统一为一种。

### 4.3 函数命名

**【推荐】** 函数名采用以下风格之一，同一项目内保持一致：

**风格一：大写下划线（系统初始化类函数）**
```c
void POWER_INITIAL(void);       // 系统初始化
void TIMER0_INITIAL(void);      // 定时器0初始化
void TIMER2_INITIAL(void);      // 定时器2初始化
void WDT_INITIAL(void);         // 看门狗初始化
```

**风格二：驼峰命名（驱动接口函数）**
```c
void DelayUs(unsigned char Time);       // 微秒延时
void DelayMs(unsigned char Time);       // 毫秒延时
void DelayS(unsigned char Time);        // 秒延时
uchar SPI_RW(uchar data);               // SPI读写
void IIC_Start(void);                   // IIC起始信号
void IIC_Stop(void);                    // IIC停止信号
```

**风格三：模块前缀_下划线（外设驱动）**
```c
void SPI_Write(uint addr, uchar dat);
uchar SPI_Read(uint addr);
void IIC_Send_Byte(unsigned char txd);
unsigned char IIC_Read_Byte(void);
unsigned char IIC_Wait_Ack(void);
```

### 4.4 变量命名

**【推荐】** 全局变量使用驼峰或下划线命名：
```c
unsigned char   FCount;             // 帧计数
unsigned char   ReadAPin;           // PA口读取值
unsigned char   RXFLAG;             // 接收标志（全大写也是常见写法）
unsigned char   IRSendStatus;       // 红外发送状态
unsigned char   ReceiveFinish;      // 接收完成标志
unsigned int    SYSTime5S;          // 系统5秒计时
```

**【推荐】** 局部变量使用简短小写：
```c
unsigned char   a, b;               // 循环变量
unsigned char   i;                  // 循环索引
unsigned char   temp;               // 临时变量
unsigned int    temp;               // 临时变量（注意作用域）
```

**【推荐】** 位变量使用小驼峰前缀：
```c
bit     SendLastBit = 0;            // 最后一位发送标志
```

### 4.5 IO 引脚宏定义命名

**【推荐】** IO 引脚宏定义使用功能含义命名：
```c
#define     DemoPortOut     PA4     // 演示输出脚
#define     DemoPortIn      PA2     // 演示输入脚
#define     TXIO            PA4     // 串口发送脚
#define     RXIO            PA2     // 串口接收脚
#define     MISO            PA4     // SPI 主入从出
#define     MOSI            PA3     // SPI 主出从入
#define     SCK             PA2     // SPI 时钟
#define     CS              PA1     // SPI 片选
#define     IIC_SCL         PA4     // IIC 时钟
#define     IIC_SDA         PA2     // IIC 数据
#define     LED             PA4     // LED 指示灯
#define     IRRIO           PA2     // 红外接收脚
```

---

## 5. 注释规范

### 5.1 函数注释

**【必须】** 每个函数必须有注释块，使用 `/*---*/` 格式，内容包含：

```c
/*-------------------------------------------------
 * 函数名：<函数名>
 * 功能：  <功能描述>
 * 输入：  <参数说明>（无参数时写"无"）
 * 输出：  <返回值说明>（无返回值时写"无"）
 --------------------------------------------------*/
void FunctionName(void)
{
    // ...
}
```

**完整示例：**

```c
/*-------------------------------------------------
 * 函数名：DelayUs
 * 功能：  短延时函数 --16M-4T--大概快1%左右.
 * 输入：  Time 延时时间长度 延时时长Time*2 Us
 * 输出：  无 
 -------------------------------------------------*/
void DelayUs(unsigned char Time)
{
    unsigned char a;
    for(a = 0; a < Time; a++)
    {
        NOP();
    }
}
```

### 5.2 寄存器配置注释

**【必须】** 对寄存器的配置值必须逐位注释其含义：

```c
OPTION = 0B01001000;
// Bit5: T0CS Timer0时钟源选择 
//   1-外部引脚电平变化T0CKI  0-内部时钟(FOSC/2)
// Bit4: T0CKI引脚触发方式 1-下降沿 0-上升沿
// Bit3: PSA 预分频器分配位 0-Timer0 1-WDT 
// Bit[2:0]: PS 8个预分频比 000-1:1 ... 111-1:256
```

**【推荐】** 对于多选项配置，可以用表格形式注释：

```c
PSRCA = 0;
// 00:   3mA
// 01/10:6mA
// 11:   24mA
// Bit[3:2]: 控制PA5源电流
// Bit[1:0]: 控制PA4源电流
```

### 5.3 行内注释

**【推荐】** 行内注释使用 `//`：

```c
OSCCON = 0B01110000;    // IRCF=111=16MHz/4T=4MHz, 0.25us
INTCON = 0;             // 暂禁止所有中断
TMR0 = 0;               // 计数初值，每个指令周期++，最大255，后溢出
```

### 5.4 代码段分隔注释

**【推荐】** 大段代码之间使用分隔注释：

```c
//***********************宏定义****************************

//***********************全局变量**************************

//***********************函数实现**************************
```

---

## 6. 数据类型规范

### 6.1 基本类型

**【必须】** FMD 8位 MCU 支持以下基本数据类型：

| 类型 | 位宽 | 取值范围 | 说明 |
|------|------|---------|------|
| `bit` | 1 bit | 0 或 1 | FMD 特有，用于位变量 |
| `unsigned char` | 8 bit | 0 ~ 255 | 最常用类型 |
| `unsigned int` | 16 bit | 0 ~ 65535 | 双字节无符号整型 |
| （不支持 signed） | - | - | 编译器通常仅支持无符号类型 |

### 6.2 类型别名

**【推荐】** 使用类型别名提高可读性和可移植性：

```c
#define     uchar       unsigned char
#define     uint        unsigned int
#define     unchar      unsigned char      // 另一种风格，项目中统一
```

> ⚠️ 官方示例中 `uchar` 和 `unchar` 混用，建议统一使用 `uchar`。

### 6.3 bit 类型

**【必须】** `bit` 是 FMD 编译器特有的 1 位数据类型，用于定义位变量：

```c
bit     SendLastBit = 0;        // 定义并初始化位变量
bit     flag;                    // 定义位变量（未初始化）

SendLastBit = 1;                 // 赋值
if(SendLastBit)                  // 条件判断
{
    // ...
}
```

**【推荐】** 位变量使用场景：
- 标志位（flag）
- 布尔状态量
- 单个 IO 口状态缓存

**【禁止】** 位变量不能用于：
- 数组元素
- 结构体成员
- 指针指向

### 6.4 volatile 关键字

**【必须】** 在中断服务函数和主循环之间共享的变量，必须使用 `volatile` 声明：

```c
volatile unsigned int temp;             // 中断中修改，主循环中读取

// 以下变量在中断和主循环中共享，理论上也应加 volatile
volatile unsigned char IRSendStatus;
volatile unsigned char ReceiveFinish;
```

> 来源：`test_ft60e21x_MSCK.C:12`

### 6.5 类型转换

**【推荐】** 16位地址拆分时，使用显式类型转换：

```c
// 正确：发送16位地址的高8位和低8位
SPI_RW((uchar)((addr) >> 8));
SPI_RW((uchar)addr);

// 16位数据拼接
temp = (SOSCPRH & 0x0F) << 8;
temp |= SOSCPRL;
```

---

## 7. 函数规范

### 7.1 函数定义格式

**【必须】** 函数定义格式：

```c
/*-------------------------------------------------
 * 函数名：<函数名>
 * 功能：  <描述>
 * 输入：  <参数>
 * 输出：  <返回值>
 --------------------------------------------------*/
<返回类型> <函数名>(<参数列表>)
{
    // 函数体
}
```

**说明：**
- 返回类型与函数名在同一行
- 左大括号 `{` 另起一行
- 无参数时使用 `void`
- 使用 Tab 缩进

### 7.2 无参函数

**【必须】** 无参函数必须显式使用 `void`：

```c
void POWER_INITIAL(void)        // 正确
{
    // ...
}

void POWER_INITIAL()            // 不推荐
{
    // ...
}
```

### 7.3 main 函数

**【必须】** `main` 函数放在源文件末尾，使用标准格式：

```c
/*-------------------------------------------------
 * 函数名：main
 * 功能：  主函数
 * 输入：  无
 * 输出：  无
 --------------------------------------------------*/
void main()
{
    POWER_INITIAL();            // 系统初始化
    // 外设初始化...

    while(1)
    {
        // 主循环：查询、处理、状态机等
    }
}
```

### 7.4 中断服务函数

**【必须】** 中断服务函数使用 `interrupt ISR` 声明，所有中断共用一个入口：

```c
/*-------------------------------------------------
 * 函数名：interrupt ISR
 * 功能：  中断处理
 * 输入：  无
 * 输出：  无
 --------------------------------------------------*/
void interrupt ISR(void)
{
    // 定时器0中断处理
    if(T0IE && T0IF)
    {
        T0IF = 0;               // 清中断标志
        // ...中断处理逻辑
    }

    // PA电平变化中断处理
    if(PAIE && PAIF)
    {
        ReadAPin = PORTA;       // 读PORTA清PAIF标志
        PAIF = 0;
        // ...中断处理逻辑
    }

    // 外部中断处理
    if(INTE && INTF)
    {
        INTF = 0;
        INTE = 0;
        // ...中断处理逻辑
    }

    // Timer2中断处理
    if(TMR2IE && TMR2IF)
    {
        TMR2IF = 0;
        // ...中断处理逻辑
    }
}
```

> ⚠️ **重要**：FMD MCU 所有中断共享一个中断向量，通过查询各中断标志位来判断中断源。中断标志位必须软件清零。

### 7.5 延时函数模板

**【推荐】** 使用以下标准延时函数模板（基于 16MHz/4T 系统）：

```c
/*-------------------------------------------------
 * 函数名：DelayUs
 * 功能：  微秒级短延时（16M-4T，大约快1%）
 * 输入：  Time 延时时间长度 延时时长 Time*2 μs
 * 输出：  无
 -------------------------------------------------*/
void DelayUs(unsigned char Time)
{
    unsigned char a;
    for(a = 0; a < Time; a++)
    {
        NOP();
    }
}

/*-------------------------------------------------
 * 函数名：DelayMs
 * 功能：  毫秒级短延时（16M-4T，大约快1%）
 * 输入：  Time 延时时间长度 延时时长 Time ms
 * 输出：  无
 -------------------------------------------------*/
void DelayMs(unsigned char Time)
{
    unsigned char a, b;
    for(a = 0; a < Time; a++)
    {
        for(b = 0; b < 5; b++)
        {
            DelayUs(98);        // 快1%
        }
    }
}

/*-------------------------------------------------
 * 函数名：DelayS
 * 功能：  秒级短延时
 * 输入：  Time 延时时间长度 延时时长 Time 秒
 * 输出：  无
 -------------------------------------------------*/
void DelayS(unsigned char Time)
{
    unsigned char a, b;
    for(a = 0; a < Time; a++)
    {
        for(b = 0; b < 10; b++)
        {
            DelayMs(100);
        }
    }
}
```

---

## 8. 寄存器操作规范

### 8.1 SFR 直接访问

**【必须】** FMD MCU 的特殊功能寄存器（SFR）可直接通过寄存器名访问：

```c
OSCCON = 0B01110000;        // 直接赋值
INTCON = 0;                 // 清零
TMR0 = 0;                   // 赋初值
```

### 8.2 位操作

**【推荐】** 单个位的置位/清零操作：

```c
// 置位（Set bit）
GIE = 1;                    // 位变量直接赋值
T0IE = 1;                   // Timer0中断使能
TMR2IE = 1;                 // Timer2中断使能

// 清零（Clear bit）
T0IF = 0;                   // Timer0中断标志清零
TMR2IF = 0;                 // Timer2中断标志清零
INTF = 0;                   // 外部中断标志清零
```

**【推荐】** 使用移位操作配置特定位：

```c
// 单位置位
TRISA = 1 << 2;             // PA2输入（Bit2=1）

// 使用 |= 组合置位（带掩码）
EECON1 |= 0x34;             // 置位WREN1, WREN2, WREN3

// 使用 &= ~() 清零
INTCON &= ~(1 << 2);        // T0IF = 0
```

### 8.3 二进制常量

**【必须】** FMD 编译器支持二进制常量，寄存器配置推荐使用二进制表示以提高可读性：

```c
OSCCON  = 0B01110000;       // IRCF=111, 16MHz, 1:1
OSCCON  = 0B01110001;       // IRCF=111, 16MHz, 1:1（另一种配置）
INTCON  = 0B00000000;       // 禁止所有中断
OPTION  = 0B01001000;       // WDT MODE, PS=000=1:1
T2CON0  = 0B00000001;       // T2预分频1:4，后分频1:1
T2CON1  = 0B00001000;       // 指令周期时钟源
```

### 8.4 十六进制常量

**【推荐】** 与外部设备通信（IIC/SPI/UART）的地址和数据使用十六进制：

```c
SPI_RW(0X06);               // 写允许命令
SPI_RW(0X05);               // 读取状态命令
SPI_RW(0X04);               // 写禁止命令
SPI_RW(0X03);               // 读取命令
SPI_RW(0X02);               // 写命令

IIC_Send_Byte(0xA0);        // IIC设备写地址
IIC_Send_Byte(0xA1);        // IIC设备读地址

WDTCON = (0X05 << 1) | (1 << 0);  // 混合使用
```

> ⚠️ FMD 编译器同时支持 `0x` 和 `0X` 两种十六进制前缀，推荐统一使用大写 `0X`。

### 8.5 寄存器配置步骤模板

**【推荐】** 外设初始化函数按以下步骤组织：

```
1. 关闭外设使能位
2. 配置控制寄存器
3. 配置参数寄存器
4. 配置相关 IO 口
5. 清除中断标志
6. 使能外设
7. 使能相关中断（可选）
```

---

## 9. 特殊关键字与编译器语法

### 9.1 #include 带分号

**【必须】** FMD 编译器中 `#include` 指令末尾必须加 `;` 分号：

```c
#include    "SYSCFG.h";         // 正确：FMD 编译器要求
#include    "FT60F21X.h";       // 正确

#include    "SYSCFG.h"          // 错误：缺少分号
```

### 9.2 bit 类型

**【必须】** `bit` 是 FMD 编译器特有的 1 位数据类型：

```c
bit SendLastBit = 0;            // 位变量定义与初始化
bit flag;                        // 位变量定义

TXIO = (bit)1;                   // 位类型强制转换
```

### 9.3 内建函数

**【必须】** FMD 编译器提供以下内建函数，无需声明即可使用：

| 函数 | 功能 | 来源 |
|------|------|------|
| `NOP()` | 空操作，延时一个指令周期 | 多文件使用 |
| `SLEEP()` | 进入睡眠/低功耗模式 | SLEEP/INT/PA_INT |
| `CLRWDT()` | 清除看门狗定时器 | WDT/SLEEP |

```c
// 使用示例
NOP();              // 延时填充
SLEEP();            // 进入睡眠
CLRWDT();           // 喂狗
```

### 9.4 二进制常量前缀

**【必须】** FMD 编译器支持 `0B` 二进制常量前缀：

```c
OSCCON = 0B01110000;            // 8位二进制常量
```

### 9.5 中断函数声明

**【必须】** 中断函数使用 `interrupt` 关键字：

```c
void interrupt ISR(void)        // 正确
{
    // ...
}

void interrupt ISR()            // 不推荐：缺少 void
{
    // ...
}
```

---

## 10. 外设编程模板

### 10.1 GPIO 初始化模板

```c
/*-------------------------------------------------
 * 函数名：POWER_INITIAL
 * 功能：  上电系统初始化
 * 输入：  无
 * 输出：  无
 --------------------------------------------------*/
void POWER_INITIAL(void)
{
    OSCCON = 0B01110000;        // IRCF=111=16MHz/4T=4MHz, 0.25us
    INTCON = 0;                 // 暂禁止所有中断

    PORTA = 0B00000000;         // PA口输出电平（1:高 0:低）
    TRISA = 0B00000100;         // PA输入输出配置（1:输入 0:输出）
    WPUA = 0B00000100;          // PA端口上拉控制  (1:使能上拉 0:关闭)

    PSRCA = 0;                  // PA源电流控制
    // 00:   3mA (或 4mA)
    // 01/10:6mA (或 8mA)
    // 11:   24mA (或 28mA)
    // Bit[3:2]: 控制PA5源电流
    // Bit[1:0]: 控制PA4源电流

    PSINKA = 0;                 // PA灌电流控制
    // Bit[1:0]: 控制PA5和PA4 0:灌电流最小 1:灌电流最大

    OPTION = 0B00001000;        // Bit3=1 WDT MODE, PS=000=1:1
    MSCON = 0B00000000;         // 其他配置
}
```

**IO 口方向切换（动态）：**

```c
#define     SDA_OUT     TRISA2 = 0      // SDA 输出模式
#define     SDA_IN      TRISA2 = 1      // SDA 输入模式
```

### 10.2 Timer0 初始化模板

```c
/*----------------------------------------------------
 * 函数名：TIMER0_INITIAL
 * 功能：  初始化设置定时器0
 * 设置TMR0定时时长 = (1/系统时钟频率)*指令周期*预分频值*TMR0
 *                  = (1/16000000)*4*256*255 = 16ms
 ----------------------------------------------------*/
void TIMER0_INITIAL(void)
{
    T0ON = 0;                       // 先关闭定时器
    TMR0 = 0;                       // 计数初值，每个指令周期++，最大255后溢出
    T0CON0 = 0B00000000;            // Bit3: 定时器0使能位
                                    // Bit[1:0]: 00=指令周期 01=HIRC
    OPTION = 0B00000111;
    // Bit5: T0CS Timer0时钟源选择
    //   1-外部引脚电平变化T0CKI 0-内部时钟(FOSC/2)
    // Bit4: T0CKI引脚触发方式 1-下降沿 0-上升沿
    // Bit3: PSA 预分频器分配位 0-Timer0 1-WDT
    // Bit[2:0]: PS 8个预分频比 111 - 1:256

    T0ON = 1;                       // 启动定时器
}
```

### 10.3 Timer2 初始化模板

```c
/*-------------------------------------------------
 * 函数名：TIMER2_INITIAL
 * 功能：  初始化设置定时器2
 * 设置Timer2定时时长 = (1/系统时钟频率)*4*预分频值*后分频值*PR2
 *                    = (1/16000000)*4*4*1*200 = 200us
 -------------------------------------------------*/
void TIMER2_INITIAL(void)
{
    T2CON0 = 0B00000001;
    // Bit[6:3]=0000, T2时钟后分频比1:1
    // Bit[1:0]=01, T2时钟预分频比1:4

    T2CON1 = 0B00001000;            // Bit[2:0] 000=指令周期 100=HIRC
    TMR2H = 0;
    TMR2L = 0;                      // TMR2赋初值

    PR2H = 0;
    PR2L = 200;                     // 设置PR2=200

    TMR2IF = 0;                     // 清TIMER2中断标志
    TMR2IE = 1;                     // 使能TIMER2的中断
    TMR2ON = 1;                     // 使能TIMER2启动

    PEIE = 1;                       // 使能外设中断
    GIE = 1;                        // 使能全局中断
}
```

### 10.4 PWM 初始化模板

```c
/*-------------------------------------------------
 * 函数名：PWM1_INITIAL
 * 功能：  PWM1初始化函数
 * 设置：  周期 = (PR2+1)*Tt2ck*TMR2预分频
 *              = (99+1)*0.25*4 = 100us
 *         脉宽 = P1xDT*Tt2ck*TMR2预分频
 *              = 50*0.25*4 = 50us
 --------------------------------------------------*/
void PWM1_INITIAL(void)
{
    T2CON0 = 0B00000001;            // T2预分频1:4，后分频1:1
    // Bit7: 0=周期结束后正常更新 1=缓冲值更新
    // Bit[6:3]: 后分频比选择 0000-1:1 ... 1:16
    // Bit2: 0=关闭定时器2 1=打开定时器2
    // Bit[1:0]: 定时器2预分频选择 00:1 01:4 1x:16

    T2CON1 = 0B00000000;            // T2时钟来自系统时钟，PWM连续模式
    // Bit4: PWM模式选择 0:连续模式 1:单脉冲模式
    // Bit3: 0:PWM模式 1:蜂鸣器模式
    // Bit[2:0]: Timer2时钟源选择

    TMR2H = 0;
    TMR2L = 0;

    PR2H = 0;
    PR2L = 99;                      // 周期寄存器

    P1ADTH = 0;
    P1ADTL = 50;                    // 占空比寄存器

    P1OE = 0B00000001;              // P1A0输出使能
    P1POL = 0B00000000;             // P1A0高电平有效
    P1CON = 0B00000000;             // PWM1 死区时间设置

    TMR2IF = 0;                     // 清TIMER2中断标志
    TMR2IE = 1;                     // 使能TIMER2的中断
    TMR2ON = 1;                     // 打开定时器2
    PEIE = 1;                       // 使能外设中断
    GIE = 1;                        // 使能全局中断
}
```

### 10.5 UART 软件模拟模板

FMD FT60E21x 无硬件 UART，使用 IO + Timer0 软件模拟：

```c
/*-------------------------------------------------
 * 函数名：WByte
 * 功能：  UART发送一个字节
 * 输入：  input 要发送的数据
 * 输出：  无
 --------------------------------------------------*/
void WByte(uchar input)
{
    uchar i = 8;

    // 发送起始位
    TXIO = 1;
    TMR0 = Bord;
    T0IE = 1;
    WaitTF0();
    TXIO = 0;
    WaitTF0();

    // 发送8位数据位（先传低位）
    while(i--)
    {
        if(input & 0x01)
            TXIO = 1;
        else
            TXIO = 0;
        WaitTF0();
        input = input >> 1;
    }

    // 发送停止位
    TXIO = (bit)1;
    T0IE = 0;
}

/*-------------------------------------------------
 * 函数名：RByte
 * 功能：  UART接收一个字节
 * 输入：  无
 * 输出：  接收到的数据
 --------------------------------------------------*/
uchar RByte()
{
    uchar Output = 0;
    uchar i = 8;

    T0IE = 1;
    TMR0 = Bord;
    WaitTF0();
    T0IE = 1;
    TMR0 = Bord;
    WaitTF0();                  // 等过起始位

    while(i--)
    {
        Output >>= 1;
        if(RXIO)
            Output |= 0x80;     // 先收低位
        WaitTF0();
    }
    T0IE = 0;
    return Output;
}

/*-------------------------------------------------
 * 函数名：WaitTF0
 * 功能：  查询定时器溢出（等待波特率定时）
 * 输入：  无
 * 输出：  无
 --------------------------------------------------*/
void WaitTF0(void)
{
    while(T0IE);                // 等待中断关闭定时器
    T0IE = 1;                   // 重新启动定时器
}
```

> **波特率计算**（9600bps @ 16MHz/4T）：
> `Bord = 49`，定时时长 = (1/16M)*4*2*49 = 24.5μs ≈ 104μs中断间隔

### 10.6 SPI 主机模拟模板

```c
/*-------------------------------------------------
 * 函数名：SPI_RW
 * 功能：  主机输出并接收一个字节
 * 输入：  data 待发送数据
 * 输出：  接收到的数据
 --------------------------------------------------*/
uchar SPI_RW(uchar data)
{
    uchar i;
    for(i = 0; i < 8; i++)
    {
        if(data & 0x80)
            MOSI = 1;
        else
            MOSI = 0;
        NOP();
        data <<= 1;
        SCK = 1;
        NOP();
        if(MISO)
            data |= 0x01;
        else
            data &= 0xFE;
        NOP();
        SCK = 0;
    }
    return data;
}

/*-------------------------------------------------
 * 函数名：SPI_Read
 * 输入：  16位地址 addr
 * 返回：  读取的数据
 * 说明：  从25C64指定地址读取一个字节
 --------------------------------------------------*/
uchar SPI_Read(uint addr)
{
    uchar spidata;
    while(SPI_ReadStatus() & 0x01);     // 等待空闲
    CS = 0;
    SPI_RW(0X03);                        // 读取命令
    SPI_RW((uchar)((addr) >> 8));
    SPI_RW((uchar)addr);
    spidata = SPI_RW(0X00);
    CS = 1;
    return spidata;
}
```

### 10.7 IIC 主机模拟模板

```c
/*-------------------------------------------------
 * 函数名：IIC_Start
 * 功能：  产生IIC起始信号
 --------------------------------------------------*/
void IIC_Start(void)
{
    SDA_OUT;
    IIC_SDA = 1;
    IIC_SCL = 1;
    DelayUs(10);
    IIC_SDA = 0;                        // CLK高时DATA由高变低
    DelayUs(10);
    IIC_SCL = 0;                        // 钳住总线
    DelayUs(10);
}

/*-------------------------------------------------
 * 函数名：IIC_Stop
 * 功能：  产生IIC停止信号
 --------------------------------------------------*/
void IIC_Stop(void)
{
    SDA_OUT;
    IIC_SCL = 0;
    IIC_SDA = 0;
    DelayUs(10);
    IIC_SCL = 1;
    DelayUs(10);
    IIC_SDA = 1;                        // CLK高时DATA由低变高
    DelayUs(10);
}

/*-------------------------------------------------
 * 函数名：IIC_Send_Byte
 * 功能：  IIC发送一个字节
 * 输入：  txd 待发送数据
 --------------------------------------------------*/
void IIC_Send_Byte(unsigned char txd)
{
    unsigned char t;
    SDA_OUT;
    IIC_SCL = 0;
    for(t = 0; t < 8; t++)
    {
        if(txd & 0x80)
            IIC_SDA = 1;
        else
            IIC_SDA = 0;
        txd <<= 1;
        DelayUs(5);
        IIC_SCL = 1;
        DelayUs(5);
        IIC_SCL = 0;
        DelayUs(5);
    }
}

/*-------------------------------------------------
 * 函数名：IIC_Read_Byte
 * 功能：  IIC读取一个字节
 * 输出：  接收到的数据
 --------------------------------------------------*/
unsigned char IIC_Read_Byte(void)
{
    unsigned char i, receive = 0;
    SDA_IN;
    for(i = 0; i < 8; i++)
    {
        IIC_SCL = 0;
        DelayUs(5);
        IIC_SCL = 1;
        receive <<= 1;
        if(IIC_SDA) receive++;
        DelayUs(5);
    }
    IIC_NAck();
    return receive;
}
```

### 10.8 WDT 看门狗初始化模板

```c
/*-------------------------------------------------
 * 函数名：WDT_INITIAL
 * 功能：  初始化看门狗（32ms超时）
 * 说明：  wdt溢出时间 = (1/32000)*16位预分频值*8位预分频值
 *                    = (1/32kHz)*1024*1 = 32ms
 -------------------------------------------------*/
void WDT_INITIAL(void)
{
    CLRWDT();                               // 清看门狗
    WDTCON = (0X05 << 1) | (1 << 0);
    // Bit5:   0:LIRC  1:HIRC
    // Bit[4:1]: 预分频值 0X05 = 1:1024
    // Bit0:   1:使能WDT

    OPTION = 0;
    PSA = 1;                                // 预分频分配给WDT
}
```

### 10.9 EEPROM 读写模板

```c
/*-------------------------------------------------
 * 函数名：EEPROMread
 * 功能：  读EEPROM数据
 * 输入：  EEAddr 读取地址
 * 输出：  对应地址的数据
 --------------------------------------------------*/
unchar EEPROMread(unchar EEAddr)
{
    unchar ReEEPROMread;
    EEADR = EEAddr;
    RD = 1;
    NOP();
    NOP();
    NOP();
    NOP();
    ReEEPROMread = EEDAT;
    return ReEEPROMread;
}

/*-------------------------------------------------
 * 函数名：EEPROMwrite
 * 功能：  写数据到EEPROM
 * 输入：  EEAddr 写入地址, Data 写入数据
 * 输出：  无
 --------------------------------------------------*/
void EEPROMwrite(unchar EEAddr, unchar Data)
{
    GIE = 0;                                // 必须关闭中断！
    while(GIE);                             // 等待GIE为0
    EEADR = EEAddr;
    EEDAT = Data;
    EEIF = 0;
    EECON1 |= 0x34;                         // 置位WREN1, WREN2, WREN3
    WR = 1;                                 // 启动写操作
    NOP();
    NOP();
    NOP();
    NOP();
    while(WR);                              // 等待写入完成
    GIE = 1;                                // 恢复中断
}
```

> ⚠️ **关键约束**：写 EEPROM 前必须关闭全局中断 (`GIE = 0`)，并在写完成后恢复。

### 10.10 外部中断 (PA2 INT) 初始化模板

```c
/*-------------------------------------------------
 * 函数名: INT_INITIAL
 * 功能：  PA2外部中断初始化
 --------------------------------------------------*/
void INT_INITIAL(void)
{
    TRISA2 = 1;                     // 设置PA2为输入
    INTEDG = 1;                     // 1=上升沿触发 0=下降沿触发
    INTF = 0;                       // 清中断标志位
    INTE = 1;                       // 使能PA2外部中断
}
```

### 10.11 PA电平变化中断初始化模板

```c
/*-------------------------------------------------
 * 函数名: PA2_Level_Change_INITIAL
 * 功能：  PA端口(PA2)电平变化中断初始化
 --------------------------------------------------*/
void PA2_Level_Change_INITIAL(void)
{
    TRISA2 = 1;                     // 设置PA2输入
    ReadAPin = PORTA;               // 读PORTA清PA电平变化中断标志
    PAIF = 0;                       // 清PA INT中断标志位
    IOCA2 = 1;                      // 使能PA2电平变化中断
    PAIE = 1;                       // 使能PA INT中断
}
```

> ⚠️ **重要**：必须先读 `PORTA` 再清 `PAIF`，否则标志位无法清除。

### 10.12 LVD 低电压检测初始化模板

```c
/*-------------------------------------------------
 * 函数名：POWER_INITIAL (含LVD)
 * 功能：  上电系统初始化（含LVD配置）
 --------------------------------------------------*/
void POWER_INITIAL(void)
{
    // ... 常规初始化

    PCON = (0 << 4) | (1 << 3);
    // Bit[7:4]: 低电压检测阈值 0=1.8V
    // Bit[3]:   LVD使能位 1=使能 0=关闭
}

// 主循环中使用
void main()
{
    POWER_INITIAL();
    while(1)
    {
        if(PCON & 0X04)             // 检测LVDW标志位
        {
            // 低电压处理
        }
        else
        {
            // 正常电压处理
        }
    }
}
```

### 10.13 睡眠/唤醒模板

```c
/*-------------------------------------------------
 * 函数名：main (睡眠示例)
 * 功能：  主函数，执行任务后进入睡眠
 --------------------------------------------------*/
void main(void)
{
    POWER_INITIAL();                // 系统初始化

    // 执行上电任务...
    led1 = 1;
    led2 = 1;
    DelayS(4);
    led1 = 0;
    led2 = 0;

    // 进入睡眠等待唤醒
    while(1)
    {
        CLRWDT();                   // 清看门狗（可选）
        NOP();
        SLEEP();                    // 进入睡眠
        NOP();                      // 唤醒后第一条指令
    }
}
```

> ⚠️ 睡眠前如果使能了 WDT，需在睡眠指令前 `CLRWDT()`。唤醒源：PA2 外部中断、PA 电平变化中断、WDT 超时复位等。

---

## 11. 代码格式规范

### 11.1 缩进

**【必须】** 使用 **Tab** 键进行缩进（与官方示例一致）：

```c
void main()
{
    POWER_INITIAL();                // 1个Tab缩进
    while(1)
    {
        if(DemoPortIn == 1)         // 2个Tab缩进
        {
            DemoPortOut = 0;        // 3个Tab缩进
        }
    }
}
```

### 11.2 大括号风格

**【推荐】** 左大括号 `{` 另起一行：

```c
void Function(void)
{
    if(condition)
    {
        // ...
    }
    else
    {
        // ...
    }

    while(condition)
    {
        // ...
    }
}
```

### 11.3 空格规范

**【推荐】** 运算符两侧加空格：

```c
// 正确
a = 0;
for(a = 0; a < Time; a++)
if(input & 0x01)
data |= 0x80;

// 不推荐
a=0;
for(a=0;a<Time;a++)
if(input&0x01)
data|=0x80;
```

> **注意**：官方示例中部分代码运算符紧凑书写，建议新项目统一加空格提高可读性。

### 11.4 逗号后空格

**【推荐】** 参数列表逗号后加空格：

```c
void SPI_Write(uint addr, uchar dat)        // 正确
void SPI_Write(uint addr,uchar dat)          // 不推荐
```

### 11.5 行宽

**【推荐】** 单行代码长度不超过 **100** 字符。超长行应在合适位置换行，换行后缩进对齐：

```c
PSRCA = 0;
// 00:   4mA
// 01/10:8mA
// 11:   28mA
// Bit[3:2]: 控制PA5源电流
// Bit[1:0]: 控制PA4源电流
```

---

## 12. 最佳实践与禁忌

### 12.1 中断安全

**【必须】** EEPROM 写入操作必须关闭全局中断：

```c
void EEPROMwrite(unchar EEAddr, unchar Data)
{
    GIE = 0;                    // 关中断
    while(GIE);                 // 等待确认
    // ... 写操作 ...
    while(WR);                  // 等待写入完成
    GIE = 1;                    // 恢复中断
}
```

**【必须】** 中断服务函数中共享的全局变量使用 `volatile` 声明：

```c
volatile unsigned int temp;             // 示例：test_ft60e21x_MSCK.C
volatile unsigned char IRSendStatus;    // 中断与主循环共享
volatile unsigned char ReceiveFinish;   // 中断与主循环共享
```

### 12.2 中断标志清除顺序

**【必须】** PA 电平变化中断必须先读 `PORTA` 再清 `PAIF`：

```c
// 正确顺序
ReadAPin = PORTA;           // 1. 先读PORTA
PAIF = 0;                   // 2. 再清标志位

// 错误：顺序颠倒会导致标志位无法清除
```

### 12.3 看门狗处理

**【推荐】**
- 在主循环中适当位置清看门狗 `CLRWDT()`
- 睡眠前清看门狗
- 避免在中断中清看门狗（可能掩盖主循环死锁问题）

### 12.4 变量初始化

**【必须】** 全局变量必须显式初始化：

```c
unsigned char   RXFLAG = 0;               // 正确
unsigned char   IRbitNum = 0;             // 正确
unsigned int    SYSTime5S = 0;            // 正确

unsigned char   RXFLAG;                   // 不推荐：未初始化
```

### 12.5 延时精度

**【推荐】** 软件延时标注实际精度偏差：

```c
/*-------------------------------------------------
 * 函数名：DelayUs
 * 功能：  短延时函数 --16M-4T--大概快1%左右.
 * 输入：  Time 延时时间长度 延时时长Time*2 Us
 -------------------------------------------------*/
```

### 12.6 禁止事项

**【禁止】**
- ❌ 在中断服务函数中使用软件延时（`DelayMs`/`DelayUs`）
- ❌ 在中断服务函数中执行耗时操作（如 EEPROM 写入）
- ❌ `#include` 忘记加分号 `;`
- ❌ 写 EEPROM 时未关闭全局中断
- ❌ 位变量用于数组/结构体/指针
- ❌ 使用标准 C 的 `0b` 前缀（FMD 编译器使用 `0B`）
- ❌ 源文件使用小写 `.c` 后缀

### 12.7 低功耗设计建议

**【推荐】**
- 空闲时使用 `SLEEP()` 进入睡眠模式
- 睡眠前关闭不必要的外设时钟
- 使用外部中断或电平变化中断唤醒
- 睡眠前根据需要配置 LVD

### 12.8 代码可移植性

**【推荐】**
- 使用宏定义封装 IO 引脚：

```c
#define     LED             PA4
#define     IIC_SCL         PA4
#define     IIC_SDA         PA2
```

- 使用宏定义封装 IO 方向切换：

```c
#define     SDA_OUT         TRISA2 = 0
#define     SDA_IN          TRISA2 = 1
```

---

## 附录 A：完整项目模板

```c
// Project: Template.prj
// Device:  FT60E21X
// Memory:  PROM=1KX14, SRAM=64B, EEPROM=128
// Description: 项目功能描述
//
// RELEASE HISTORY
// VERSION     DATE         DESCRIPTION
// 1.0        YY-MM-DD        初始版本
//*****************************************************
#include    "SYSCFG.h";
#include    "FT60F21X.h";

//***********************宏定义****************************
#define     uchar           unsigned char
#define     uint            unsigned int

#define     LED             PA4
#define     KEY             PA2

//***********************全局变量**************************
volatile unsigned char   g_SysTick = 0;

/*-------------------------------------------------
 * 函数名：POWER_INITIAL
 * 功能：  上电系统初始化
 * 输入：  无
 * 输出：  无
 --------------------------------------------------*/
void POWER_INITIAL(void)
{
    OSCCON = 0B01110000;        // 16MHz/4T=4MHz
    INTCON = 0;                 // 暂禁止所有中断

    PORTA = 0B00000000;
    TRISA = 0B00000100;         // PA2输入，其余输出
    WPUA = 0B00000100;          // PA2上拉

    PSRCA = 0;
    PSINKA = 0;

    OPTION = 0B00001000;        // WDT MODE
    MSCON = 0B00000000;
}

/*-------------------------------------------------
 * 函数名：interrupt ISR
 * 功能：  中断处理
 * 输入：  无
 * 输出：  无
 --------------------------------------------------*/
void interrupt ISR(void)
{
    // 在此处理各中断源
}

/*-------------------------------------------------
 * 函数名：main
 * 功能：  主函数
 * 输入：  无
 * 输出：  无
 --------------------------------------------------*/
void main()
{
    POWER_INITIAL();            // 系统初始化

    while(1)
    {
        CLRWDT();               // 清看门狗

        // 用户代码...
    }
}
```

## 附录 B：FMD 常用 SFR 速查表

| 寄存器 | 功能 | 位宽 |
|--------|------|------|
| `OSCCON` | 系统时钟控制 | 8 bit |
| `INTCON` | 中断控制寄存器 | 8 bit |
| `OPTION` | 选项寄存器（T0/WDT预分频） | 8 bit |
| `PORTA` | PA口输出数据寄存器 | 8 bit |
| `TRISA` | PA口方向控制寄存器 | 8 bit |
| `WPUA` | PA口上拉控制寄存器 | 8 bit |
| `PSRCA` | PA口源电流控制 | 8 bit |
| `PSINKA` | PA口灌电流控制 | 8 bit |
| `MSCON` | 杂项控制寄存器 | 8 bit |
| `PCON` | 电源/LVD控制寄存器 | 8 bit |
| `TMR0` | Timer0 计数寄存器 | 8 bit |
| `T0CON0` | Timer0 控制寄存器0 | 8 bit |
| `TMR2H/L` | Timer2 计数寄存器 | 16 bit |
| `PR2H/L` | Timer2 周期寄存器 | 16 bit |
| `T2CON0/1` | Timer2 控制寄存器 | 8 bit |
| `WDTCON` | 看门狗控制寄存器 | 8 bit |
| `EEADR` | EEPROM 地址寄存器 | 8 bit |
| `EEDAT` | EEPROM 数据寄存器 | 8 bit |
| `EECON1` | EEPROM 控制寄存器 | 8 bit |
| `P1ADTH/L` | PWM1 占空比寄存器 | 16 bit |
| `P1OE` | PWM1 输出使能 | 8 bit |
| `P1POL` | PWM1 极性控制 | 8 bit |
| `P1CON` | PWM1 死区/重启控制 | 8 bit |
| `SOSCPRH/L` | 慢时钟测量结果 | 16 bit |

---

> **文档结束**  
> 本规范基于 FMD FT60E21x-C语言_V1.1 官方示例代码编写，版本 V1.0，2024-06-24。
