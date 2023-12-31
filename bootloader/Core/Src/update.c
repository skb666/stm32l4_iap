#include "update.h"

#include <stdio.h>

#include "crc.h"
#include "main.h"

typedef void (*pFunction)(void);
__IO uint32_t MspAddress;
__IO uint32_t JumpAddress;
pFunction JumpToApplication;

const uint32_t boot_param_size32 = sizeof(BOOT_PARAM) / 4 + (sizeof(BOOT_PARAM) % 4 ? 1 : 0);
const uint32_t boot_param_size64 = sizeof(BOOT_PARAM) / 8 + (sizeof(BOOT_PARAM) % 8 ? 1 : 0);
const uint32_t boot_param_crcdatalen = 4 * (boot_param_size32 - 1);
const BOOT_PARAM boot_param_default = {
    .app_boot = BOOT_FACTORY,
    .app_status = STATUS_ERROR,
    .padding = {0, 0},
    .crc_val = 0xc704dd7b,
};

static uint32_t param_crc_calc(const BOOT_PARAM *param) {
  uint32_t crc = 0;

  crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)param, boot_param_crcdatalen);

  return crc;
}

static void boot_param_update(uint32_t addr, BOOT_PARAM *param) {
  param->crc_val = param_crc_calc(param);
  STMFLASH_Write(addr, (uint64_t *)param, boot_param_size64);
}

void boot_param_read_check(BOOT_PARAM *pdata) {
  BOOT_PARAM param, param_bak;

  STMFLASH_Read(ADDR_BASE_PARAM, (uint64_t *)&param, boot_param_size64);
  STMFLASH_Read(ADDR_BASE_PARAM_BAK, (uint64_t *)&param_bak, boot_param_size64);

  if (param_crc_calc(&param) == param.crc_val) {
    uart_printf("boot param checked Ok\n");
    if (param_crc_calc(&param_bak) == param_bak.crc_val) {
      uart_printf("boot param backup checked Ok\n");
      if (memcmp(&param, &param_bak, sizeof(BOOT_PARAM)) != 0) {
        uart_printf("boot param main sector and backup sector data are different, update bakup sector data\n");
        STMFLASH_Write(ADDR_BASE_PARAM_BAK, (uint64_t *)&param, boot_param_size64);
      } else {
        uart_printf("boot param main sector and backup sector data are the same\n");
      }
    } else {
      uart_printf("boot param backup checked Fail, update backup sector data\n");
      STMFLASH_Write(ADDR_BASE_PARAM_BAK, (uint64_t *)&param, boot_param_size64);
    }
    memcpy(pdata, &param, sizeof(BOOT_PARAM));
  } else {
    uart_printf("boot param checked Fail\n");
    if (param_crc_calc(&param_bak) == param_bak.crc_val) {
      uart_printf("boot param backup checked Ok\n");
      uart_printf("update main sector data\n");
      STMFLASH_Write(ADDR_BASE_PARAM, (uint64_t *)&param_bak, boot_param_size64);
      memcpy(pdata, &param_bak, sizeof(BOOT_PARAM));
    } else {
      uart_printf("boot param backup checked Fail\n");
      uart_printf("restore defaults\n");
      STMFLASH_Write(ADDR_BASE_PARAM, (uint64_t *)&boot_param_default, boot_param_size64);
      STMFLASH_Write(ADDR_BASE_PARAM_BAK, (uint64_t *)&boot_param_default, boot_param_size64);
      memcpy(pdata, &boot_param_default, sizeof(BOOT_PARAM));
    }
  }
}

uint32_t select_boot_addr(BOOT_PARAM *param) {
  uint32_t boot_addr = ADDR_BASE_FACTORY;

  if (param->app_boot == BOOT_FACTORY) {
    switch (param->app_status) {
      case STATUS_ERROR: {
        boot_addr = ADDR_BASE_FACTORY;
      } break;
      case STATUS_VERIFY: {
        boot_addr = ADDR_BASE_APP;
        param->app_boot = BOOT_APP;
        param->app_status = STATUS_VERIFY;
        boot_param_update(ADDR_BASE_PARAM, param);
        boot_param_update(ADDR_BASE_PARAM_BAK, param);
      } break;
      case STATUS_NORMAL:
      case STATUS_UPDATED:
      default: {
        param->app_boot = BOOT_FACTORY;
        param->app_status = STATUS_ERROR;
        boot_param_update(ADDR_BASE_PARAM, param);
        boot_param_update(ADDR_BASE_PARAM_BAK, param);
      } break;
    }
  } else if (param->app_boot == BOOT_APP) {
    switch (param->app_status) {
      case STATUS_NORMAL: {
        boot_addr = ADDR_BASE_APP;
      } break;
      case STATUS_UPDATED: {
        boot_addr = ADDR_BASE_FACTORY;
      } break;
      case STATUS_ERROR:
      case STATUS_VERIFY:
      default: {
        param->app_boot = BOOT_FACTORY;
        param->app_status = STATUS_ERROR;
        boot_param_update(ADDR_BASE_PARAM, param);
        boot_param_update(ADDR_BASE_PARAM_BAK, param);
      } break;
    }
  } else {
    param->app_boot = BOOT_FACTORY;
    param->app_status = STATUS_ERROR;
    boot_param_update(ADDR_BASE_PARAM, param);
    boot_param_update(ADDR_BASE_PARAM_BAK, param);
  }

  return boot_addr;
}

inline __attribute__((always_inline)) void start_boot_app(uint32_t boot_addr) {
  MspAddress = *(__IO uint32_t *)(boot_addr);
  JumpAddress = *(__IO uint32_t *)(boot_addr + 4);
  JumpToApplication = (pFunction)JumpAddress;
  if ((MspAddress & 0xFFF00000) != 0x10000000 && (MspAddress & 0xFFF00000) != 0x20000000) {
    NVIC_SystemReset();
  }
  __set_CONTROL(0);
  __set_MSP(MspAddress);
  JumpToApplication();
}

void boot_test(void) {
  uint32_t crc = 0;

  crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)&boot_param_default, boot_param_crcdatalen);
  printf("0x%08lx\n", crc);

  return;
}