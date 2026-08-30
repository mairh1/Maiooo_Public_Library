# uart_ring — 串口收发环形缓冲中间件

与硬件无关的串口数据收发处理层：RX / TX 双环形缓冲 + 中断协作函数。你的工程只需要在外设初始化代码里配置 UART 寄存器、在中断服务程序里转调一行，即可得到"中断收、主循环读；主循环写、中断发"的异步收发能力。

- 版本：1.0.0（2026-08-30）
- 语言：纯 C99，零平台依赖（无 OS API、无动态内存、无厂商库符号）
- 作者：Maiooo

## 文件组成

| 文件 | 职责 |
|---|---|
| `uart_ring.h` | 公共 API：类型、结果码、全部函数声明（文件头含完整用法示例） |
| `uart_ring.c` | 核心实现：环形缓冲算法、RX / TX 路径、自动泵状态机 |
| `uart_ring_conf.h` | 编译期功能裁剪开关（`#ifndef` 默认值，`-D` 可覆盖） |

## 设计要点

### 双环形缓冲 + SPSC 无锁模型

```
        接收中断 (生产者)                主循环 (消费者)
  UART->DATAR ──► uart_ring_rx_isr_byte ──► rx 环 ──► uart_ring_read
                                                满则丢弃 + rx_overflow++

        主循环 (生产者)                发送中断 (消费者)
  uart_ring_write ──► tx 环 ──► uart_ring_tx_isr ──► hw.send_byte → UART
                        ▲ 环由空转非空时自动开 TXE 中断，吐空自动关断
```

- head / tail 是**自由递增**的 `uint16_t` 计数器（自然回绕），数据量 = `(uint16_t)(head - tail)`，满 / 空无歧义，**容量全部可用**（不浪费一格）
- 每个计数器只有一方修改（生产者写 head、消费者写 tail），另一方只读——这正是"一个 ISR + 一个主循环"的串口典型拓扑，因此**无需关中断即并发安全**
- 代价：仅保证单生产者 / 单消费者。两个任务同时 `uart_ring_write` 需要调用者自行互斥

### 2 的幂容量

容量必须为 2 的幂（`uart_ring_init` 运行时校验），下标运算退化为 `& (cap-1)` 掩码，除法被编译器消掉。上限由 `UART_RING_MAX_CAPACITY`（默认 4096）约束，防止头文件 `uint16_t` 计数器比较的取模歧义并控制 RAM 占用。

### TX 自动泵（`UART_RING_USE_TX_IRQ=1`）

| 事件 | 动作 |
|---|---|
| `uart_ring_write` 时 TX 环**原为空** | 置 `tx_on`，回调 `tx_irq_enable(true)` 踢发 |
| `uart_ring_tx_isr` 弹出字节 | 回调 `send_byte()` 直写数据寄存器（契约：调用时 TXE 已置位） |
| `uart_ring_tx_isr` 后 TX 环**吐空** | 清 `tx_on`，回调 `tx_irq_enable(false)` 关断，杜绝 TXE 空转 |

"写空踢发"与"读空关断"两条互补路径闭合竞态窗口，前提是 **`uart_ring_write` 运行在线程上下文**（不会被本模块的 TX ISR 抢占其"判空→使能"序列）。若你的工程要在 ISR 里调用 `uart_ring_write`，请在调用前后屏蔽该外设的 TX 中断，或改用 `UART_RING_USE_TX_IRQ=0` 手动模式。

不需要泵的用户（例如 DMA 发送、或本来就在中断里轮询取字节）依然只用 `uart_ring_pop_tx_byte()`：该函数不受开关影响，是纯环操作。

### 结果码

| 值 | 语义 |
|---|---|
| `UART_RING_OK` | 成功（批量 read / write 含"部分成功"，实际字节数经 `*out` 返回） |
| `UART_RING_ERR_NULL_PTR` | 空指针 |
| `UART_RING_ERR_PARAM` | 容量非 2 的幂 / 超上限 / `len == 0` |
| `UART_RING_ERR_NOT_READY` | TX 路径未配置缓冲，或未注册 hw 回调 |
| `UART_RING_ERR_EMPTY` | 单字节读取时环内无数据 |

## 快速上手

```c
#include "uart_ring.h"

/* 1. 静态资源（容量为 2 的幂） */
static uint8_t    s_rx_mem[256];
static uint8_t    s_tx_mem[256];
static uart_ring_t s_uart1;

/* 2. 硬件回调：send_byte 直写数据寄存器；tx_irq_enable 开关 TXE 中断 */
static void uart1_send_byte(void *ctx, uint8_t byte)
{
    (void)ctx;
    USART1->DATAR = byte;
}

static void uart1_tx_irq_en(void *ctx, bool en)
{
    (void)ctx;
    if (en) { USART1->CTLR1 |= USART_CTLR1_TXEIE; }
    else    { USART1->CTLR1 &= (uint16_t)~USART_CTLR1_TXEIE; }
}

static const uart_ring_hw_t s_uart1_hw = {
    .send_byte     = uart1_send_byte,
    .tx_irq_enable = uart1_tx_irq_en,
};

/* 3. 初始化（外设本身仍由你的 BSP 配置好并使能 RXNE 中断） */
uart_ring_init(&s_uart1, s_rx_mem, 256, s_tx_mem, 256, &s_uart1_hw, NULL);

/* 4. 中断服务程序：一行转调 */
void USART1_IRQHandler(void)
{
    if (USART1->STATR & USART_STATR_RXNE)
    {
        uart_ring_rx_isr_byte(&s_uart1, (uint8_t)USART1->DATAR);
    }
    if ((USART1->STATR & USART_STATR_TXE) &&
        (USART1->CTLR1 & USART_CTLR1_TXEIE))
    {
        uart_ring_tx_isr(&s_uart1);
    }
}

/* 5. 应用侧：非阻塞消费 / 投递 */
uint8_t  buf[32];
uint16_t got;

uart_ring_read(&s_uart1, buf, sizeof(buf), &got);        /* got 可为 0 */
uart_ring_write(&s_uart1, (const uint8_t *)"AT\r\n", 4, &got);
```

STM32 HAL 工程的等价写法：`send_byte` 用 `HAL_UART_Transmit_IT(huart, &byte, 1)` 或直接 `huart->Instance->TDR = byte`，`tx_irq_enable` 操作 `CR1.TXEIE` 位，ISR 转调位置相同。

## 配置裁剪矩阵

| 宏 | 默认 | 置 0 效果 |
|---|---|---|
| `UART_RING_USE_TX_IRQ` | 1 | 剔除 `uart_ring_hw_t`、`uart_ring_tx_isr()`、`tx_on/hw/user_ctx` 字段；发送退化为"write 入环 + 用户在 TXE 中断自行 `uart_ring_pop_tx_byte()`" |
| `UART_RING_USE_STATS` | 1 | 剔除 `rx_overflow` 字段与 `uart_ring_get_rx_overflow()`；环满仍丢弃，仅不再计数 |
| `UART_RING_USE_RB_API` | 1 | 剔除 `uart_rb_*` 公共函数与 `uart_rb_init/reset` 等工具面（环算法仍在内部使用） |
| `UART_RING_MAX_CAPACITY` | 4096 | 单环容量上限，`init` 运行时校验；调小可强制约束 RAM |

三种配置途径优先级：编译器 `-D` > 工程私有 conf（include 路径靠前）> 库默认值。

## API 一览

| 分组 | 函数 | 上下文 |
|---|---|---|
| 生命周期 | `uart_ring_init` / `uart_ring_reset` | 线程（静默期） |
| 接收-生产 | `uart_ring_rx_isr_byte` | **ISR** |
| 接收-消费 | `uart_ring_read` / `uart_ring_read_byte` / `uart_ring_rx_available` | 线程（非阻塞） |
| 发送-生产 | `uart_ring_write` / `uart_ring_tx_free` | 线程（单生产者） |
| 发送-消费 | `uart_ring_tx_isr`（泵模式）/ `uart_ring_pop_tx_byte`（手动模式） | **ISR** |
| 状态 | `uart_ring_tx_busy` | 任意 |
| 统计 | `uart_ring_get_rx_overflow` | 任意（读取+清零非原子，尽力值） |
| 裸环工具 | `uart_rb_init / push / pop / write / read / count / space / reset` | 按 SPSC 规则任意 |

物理单位约定：所有长度 / 容量参数均为**字节**；本模块不含任何时间语义（无超时、无延时参数）。

## 设计约束与注意事项（已知坑）

1. **环空 ≠ 线路空闲**。TX 环取完最后一个字节后，它仍可能停留在数据寄存器 / 移位寄存器里。需要"这一帧真正发完"时刻（如 RS-485 方向翻转）请结合外设 TC（发送完成）中断自行判定，`uart_ring_tx_busy()` 只反映缓冲层视角。
2. **RX 溢出策略是"丢新保旧"**：环满时丢弃当前到达字节并计数（若开启统计），不会覆盖已入队数据。通信质量监控请周期读取 `uart_ring_get_rx_overflow()`。
3. **`uart_ring_write` 的踢发假设调用者不被本模块 TX ISR 抢占**（见"TX 自动泵"末段）。违反该假设的最坏后果是延迟到下一次写才被踢发，不会丢数据。
4. **多实例天然安全，单实例多生产者需互斥**。两个任务共享一个串口实例时，请用 mutex 或把发送收敛到单一任务。
5. **init / reset 不与 ISR 并发**。顺序固定：关外设中断 → init/reset → 开外设中断。
6. **计数器回绕**：head / tail 为 `uint16_t` 自由递增，容量 ≤ 32768 时差值运算 `(uint16_t)(a - b)` 恒正确，无需处理回绕。
7. `uart_ring_pop_tx_byte()` 在泵模式下也可用作"诊断查询"，但**不要在两处同时消费同一个 TX 环**（泵与手动取字节二选一）。

## 许可证

WTFPL，见仓库根目录 LICENSE。
