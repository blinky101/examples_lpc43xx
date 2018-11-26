/***********************************************************************
 * $Id:: mw_usbd_rom_api.c 2219 2015-12-08 22:18:38Z usb00423                  $
 *
 * Project: USB device ROM Stack
 *
 * Description:
 *     ROM API Module.
 *
 ***********************************************************************
 *   Copyright(C) 2011, NXP Semiconductor
 *   All rights reserved.
 *
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * products. This software is supplied "AS IS" without any warranties.
 * NXP Semiconductors assumes no responsibility or liability for the
 * use of the software, conveys no license or title under any patent,
 * copyright, or mask work right to the product. NXP Semiconductors
 * reserves the right to make changes in the software without
 * notification. NXP Semiconductors also make no representation or
 * warranty that such application will be suitable for the specified
 * use without further testing or modification.
 **********************************************************************/

#include "mw_usbd_rom_api.h"

#if defined (__ICCARM__)
#pragma section = "usbd_hw_api_table"
#elif defined ( __GNUC__ )
__attribute__((section(".nsec.USBD_HW_API_TABLE")))
#elif defined ( __CC_ARM )
#pragma arm section rodata = "usbd_hw_api_table"
#endif
const  USBD_HW_API_T hw_api = {
	/* Main Driver functions */
	hwUSB_GetMemSize,
	hwUSB_Init,
	hwUSB_Connect,
	hwUSB_ISR,
	/* Other USB Hardware Util Functions */
	hwUSB_Reset,
	hwUSB_ForceFullSpeed,
	hwUSB_WakeUpCfg,
	hwUSB_SetAddress,
	hwUSB_Configure,
	hwUSB_ConfigEP,
	hwUSB_DirCtrlEP,
	hwUSB_EnableEP,
	hwUSB_DisableEP,
	hwUSB_ResetEP,
	hwUSB_SetStallEP,
	hwUSB_ClrStallEP,
	hwUSB_SetTestMode,
	hwUSB_ReadEP,
	hwUSB_ReadReqEP,
	hwUSB_ReadSetupPkt,
	hwUSB_WriteEP,
	hwUSB_WakeUp,
	hwUSB_EnableEvent,
};
#if defined ( __CC_ARM )
#pragma arm section /*"usbd_hw_api_table"*/
#endif

/* midleware API */
#if defined (__ICCARM__)
#pragma section = "usbd_core_api_table"
#elif defined ( __GNUC__ )
__attribute__((section(".nsec.USBD_CORE_API_TABLE")))
#elif defined ( __CC_ARM )
#pragma arm section rodata = "usbd_core_api_table"
#endif
const  USBD_CORE_API_T core_api = {
	mwUSB_RegisterClassHandler,
	mwUSB_RegisterEpHandler,
	mwUSB_SetupStage,
	mwUSB_DataInStage,
	mwUSB_DataOutStage,
	mwUSB_StatusInStage,
	mwUSB_StatusOutStage,
	mwUSB_StallEp0,
	mwUSB_GetEpHandler,
};
#if defined ( __CC_ARM )
#pragma arm section /*"usbd_core_api_table"*/
#endif

/*----------------------------------------------------------------------------
 * Mass Storage API structures and function prototypes
 *----------------------------------------------------------------------------*/
#if defined (__ICCARM__)
#pragma section = "usbd_msc_api_table"
#elif defined ( __GNUC__ )
__attribute__((section(".nsec.USBD_MSC_API_TABLE")))
#elif defined ( __CC_ARM )
#pragma arm section rodata = "usbd_msc_api_table"
#endif
const  USBD_MSC_API_T msc_api = {
	mwMSC_GetMemSize,
	mwMSC_init,
};
#if defined ( __CC_ARM )
#pragma arm section /*"usbd_msc_api_table"*/
#endif

/*----------------------------------------------------------------------------
 * Device Firmware Upgrade (DFU) API structures and function prototypes
 *----------------------------------------------------------------------------*/
#if defined (__ICCARM__)
#pragma section = "usbd_dfu_api_table"
#elif defined ( __GNUC__ )
__attribute__((section(".nsec.USBD_DFU_API_TABLE")))
#elif defined ( __CC_ARM )
#pragma arm section rodata = "usbd_dfu_api_table"
#endif
const  USBD_DFU_API_T dfu_api = {
	mwDFU_GetMemSize,
	mwDFU_init,
};
#if defined ( __CC_ARM )
#pragma arm section /*"usbd_dfu_api_table"*/
#endif

/*----------------------------------------------------------------------------
 * Human Interface Device (HID) API structures and function prototypes
 *----------------------------------------------------------------------------*/
#if defined (__ICCARM__)
#pragma section = "usbd_hid_api_table"
#elif defined ( __GNUC__ )
__attribute__((section(".nsec.USBD_HID_API_TABLE")))
#elif defined ( __CC_ARM )
#pragma arm section rodata = "usbd_hid_api_table"
#endif
const  USBD_HID_API_T hid_api = {
	mwHID_GetMemSize,
	mwHID_init,
};
#if defined ( __CC_ARM )
#pragma arm section /*"usbd_hid_api_table"*/
#endif

/*----------------------------------------------------------------------------
 * Communication Device class (CDC) API structures and function prototypes
 *----------------------------------------------------------------------------*/
#if defined (__ICCARM__)
#pragma section = "usbd_cdc_api_table"
#elif defined ( __GNUC__ )
__attribute__((section(".nsec.USBD_CDC_API_TABLE")))
#elif defined ( __CC_ARM )
#pragma arm section rodata = "usbd_cdc_api_table"
#endif
const  USBD_CDC_API_T cdc_api = {
	mwCDC_GetMemSize,
	mwCDC_init,
	mwCDC_SendNotification,
};
#if defined ( __CC_ARM )
#pragma arm section /*"usbd_cdc_api_table"*/
#endif

/*----------------------------------------------------------------------------
 * Main USBD API structure
 *----------------------------------------------------------------------------*/
#if defined (__ICCARM__)
#pragma section = "usbd_api_table"
#elif defined ( __GNUC__ )
__attribute__((section(".nsec.USBD_API_TABLE")))
#elif defined ( __CC_ARM )
#pragma arm section rodata = "usbd_api_table"
#endif
const  USBD_API_T usb_api = {
	&hw_api,
	&core_api,
	&msc_api,
	&dfu_api,
	&hid_api,
	&cdc_api,
	0,
	0x02233405,	/* Version identifier of USB ROM stack. The version is
				           defined as 0x0CHDMhCC where each nibble represnts version
				           number of the corresponding component.
				           CC -  7:0  - 8bit core version number
				            h - 11:8  - 4bit hardware interface version number
				            M - 15:12 - 4bit MSC class module version number
				            D - 19:16 - 4bit DFU class module version number
				            H - 23:20 - 4bit HID class module version number
				            C - 27:24 - 4bit CDC class module version number
				            H - 31:28
				 */
};
#if defined ( __CC_ARM )
#pragma arm section /*"usbd_api_table"*/
#endif
