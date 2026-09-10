/**
 * @file    sgm41513_conf.h
 * @brief   SGM41513 驱动的编译期配置
 * @details 本文件集中存放驱动的全部编译期配置：每项都可以直接在此
 *          修改，或用编译器命令行覆盖（如 -DSGM41513_THREAD_SAFE=1），
 *          因为每个默认值都包在 #ifndef 里。
 * @note    配置优先级从高到低：编译器 -D 覆盖 > 工程私有 conf
 *          （include 路径靠前）> 本文件默认值兜底。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-10
 */

#ifndef SGM41513_CONF_H
#define SGM41513_CONF_H

/* ═════════════════════════════════════════════════════════════════════════════
 * SGM41513_VARIANT - 板上器件是哪个硅片变体
 * ═════════════════════════════════════════════════════════════════════════════ */
/* 型号字段（REG0B PN[3:0]）只能区分 PN=0000（SGM41513）与 PN=0001
 * （SGM41513A 或 SGM41513D）。A 与 D 变体在 I2C 上电气特性一致，
 * 差别仅在 VBUS_STAT 状态编码。驱动在 init() 时自动识别 SGM41513 与
 * A/D，当 PN = 0001 时用下面的选项解码 VBUS_STAT。
 *
 *  SGM41513_VARIANT_SGM41513  (0)：基础型号，PSEL 引脚输入检测
 *  SGM41513_VARIANT_SGM41513A (1)：D+/D- BC1.2 检测
 *  SGM41513_VARIANT_SGM41513D (2)：D+/D- BC1.2 检测（状态集完整）
 */

#define SGM41513_VARIANT_SGM41513   0   /**< 基础型号：PSEL 引脚输入检测 */
#define SGM41513_VARIANT_SGM41513A  1   /**< A 变体：D+/D- BC1.2 检测 */
#define SGM41513_VARIANT_SGM41513D  2   /**< D 变体：D+/D- BC1.2 检测
                                             （状态集完整） */

#ifndef SGM41513_VARIANT
#define SGM41513_VARIANT    SGM41513_VARIANT_SGM41513D  /**< 本板器件变体，
                                                             默认按 D 解码 */
#endif

/* ═════════════════════════════════════════════════════════════════════════════
 * 功能开关（置 0 后对应公共 API 声明与实现随 #if 一同从编译中移除，
 * 调用被裁剪 API 在编译期报错）
 * ═════════════════════════════════════════════════════════════════════════════ */

/* JEITA 温度窗口配置（REG0C 及相关位）                                    */
#ifndef SGM41513_USE_JEITA
#define SGM41513_USE_JEITA  1   /**< 1：提供 sgm41513_jeita_configure() */
#endif

/* OTG 反向升压(boost)模式（REG01 OTG_CONFIG、REG06 BOOSTV、REG02 LIM）   */
#ifndef SGM41513_USE_OTG
#define SGM41513_USE_OTG    1   /**< 1：提供 OTG 相关 API */
#endif

/* 面向可调适配器的 PUMPX 电流脉冲协议（REG0D）                           */
#ifndef SGM41513_USE_PUMPX
#define SGM41513_USE_PUMPX  1   /**< 1：提供 PUMPX 相关 API */
#endif

/* 船运模式 / BATFET 控制（REG07）                                        */
#ifndef SGM41513_USE_SHIP
#define SGM41513_USE_SHIP   1   /**< 1：提供船运模式相关 API */
#endif

/* ═════════════════════════════════════════════════════════════════════════════
 * SGM41513_REG_SHADOW - 寄存器影子缓存(shadow)
 * ═════════════════════════════════════════════════════════════════════════════ */
/* 1：驱动保存每个可读可写寄存器的副本，并提供
 *    sgm41513_restore_settings() 一次调用重放全部设置。
 *    看门狗超时（芯片将可读可写寄存器复位为 POR 值）后、或退出船运
 *    模式（芯片强制 EN_HIZ = 1）后使用。
 * 0：每实例省 16 字节，不支持重放。                                       */

#ifndef SGM41513_REG_SHADOW
#define SGM41513_REG_SHADOW 1   /**< 1：启用影子缓存与 restore_settings() */
#endif

/* ═════════════════════════════════════════════════════════════════════════════
 * SGM41513_VERIFY_WRITES - 写入回读校验
 * ═════════════════════════════════════════════════════════════════════════════ */
/* 1：每次写可读可写寄存器后回读比较（自清零位除外），不一致返回
 *    SGM41513_ERR_VERIFY。每次写入多一次 I2C 读——量产默认关闭，
 *    怀疑总线完整性问题时再开。                                            */

#ifndef SGM41513_VERIFY_WRITES
#define SGM41513_VERIFY_WRITES  0   /**< 1：写后回读比较 */
#endif

/* ═════════════════════════════════════════════════════════════════════════════
 * SGM41513_THREAD_SAFE - 并发保护钩子
 * ═════════════════════════════════════════════════════════════════════════════ */
/* 1：驱动在每个多次访问的 API 调用前后加 sgm41513_io_lock() /
 *    sgm41513_io_unlock()，这两个函数由移植者实现（如互斥锁
 *    take/give）。单线程系统保持 0 即零开销。                             */

#ifndef SGM41513_THREAD_SAFE
#define SGM41513_THREAD_SAFE   0    /**< 1：要求移植层实现 lock/unlock */
#endif

#endif /* SGM41513_CONF_H */
