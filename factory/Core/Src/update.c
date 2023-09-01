#include "update.h"

#include <stdio.h>

#include "common.h"
#include "main.h"

typedef void (*pFunction)(void);
__IO uint32_t MspAddress;
__IO uint32_t JumpAddress;
pFunction JumpToApplication;

typedef enum {
  IAP_START,
  IAP_TRANS,
} IAP_STATUS;

typedef struct {
  __IO uint8_t enabled;
  uint8_t status;
  uint32_t addr;
  uint32_t filesize;
  uint32_t wr_cnt;
  uint32_t crc;
} IAP_UP;

static IAP_UP iap_up = {
    .enabled = 1,
    .status = IAP_START,
};

const uint32_t boot_param_size32 = sizeof(BOOT_PARAM) / 4 + (sizeof(BOOT_PARAM) % 4 ? 1 : 0);
const uint32_t boot_param_size64 = sizeof(BOOT_PARAM) / 8 + (sizeof(BOOT_PARAM) % 8 ? 1 : 0);
const uint32_t boot_param_crcdatalen = boot_param_size32 - 1;
const BOOT_PARAM boot_param_default = {
    .app_boot = BOOT_FACTORY,
    .app_status = STATUS_ERROR,
    .padding = {0, 0},
    .crc_val = 0xc704dd7b,
};

/* 常用CRC32校验算法 */
static uint32_t uiReflect(uint32_t uiData, uint8_t ucLength) {
  uint32_t uiMask = 1 << (ucLength - 1), uiMaskRef = 1, uiDataReturn = 0;

  for (; uiMask; uiMask >>= 1) {
    if (uiData & uiMask)
      uiDataReturn |= uiMaskRef;

    uiMaskRef <<= 1;
  }

  return uiDataReturn;
}

uint32_t uiCRC32(uint32_t *puiInitCRC, uint8_t *pucDataBuff, uint32_t uiLength) {
  uint32_t uiPolynomial = 0x04C11DB7, uiInputCRC = 0xFFFFFFFF, i = 0;
  uint8_t ucMask = 0;

  if (puiInitCRC != NULL)
    uiInputCRC = *puiInitCRC;

  uiPolynomial = uiReflect(uiPolynomial, 32);

  for (i = 0; i < uiLength; ++i) {
    uiInputCRC ^= *pucDataBuff++;

    for (ucMask = 1; ucMask; ucMask <<= 1) {
      if (uiInputCRC & 1)
        uiInputCRC = (uiInputCRC >> 1) ^ uiPolynomial;
      else
        uiInputCRC >>= 1;
    }
  }

  return ~uiInputCRC;
}
/* CRC32-MPEG2.0校验算法 */
uint32_t uiCRC32_MPEG2(uint32_t *puiInitCRC, uint8_t *pucDataBuff, uint32_t uiLength) {
  uint32_t uiPolynomial = 0x04C11DB7, uiInputCRC = 0xFFFFFFFF, i = 0;
  uint8_t ucMask = 0;

  if (puiInitCRC != NULL)
    uiInputCRC = *puiInitCRC;

  for (i = 0; i < uiLength; ++i) {
    uiInputCRC ^= (uint32_t)(*pucDataBuff++) << 24;

    for (ucMask = 1; ucMask; ucMask <<= 1) {
      if (uiInputCRC & 0x80000000)
        uiInputCRC = (uiInputCRC << 1) ^ uiPolynomial;
      else
        uiInputCRC <<= 1;
    }
  }

  return uiInputCRC;
}

static uint32_t param_crc_calc(const BOOT_PARAM *param) {
  uint32_t crc = 0;

  crc = uiCRC32_MPEG2(NULL, (uint8_t *)param, boot_param_crcdatalen * 4);

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
    LL_mDelay(100);
    NVIC_SystemReset();
  }
  __set_CONTROL(0);
  __set_MSP(MspAddress);
  JumpToApplication();
}

void boot_param_check_upgrade(void) {
  BOOT_PARAM param;

  disable_global_irq();

  STMFLASH_Read(ADDR_BASE_PARAM, (uint64_t *)&param, boot_param_size64);

  if (SCB->VTOR == ADDR_BASE_FACTORY) {
    uart_printf("currently guided to the `factory` partition\n");
  } else {
    uart_printf("currently guided to the `app` partition\n");
    if (param.app_status == STATUS_VERIFY) {
      param.app_boot = BOOT_APP;
      param.app_status = STATUS_NORMAL;
      boot_param_update(ADDR_BASE_PARAM, &param);
      boot_param_update(ADDR_BASE_PARAM_BAK, &param);
      uart_printf("device reboot\n");
      LL_mDelay(100);
      NVIC_SystemReset();
    }
  }

  enable_global_irq();
}

void iap_update(frame_parse_t *frame) {
  static BOOT_PARAM param;

  if (!iap_up.enabled) {
    uart_printf("cannot upgrade for now!\n");
    return;
  }

  switch (iap_up.status) {
    case IAP_START: {
      if (frame->id == FRAME_TYPE_BEGIN) {
        disable_global_irq();
        STMFLASH_Read(ADDR_BASE_PARAM, (uint64_t *)&param, boot_param_size64);
        enable_global_irq();
        if (frame->length != 4) {
          break;
        }
        iap_up.filesize = *(uint32_t *)frame->data;
        if (frame->byte_order) {
          change_byte_order((uint8_t *)&iap_up.filesize, sizeof(iap_up.filesize));
        }
        if (iap_up.filesize % 8) {
          break;
        }
        iap_up.crc = 0xFFFFFFFF;
        iap_up.addr = ADDR_BASE_APP;
        iap_up.wr_cnt = 0;
        iap_up.status = IAP_TRANS;
      }
    } break;
    case IAP_TRANS: {
      if (frame->id == FRAME_TYPE_DATA) {
        if (frame->length % 8) {
          uart_printf("iap data length error!\n");
          iap_up.status = IAP_START;
        }
        if (iap_up.wr_cnt >= PART_SIZE_APP) {
          uart_printf("iap data length is too large!\n");
          iap_up.status = IAP_START;
        }
        disable_global_irq();
        STMFLASH_Write(iap_up.addr + iap_up.wr_cnt, (uint64_t *)(frame->data), frame->length / 8);
        iap_up.crc = uiCRC32_MPEG2(&iap_up.crc, frame->data, frame->length);
        enable_global_irq();
        iap_up.wr_cnt += frame->length;
        uart_printf("trans ok\n");
      } else if (frame->id == FRAME_TYPE_END) {
        if (iap_up.wr_cnt != iap_up.filesize) {
          iap_up.status = IAP_START;
          uart_printf("upgrade failed\n");
          break;
        }
        if ((frame->length != 4) || (iap_up.crc != *(uint32_t *)frame->data)) {
          iap_up.status = IAP_START;
          uart_printf("upgrade failed\n");
          break;
        }
        param.app_boot = BOOT_FACTORY;
        param.app_status = STATUS_VERIFY;
        disable_global_irq();
        boot_param_update(ADDR_BASE_PARAM, &param);
        boot_param_update(ADDR_BASE_PARAM_BAK, &param);
        enable_global_irq();
        iap_up.enabled = 0;
        iap_up.status = IAP_START;
        uart_printf("upgrade completed\n");
        LL_mDelay(100);
        NVIC_SystemReset();
      } else {
        ;
      }
    } break;
  }
}

void reboot_for_update(void) {
  BOOT_PARAM param = {
      .app_boot = BOOT_APP,
      .app_status = STATUS_UPDATED,
  };

  disable_global_irq();
  boot_param_update(ADDR_BASE_PARAM, &param);
  boot_param_update(ADDR_BASE_PARAM_BAK, &param);
  enable_global_irq();

  LL_mDelay(100);
  NVIC_SystemReset();
}

void back_to_app(void) {
  BOOT_PARAM param;

  if (iap_up.wr_cnt) {
    return;
  }

  disable_global_irq();
  STMFLASH_Read(ADDR_BASE_PARAM, (uint64_t *)&param, boot_param_size64);
  enable_global_irq();

  if (param.app_boot != BOOT_APP || param.app_status != STATUS_UPDATED) {
    return;
  }

  param.app_boot = BOOT_APP;
  param.app_status = STATUS_NORMAL;

  disable_global_irq();
  boot_param_update(ADDR_BASE_PARAM, &param);
  boot_param_update(ADDR_BASE_PARAM_BAK, &param);
  enable_global_irq();

  LL_mDelay(100);
  NVIC_SystemReset();
}

void boot_test(void) {
  return;
}
