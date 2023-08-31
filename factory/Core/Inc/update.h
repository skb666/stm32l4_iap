#ifndef __UPDATE_H__
#define __UPDATE_H__

#include <stdint.h>

#include "device.h"
#include "onchip_flash.h"

#define PART_SIZE_BOOTLOADER (0x5000UL)  // 20KB
#define PART_SIZE_FACTORY (0x8000UL)     // 32KB
#define PART_SIZE_CUSTOM (0x2000UL)      // 8KB
#define PART_SIZE_PARAM (0x800UL)        // 2KB
#define PART_SIZE_PARAM_BAK (0x800UL)    // 2KB
#define PART_SIZE_APP (0x30000UL)        // 192KB

#define ADDR_BASE_BOOTLOADER (STMFLASH_BASE)
#define ADDR_BASE_FACTORY ((ADDR_BASE_BOOTLOADER) + (PART_SIZE_BOOTLOADER))
#define ADDR_BASE_CUSTOM ((ADDR_BASE_FACTORY) + (PART_SIZE_FACTORY))
#define ADDR_BASE_PARAM ((ADDR_BASE_CUSTOM) + (PART_SIZE_CUSTOM))
#define ADDR_BASE_PARAM_BAK ((ADDR_BASE_PARAM) + (PART_SIZE_PARAM))
#define ADDR_BASE_APP ((ADDR_BASE_PARAM_BAK) + (PART_SIZE_PARAM_BAK))

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  BOOT_FACTORY = 0,
  BOOT_APP,
} APP_BOOT;

typedef enum {
  STATUS_ERROR = 0,  // APP错误，不能正常工作
  STATUS_UPDATED,    // APP更新状态
  STATUS_VERIFY,     // APP等待验证
  STATUS_NORMAL,     // APP能正常稳定运行
} APP_STATUS;

typedef struct {
  uint8_t app_boot;    // 指示待引导的分区
  uint8_t app_status;  // APP 的状态
  uint8_t padding[2];  // 结构体填充
  uint32_t crc_val;    // 引导参数的 crc 校验值
} BOOT_PARAM;

void boot_param_read_check(BOOT_PARAM *pdata);
uint32_t select_boot_addr(BOOT_PARAM *param);
void start_boot_app(uint32_t boot_addr);

void boot_param_check_upgrade(void);
void iap_update(frame_parse_t *frame);
void reboot_for_update(void);
void back_to_app(void);

void boot_test(void);

#ifdef __cplusplus
}
#endif

#endif
