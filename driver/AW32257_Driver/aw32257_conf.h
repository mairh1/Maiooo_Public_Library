/**
 * @file    aw32257_conf.h
 * @brief   AW32257 驱动配置头（当前版本无功能裁剪项）
 * @details 通用驱动五件套固定文件之一。AW32257 核心的功能集是固定
 *          的：POR 安全初始化、类型化配置与快照、软件复位缺一不可，
 *          没有适合编译期裁剪的独立子功能，因此本文件暂不定义任何
 *          配置宏，仅作为统一的配置入口保留。数据手册协议常量
 *          AW32257_SOFT_RESET_DELAY_MS 归属 aw32257_regs.h，不放在
 *          这里。
 * @note    未来若引入可选功能（如写后回读校验、线程安全互斥），
 *          对应的配置宏应集中定义在本文件，采用
 *          "#ifndef 默认值 + #endif" 写法，支持编译器 -D 命令行
 *          覆盖；禁止把配置散落到核心 .c 中。
 * @author  Maiooo
 * @version 1.0.0
 * @date    2026-09-10
 */

#ifndef AW32257_CONF_H
#define AW32257_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* 当前版本无功能裁剪项：本文件为五件套固定文件与未来配置入口。 */

#ifdef __cplusplus
}
#endif

#endif /* AW32257_CONF_H */
