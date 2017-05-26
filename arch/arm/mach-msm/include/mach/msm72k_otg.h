/* Copyright (c) 2009-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_USB_GADGET_MSM72K_OTG_H__
#define __LINUX_USB_GADGET_MSM72K_OTG_H__

#include <linux/usb.h>
#include <linux/usb/gadget.h>
#include <linux/usb/otg.h>
#include <linux/wakelock.h>
#include <linux/usb/msm_hsusb.h>

#include <asm/mach-types.h>

#define OTGSC_BSVIE            (1 << 27)
#define OTGSC_IDIE             (1 << 24)
#define OTGSC_IDPU             (1 << 5)
#define OTGSC_BSVIS            (1 << 19)
#define OTGSC_ID               (1 << 8)
#define OTGSC_IDIS             (1 << 16)
#define OTGSC_BSV              (1 << 11)
#define OTGSC_DPIE             (1 << 30)
#define OTGSC_DPIS             (1 << 22)
#define OTGSC_HADP             (1 << 6)
#define OTGSC_IDPU             (1 << 5)

#define ULPI_STP_CTRL   (1 << 30)
#define ASYNC_INTR_CTRL (1 << 29)
#define ULPI_SYNC_STATE (1 << 27)

#define PORTSC_PHCD     (1 << 23)
#define PORTSC_CSC	(1 << 1)
#define disable_phy_clk() (writel(readl(USB_PORTSC) | PORTSC_PHCD, USB_PORTSC))
#define enable_phy_clk() (writel(readl(USB_PORTSC) & ~PORTSC_PHCD, USB_PORTSC))
#define is_phy_clk_disabled() (readl(USB_PORTSC) & PORTSC_PHCD)
#define is_phy_active()       (readl_relaxed(USB_ULPI_VIEWPORT) &\
						ULPI_SYNC_STATE)
#define is_usb_active()       (!(readl(USB_PORTSC) & PORTSC_SUSP))

#define USB_IDCHG_MIN	500
#define USB_IDCHG_MAX	1500
#define USB_IB_UNCFG	2
#define OTG_ID_POLL_MS	1000

#endif
