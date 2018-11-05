/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __ASM_EXPORT_H
#define __ASM_EXPORT_H
#include <asm-generic/export.h>

#ifdef CONFIG_KASAN
#define EXPORT_SYMBOL_NOKASAN(name)
#else
#define EXPORT_SYMBOL_NOKASAN(name)	EXPORT_SYMBOL(name)
#endif

#endif /* __ASM_EXPORT_H */
