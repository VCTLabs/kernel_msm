/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <soc/qcom/scm.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"

#define MSM_VDEC_DVC_NAME "msm_vdec_8974"
#define MIN_NUM_OUTPUT_BUFFERS 4
#define MAX_NUM_OUTPUT_BUFFERS VB2_MAX_FRAME
#define DEFAULT_VIDEO_CONCEAL_COLOR_BLACK 0x8010
#define MB_SIZE_IN_PIXEL (16 * 16)

#define TZ_DYNAMIC_BUFFER_FEATURE_ID 12
#define TZ_FEATURE_VERSION(major, minor, patch) \
	(((major & 0x3FF) << 22) | ((minor & 0x3FF) << 12) | (patch & 0xFFF))

static const char *const mpeg_video_vidc_divx_format[] = {
	"DIVX Format 3",
	"DIVX Format 4",
	"DIVX Format 5",
	"DIVX Format 6",
	NULL
};
static const char *mpeg_video_stream_format[] = {
	"NAL Format Start Codes",
	"NAL Format One NAL Per Buffer",
	"NAL Format One Byte Length",
	"NAL Format Two Byte Length",
	"NAL Format Four Byte Length",
	NULL
};
static const char *const mpeg_video_output_order[] = {
	"Display Order",
	"Decode Order",
	NULL
};
static const char *const mpeg_video_vidc_extradata[] = {
	"Extradata none",
	"Extradata MB Quantization",
	"Extradata Interlace Video",
	"Extradata VC1 Framedisp",
	"Extradata VC1 Seqdisp",
	"Extradata timestamp",
	"Extradata S3D Frame Packing",
	"Extradata Frame Rate",
	"Extradata Panscan Window",
	"Extradata Recovery point SEI",
	"Extradata Closed Caption UD",
	"Extradata AFD UD",
	"Extradata Multislice info",
	"Extradata number of concealed MB",
	"Extradata metadata filler",
	"Extradata input crop",
	"Extradata digital zoom",
	"Extradata aspect ratio",
	"Extradata mpeg2 seqdisp",
};
static const char *const mpeg_vidc_video_alloc_mode_type[] = {
	"Buffer Allocation Static",
	"Buffer Allocation Ring Buffer",
	"Buffer Allocation Dynamic Buffer"
};

static const char *const perf_level[] = {
	"Nominal",
	"Performance",
	"Turbo"
};

static const char *const h263_level[] = {
	"1.0",
	"2.0",
	"3.0",
	"4.0",
	"4.5",
	"5.0",
	"6.0",
	"7.0",
};

static const char *const h263_profile[] = {
	"Baseline",
	"H320 Coding",
	"Backward Compatible",
	"ISWV2",
	"ISWV3",
	"High Compression",
	"Internet",
	"Interlace",
	"High Latency",
};

static const char *const vp8_profile_level[] = {
	"Unused",
	"0.0",
	"1.0",
	"2.0",
	"3.0",
};

static const char *const mpeg_vidc_video_h264_mvc_layout[] = {
	"Frame packing arrangement sequential",
	"Frame packing arrangement top-bottom",
};

static struct msm_vidc_ctrl msm_vdec_ctrls[] = {
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT,
		.name = "NAL Format",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES,
		.maximum = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_FOUR_BYTE_LENGTH,
		.default_value = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_ONE_NAL_PER_BUFFER) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_ONE_BYTE_LENGTH) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_TWO_BYTE_LENGTH) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_FOUR_BYTE_LENGTH)
		),
		.qmenu = mpeg_video_stream_format,
		.step = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER,
		.name = "Output Order",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY,
		.maximum = V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DECODE,
		.default_value = V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY) |
			(1 << V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DECODE)
			),
		.qmenu = mpeg_video_output_order,
		.step = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE,
		.name = "Picture Type Decoding",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 15,
		.default_value = 15,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_KEEP_ASPECT_RATIO,
		.name = "Keep Aspect Ratio",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_POST_LOOP_DEBLOCKER_MODE,
		.name = "Deblocker Mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_DIVX_FORMAT,
		.name = "Divx Format",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_4,
		.maximum = V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_6,
		.default_value = V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_4,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_4) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_5) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_6)
			),
		.qmenu = mpeg_video_vidc_divx_format,
		.step = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MB_ERROR_MAP_REPORTING,
		.name = "MB Error Map Reporting",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_CONTINUE_DATA_TRANSFER,
		.name = "control",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE,
		.name = "Sync Frame Decode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_ENABLE,
		.default_value = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SECURE,
		.name = "Secure mode",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.minimum = 0,
		.maximum = 0,
		.default_value = 0,
		.step = 0,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA,
		.name = "Extradata Type",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_EXTRADATA_NONE,
		.maximum = V4L2_MPEG_VIDC_EXTRADATA_FRAME_BITS_INFO,
		.default_value = V4L2_MPEG_VIDC_EXTRADATA_NONE,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_EXTRADATA_NONE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VC1_FRAMEDISP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VC1_SEQDISP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MULTISLICE_INFO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_METADATA_FILLER) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_INPUT_CROP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_DIGITAL_ZOOM) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_STREAM_USERDATA) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_FRAME_QP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_FRAME_BITS_INFO)
			),
		.qmenu = mpeg_video_vidc_extradata,
		.step = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_SET_PERF_LEVEL,
		.name = "Decoder Performance Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_PERF_LEVEL_NOMINAL,
		.maximum = V4L2_CID_MPEG_VIDC_PERF_LEVEL_TURBO,
		.default_value = V4L2_CID_MPEG_VIDC_PERF_LEVEL_NOMINAL,
		.menu_skip_mask = ~(
			(1 << V4L2_CID_MPEG_VIDC_PERF_LEVEL_NOMINAL) |
			(1 << V4L2_CID_MPEG_VIDC_PERF_LEVEL_TURBO)),
		.qmenu = perf_level,
		.step = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE_INPUT,
		.name = "Buffer allocation mode for input",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_STATIC,
		.maximum = V4L2_MPEG_VIDC_VIDEO_DYNAMIC,
		.default_value = V4L2_MPEG_VIDC_VIDEO_STATIC,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_STATIC) |
			(1 << V4L2_MPEG_VIDC_VIDEO_RING) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DYNAMIC)
			),
		.qmenu = mpeg_vidc_video_alloc_mode_type,
		.step = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE_OUTPUT,
		.name = "Buffer allocation mode for output",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_STATIC,
		.maximum = V4L2_MPEG_VIDC_VIDEO_DYNAMIC,
		.default_value = V4L2_MPEG_VIDC_VIDEO_STATIC,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_STATIC) |
			(1 << V4L2_MPEG_VIDC_VIDEO_RING) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DYNAMIC)
			),
		.qmenu = mpeg_vidc_video_alloc_mode_type,
		.step = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FRAME_ASSEMBLY,
		.name = "Video frame assembly",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_FRAME_ASSEMBLY_DISABLE,
		.maximum = V4L2_MPEG_VIDC_FRAME_ASSEMBLY_ENABLE,
		.default_value =  V4L2_MPEG_VIDC_FRAME_ASSEMBLY_DISABLE,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE,
		.name = "Video decoder multi stream",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum =
			V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY,
		.maximum =
			V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_SECONDARY,
		.default_value =
			V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY,
		.menu_skip_mask = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
		.name = "MPEG4 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
		.maximum =
		V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_CODING_EFFICIENCY,
		.default_value = V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
		.name = "MPEG4 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_MPEG4_LEVEL_0,
		.maximum = V4L2_MPEG_VIDEO_MPEG4_LEVEL_5,
		.default_value = V4L2_MPEG_VIDEO_MPEG4_LEVEL_0,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.name = "H264 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.maximum = V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH,
		.default_value = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.name = "H264 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_5_2,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H263_PROFILE,
		.name = "H263 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BASELINE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_HIGHLATENCY,
		.default_value = V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BASELINE,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BASELINE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_H320CODING) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BACKWARDCOMPATIBLE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_ISWV2) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_ISWV3) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_HIGHCOMPRESSION) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_INTERNET) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_INTERLACE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_HIGHLATENCY)
		),
		.qmenu = h263_profile,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H263_LEVEL,
		.name = "H263 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_7_0,
		.default_value = V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_1_0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_1_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_2_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_3_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_4_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_5_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_6_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_7_0)
		),
		.qmenu = h263_level,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
		.name = "VP8 Profile Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_1,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED) |
			(1 << V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0) |
			(1 << V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_1)
		),
		.qmenu = vp8_profile_level,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE,
		.name = "MPEG2 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_HIGH,
		.default_value = V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL,
		.name = "MPEG2 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0,
		.maximum = V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_3,
		.default_value = V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SCS_THRESHOLD,
		.name = "Video start code search threshold",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = INT_MAX,
		.default_value = INT_MAX,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MVC_BUFFER_LAYOUT,
		.name = "MVC buffer layout",
		.type = V4L2_CTRL_TYPE_MENU,
		.maximum = V4L2_MPEG_VIDC_VIDEO_MVC_TOP_BOTTOM,
		.default_value = V4L2_MPEG_VIDC_VIDEO_MVC_SEQUENTIAL,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_MVC_SEQUENTIAL) |
			(1 << V4L2_MPEG_VIDC_VIDEO_MVC_TOP_BOTTOM)
			),
		.qmenu = mpeg_vidc_video_h264_mvc_layout,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR,
		.name = "Picture concealed color",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0x0,
		.maximum = 0xffffff,
		.default_value = DEFAULT_VIDEO_CONCEAL_COLOR_BLACK,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_BUFFER_SIZE_LIMIT,
		.name = "Buffer size limit",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = INT_MAX,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SECURE_SCALING_THRESHOLD,
		.name = "Secure scaling output2 threshold",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = INT_MAX,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_NON_SECURE_OUTPUT2,
		.name = "Non-Secure output2",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_vdec_ctrls)

static u32 get_frame_size_nv12(int plane,
					u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12, width, height);
}

static u32 get_frame_size_compressed(int plane,
					u32 max_mbs_per_frame, u32 size_per_mb)
{
	return (max_mbs_per_frame * size_per_mb * 3/2)/2;
}

static u32 get_frame_size(struct msm_vidc_inst *inst,
					const struct msm_vidc_format *fmt,
					int fmt_type, int plane)
{
	u32 frame_size = 0;
	if (fmt_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		frame_size = fmt->get_frame_size(plane,
					inst->capability.mbs_per_frame.max,
					MB_SIZE_IN_PIXEL);
		if (inst->capability.buffer_size_limit &&
			(inst->capability.buffer_size_limit < frame_size)) {
			frame_size = inst->capability.buffer_size_limit;
			dprintk(VIDC_DBG, "input buffer size limited to %d\n",
				frame_size);
		} else {
			dprintk(VIDC_DBG, "set input buffer size to %d\n",
				frame_size);
		}
	} else if (fmt_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		frame_size = fmt->get_frame_size(plane,
					inst->capability.height.max,
					inst->capability.width.max);
		dprintk(VIDC_DBG, "set output buffer size to %d\n",
			frame_size);
	} else {
		dprintk(VIDC_WARN, "Wrong format type\n");
	}
	return frame_size;
}

static int is_ctrl_valid_for_codec(struct msm_vidc_inst *inst,
					struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_MVC_BUFFER_LAYOUT:
		if (inst->fmts[OUTPUT_PORT]->fourcc != V4L2_PIX_FMT_H264_MVC) {
			dprintk(VIDC_ERR, "Control 0x%x only valid for MVC\n",
					ctrl->id);
			rc = -ENOTSUPP;
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		if (inst->fmts[OUTPUT_PORT]->fourcc == V4L2_PIX_FMT_H264_MVC &&
			ctrl->val != V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH) {
			dprintk(VIDC_ERR,
					"Profile 0x%x not supported for MVC\n",
					ctrl->val);
			rc = -ENOTSUPP;
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		if (inst->fmts[OUTPUT_PORT]->fourcc == V4L2_PIX_FMT_H264_MVC &&
			ctrl->val >= V4L2_MPEG_VIDEO_H264_LEVEL_5_2) {
			dprintk(VIDC_ERR, "Level 0x%x not supported for MVC\n",
					ctrl->val);
			rc = -ENOTSUPP;
			break;
		}
		break;
	default:
		break;
	}
	return rc;
}

struct msm_vidc_format vdec_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 2,
		.get_frame_size = get_frame_size_nv12,
		.type = CAPTURE_PORT,
	},
	{
		.name = "Mpeg4",
		.description = "Mpeg4 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "Mpeg2",
		.description = "Mpeg2 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H263",
		.description = "H263 compressed format",
		.fourcc = V4L2_PIX_FMT_H263,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VC1",
		.description = "VC-1 compressed format",
		.fourcc = V4L2_PIX_FMT_VC1_ANNEX_G,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VC1 SP",
		.description = "VC-1 compressed format G",
		.fourcc = V4L2_PIX_FMT_VC1_ANNEX_L,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H264_MVC",
		.description = "H264_MVC compressed format",
		.fourcc = V4L2_PIX_FMT_H264_MVC,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "HEVC_HYBRID",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC_HYBRID,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VP8",
		.description = "VP8 compressed format",
		.fourcc = V4L2_PIX_FMT_VP8,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "DIVX 311",
		.description = "DIVX 311 compressed format",
		.fourcc = V4L2_PIX_FMT_DIVX_311,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "DIVX",
		.description = "DIVX 4/5/6 compressed format",
		.fourcc = V4L2_PIX_FMT_DIVX,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	}
};

int msm_vdec_streamon(struct msm_vidc_inst *inst, enum v4l2_buf_type i)
{
	int rc = 0;
	struct buf_queue *q;
	q = msm_comm_get_vb2q(inst, i);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}
	dprintk(VIDC_DBG, "Calling streamon\n");
	mutex_lock(&q->lock);
	rc = vb2_streamon(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "streamon failed on port: %d\n", i);
	return rc;
}

int msm_vdec_streamoff(struct msm_vidc_inst *inst, enum v4l2_buf_type i)
{
	int rc = 0;
	struct buf_queue *q;

	q = msm_comm_get_vb2q(inst, i);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}
	dprintk(VIDC_DBG, "Calling streamoff\n");
	mutex_lock(&q->lock);
	rc = vb2_streamoff(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "streamoff failed on port: %d\n", i);
	return rc;
}

int msm_vdec_prepare_buf(struct msm_vidc_inst *inst,
					struct v4l2_buffer *b)
{
	int rc = 0;
	struct vidc_buffer_addr_info buffer_info;
	int extra_idx = 0;
	int i;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->state == MSM_VIDC_CORE_INVALID ||
			inst->core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
			"Core %pK in bad state, ignoring prepare buf\n",
				inst->core);
		goto exit;
	}

	switch (b->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			if (b->length != inst->fmts[CAPTURE_PORT]->num_planes) {
				dprintk(VIDC_ERR,
				"Planes mismatch: needed: %d, allocated: %d\n",
				inst->fmts[CAPTURE_PORT]->num_planes,
				b->length);
				rc = -EINVAL;
				break;
			}
			for (i = 0; (i < b->length)
				&& (i < VIDEO_MAX_PLANES); ++i) {
				dprintk(VIDC_DBG,
				"prepare plane: %d, device_addr = 0x%lx, size = %d\n",
				i, b->m.planes[i].m.userptr,
				b->m.planes[i].length);
			}
			buffer_info.buffer_size = b->m.planes[0].length;
			buffer_info.buffer_type =
				msm_comm_get_hal_output_buffer(inst);
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr =
				b->m.planes[0].m.userptr;
			extra_idx = EXTRADATA_IDX(b->length);
			if (extra_idx && (extra_idx < VIDEO_MAX_PLANES) &&
				b->m.planes[extra_idx].m.userptr) {
				buffer_info.extradata_addr =
					b->m.planes[extra_idx].m.userptr;
				buffer_info.extradata_size =
					b->m.planes[extra_idx].length;
				dprintk(VIDC_DBG,
					"extradata: 0x%pa, length = %d\n",
					&buffer_info.extradata_addr,
					buffer_info.extradata_size);
			} else {
				buffer_info.extradata_addr = 0;
				buffer_info.extradata_size = 0;
			}
			rc = call_hfi_op(hdev, session_set_buffers,
					(void *)inst->session, &buffer_info);
			if (rc)
				dprintk(VIDC_ERR,
				"vidc_hal_session_set_buffers failed\n");
		break;
	default:
		dprintk(VIDC_ERR, "Buffer type not recognized: %d\n", b->type);
		break;
	}
exit:
	return rc;
}

int msm_vdec_release_buf(struct msm_vidc_inst *inst,
					struct v4l2_buffer *b)
{
	int rc = 0;
	struct vidc_buffer_addr_info buffer_info;
	struct msm_vidc_core *core;
	int extra_idx = 0;
	int i;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	hdev = inst->core->device;

	if (inst->state == MSM_VIDC_CORE_INVALID ||
			core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
			"Core %pK in bad state, ignoring release output buf\n",
				core);
		goto exit;
	}

	switch (b->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			if (b->length !=
				inst->fmts[CAPTURE_PORT]->num_planes) {
				dprintk(VIDC_ERR,
				"Planes mismatch: needed: %d, to release: %d\n",
				inst->fmts[CAPTURE_PORT]->num_planes,
				b->length);
				rc = -EINVAL;
				break;
			}
			for (i = 0; i < b->length; ++i) {
				dprintk(VIDC_DBG,
				"Release plane: %d device_addr = 0x%lx, size = %d\n",
				i, b->m.planes[i].m.userptr,
				b->m.planes[i].length);
			}
			buffer_info.buffer_size = b->m.planes[0].length;
			buffer_info.buffer_type =
				msm_comm_get_hal_output_buffer(inst);
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr =
				 b->m.planes[0].m.userptr;
			extra_idx = EXTRADATA_IDX(b->length);
			if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)
				&& b->m.planes[extra_idx].m.userptr)
				buffer_info.extradata_addr =
					b->m.planes[extra_idx].m.userptr;
			else
				buffer_info.extradata_addr = 0;
			buffer_info.response_required = false;
			rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
			if (rc)
				dprintk(VIDC_ERR,
				"vidc_hal_session_release_buffers failed\n");
		break;
	default:
		dprintk(VIDC_ERR, "Buffer type not recognized: %d\n", b->type);
		break;
	}
exit:
	return rc;
}

int msm_vdec_qbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct buf_queue *q = NULL;
	int rc = 0;
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR, "Failed to find buffer queue for type = %d\n"
			, b->type);
		return -EINVAL;
	}
	mutex_lock(&q->lock);
	rc = vb2_qbuf(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "Failed to qbuf, %d\n", rc);
	return rc;
}
int msm_vdec_dqbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct buf_queue *q = NULL;
	int rc = 0;
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR, "Failed to find buffer queue for type = %d\n"
			, b->type);
		return -EINVAL;
	}
	mutex_lock(&q->lock);
	rc = vb2_dqbuf(&q->vb2_bufq, b, true);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_DBG, "Failed to dqbuf, %d\n", rc);
	return rc;
}

int msm_vdec_reqbufs(struct msm_vidc_inst *inst, struct v4l2_requestbuffers *b)
{
	struct buf_queue *q = NULL;
	int rc = 0;
	if (!inst || !b) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, buffer = %pK\n", inst, b);
		return -EINVAL;
	}
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR, "Failed to find buffer queue for type = %d\n"
			, b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_reqbufs(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_DBG, "Failed to get reqbufs, %d\n", rc);
	return rc;
}

int msm_vdec_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	const struct msm_vidc_format *fmt = NULL;
	unsigned int *plane_sizes = NULL;
	struct hfi_device *hdev;
	int stride, scanlines;
	unsigned int extra_idx = 0;
	int rc = 0;
	int i;
	struct hal_buffer_requirements *bufreq;

	if (!inst || !f || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, format = %pK\n", inst, f);
		return -EINVAL;
	}

	rc = msm_comm_try_get_bufreqs(inst);
	if (rc) {
		dprintk(VIDC_ERR, "Getting buffer requirements failed: %d\n",
				rc);
		return rc;
	}

	hdev = inst->core->device;
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		fmt = inst->fmts[CAPTURE_PORT];
	else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		fmt = inst->fmts[OUTPUT_PORT];
	else
		return -ENOTSUPP;

	f->fmt.pix_mp.pixelformat = fmt->fourcc;
	f->fmt.pix_mp.num_planes = fmt->num_planes;
	if (inst->in_reconfig) {
		if (msm_comm_get_stream_output_mode(inst) ==
				HAL_VIDEO_DECODER_PRIMARY) {
			inst->prop.height[CAPTURE_PORT] = inst->reconfig_height;
			inst->prop.width[CAPTURE_PORT] = inst->reconfig_width;
			inst->prop.height[OUTPUT_PORT] = inst->reconfig_height;
			inst->prop.width[OUTPUT_PORT] = inst->reconfig_width;
		} else {
			inst->prop.height[OUTPUT_PORT] = inst->reconfig_height;
			inst->prop.width[OUTPUT_PORT] = inst->reconfig_width;
		}
		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
					"%s: unsupported session\n", __func__);
			goto exit;
		}
	}

	f->fmt.pix_mp.height = inst->prop.height[CAPTURE_PORT];
	f->fmt.pix_mp.width = inst->prop.width[CAPTURE_PORT];
	stride = inst->prop.width[CAPTURE_PORT];
	scanlines = inst->prop.height[CAPTURE_PORT];

	rc = msm_comm_try_get_bufreqs(inst);
	if (rc) {
		dprintk(VIDC_ERR,
				"%s: Failed : Buffer requirements\n", __func__);
		goto exit;
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		plane_sizes = &inst->bufq[OUTPUT_PORT].vb2_bufq.plane_sizes[0];
		for (i = 0; i < fmt->num_planes; ++i) {
			if (plane_sizes[i] == 0) {
				f->fmt.pix_mp.plane_fmt[i].sizeimage =
					get_frame_size(inst, fmt, f->type, i);
				plane_sizes[i] = f->fmt.pix_mp.plane_fmt[i].
					sizeimage;
			} else
				f->fmt.pix_mp.plane_fmt[i].sizeimage =
					plane_sizes[i];
		}
	} else {
		switch (fmt->fourcc) {
			case V4L2_PIX_FMT_NV12:
				call_hfi_op(hdev, get_stride_scanline,
						COLOR_FMT_NV12,
						inst->prop.width[CAPTURE_PORT],
						inst->prop.height[CAPTURE_PORT],
						&stride, &scanlines);
				break;
			default:
				dprintk(VIDC_WARN,
						"Color format not recognized\n");
				break;
		}

		bufreq = get_buff_req_buffer(inst,
				msm_comm_get_hal_output_buffer(inst));
		f->fmt.pix_mp.plane_fmt[0].sizeimage =
			bufreq ? bufreq->buffer_size : 0;

		extra_idx = EXTRADATA_IDX(fmt->num_planes);
		if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
			bufreq = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_OUTPUT);
			f->fmt.pix_mp.plane_fmt[extra_idx].sizeimage =
				bufreq ? bufreq->buffer_size : 0;
		}

		for (i = 0; i < fmt->num_planes; ++i)
			inst->bufq[CAPTURE_PORT].vb2_bufq.plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
	}

	if (stride && scanlines) {
		f->fmt.pix_mp.plane_fmt[0].bytesperline =
			(__u16)stride;
		f->fmt.pix_mp.plane_fmt[0].reserved[0] =
			(__u16)scanlines;
	} else {
		f->fmt.pix_mp.plane_fmt[0].bytesperline =
			(__u16)inst->prop.width[CAPTURE_PORT];
		f->fmt.pix_mp.plane_fmt[0].reserved[0] =
			(__u16)inst->prop.height[CAPTURE_PORT];
	}

	if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			f->fmt.pix_mp.height = inst->prop.height[CAPTURE_PORT];
			f->fmt.pix_mp.width = inst->prop.width[CAPTURE_PORT];
		} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			f->fmt.pix_mp.height = inst->prop.height[OUTPUT_PORT];
			f->fmt.pix_mp.width = inst->prop.width[OUTPUT_PORT];
			f->fmt.pix_mp.plane_fmt[0].bytesperline =
				(__u16)inst->prop.width[OUTPUT_PORT];
			f->fmt.pix_mp.plane_fmt[0].reserved[0] =
				(__u16)inst->prop.height[OUTPUT_PORT];
		}
	}
exit:
	return rc;
}
int msm_vdec_s_parm(struct msm_vidc_inst *inst, struct v4l2_streamparm *a)
{
	u64 us_per_frame = 0;
	int rc = 0, fps = 0;
	if (a->parm.output.timeperframe.denominator) {
		switch (a->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			us_per_frame = a->parm.output.timeperframe.numerator *
				(u64)USEC_PER_SEC;
			do_div(us_per_frame, a->parm.output.\
					timeperframe.denominator);
			break;
		default:
			dprintk(VIDC_ERR,
					"Scale clocks : Unknown buffer type %d\n",
					a->type);
			break;
		}
	}

	if (!us_per_frame) {
		dprintk(VIDC_ERR,
			"Failed to scale clocks : time between frames is 0\n");
		rc = -EINVAL;
		goto exit;
	}

	fps = USEC_PER_SEC;
	do_div(fps, us_per_frame);

	if ((fps % 15 == 14) || (fps % 24 == 23))
		fps = fps + 1;
	else if ((fps % 24 == 1) || (fps % 15 == 1))
		fps = fps - 1;

	if (inst->prop.fps != fps) {
		dprintk(VIDC_PROF, "reported fps changed for %pK: %d->%d\n",
				inst, inst->prop.fps, fps);
		inst->prop.fps = fps;
		msm_comm_init_dcvs_load(inst);
		msm_comm_scale_clocks_and_bus(inst);
	}
exit:
	return rc;
}

int msm_vdec_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	struct msm_vidc_format *fmt = NULL;
	struct hal_frame_size frame_sz;
	int extra_idx = 0;
	int rc = 0;
	int ret = 0;
	int i;
	struct hal_buffer_requirements *bufreq;
	int max_input_size = 0;

	if (!inst || !f) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_fourcc(vdec_formats,
			ARRAY_SIZE(vdec_formats), f->fmt.pix_mp.pixelformat,
			CAPTURE_PORT);
		if (!fmt || (fmt && fmt->type != CAPTURE_PORT)) {
			dprintk(VIDC_ERR,
				"Format: %d not supported on CAPTURE port\n",
				f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto err_invalid_fmt;
		}

		inst->prop.width[CAPTURE_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[CAPTURE_PORT] = f->fmt.pix_mp.height;
		if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_PRIMARY) {
			inst->prop.width[OUTPUT_PORT] = f->fmt.pix_mp.width;
			inst->prop.height[OUTPUT_PORT] = f->fmt.pix_mp.height;
		}

		inst->fmts[fmt->type] = fmt;
		if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
			frame_sz.buffer_type = HAL_BUFFER_OUTPUT2;
			frame_sz.width = inst->prop.width[CAPTURE_PORT];
			frame_sz.height = inst->prop.height[CAPTURE_PORT];
			dprintk(VIDC_DBG,
				"buffer type = %d width = %d, height = %d\n",
				frame_sz.buffer_type, frame_sz.width,
				frame_sz.height);
			ret = msm_comm_try_set_prop(inst,
				HAL_PARAM_FRAME_SIZE, &frame_sz);
		}

		ret = ret || msm_comm_try_get_bufreqs(inst);
		if (ret) {
			for (i = 0; i < fmt->num_planes; ++i) {
				f->fmt.pix_mp.plane_fmt[i].sizeimage =
					get_frame_size(inst, fmt, f->type, i);
			}
		} else {
			bufreq = get_buff_req_buffer(inst,
					msm_comm_get_hal_output_buffer(inst));
			f->fmt.pix_mp.plane_fmt[0].sizeimage =
				bufreq ? bufreq->buffer_size : 0;

			extra_idx = EXTRADATA_IDX(fmt->num_planes);
			if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
				bufreq = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_OUTPUT);
				f->fmt.pix_mp.plane_fmt[1].sizeimage =
					bufreq ? bufreq->buffer_size : 0;
			}
		}

		f->fmt.pix_mp.num_planes = fmt->num_planes;
		for (i = 0; i < fmt->num_planes; ++i) {
			inst->bufq[CAPTURE_PORT].vb2_bufq.plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		inst->prop.width[OUTPUT_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[OUTPUT_PORT] = f->fmt.pix_mp.height;
		if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_PRIMARY) {
			inst->prop.width[CAPTURE_PORT] = f->fmt.pix_mp.width;
			inst->prop.height[CAPTURE_PORT] = f->fmt.pix_mp.height;
		}

		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
			goto err_invalid_fmt;
		}

		fmt = msm_comm_get_pixel_fmt_fourcc(vdec_formats,
				ARRAY_SIZE(vdec_formats),
				f->fmt.pix_mp.pixelformat,
				OUTPUT_PORT);
		if (!fmt || fmt->type != OUTPUT_PORT) {
			dprintk(VIDC_ERR,
			"Format: %d not supported on OUTPUT port\n",
			f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto err_invalid_fmt;
		}

		rc = msm_comm_try_state(inst, MSM_VIDC_CORE_INIT_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to initialize instance\n");
			goto err_invalid_fmt;
		}

		if (!(get_hal_codec_type(fmt->fourcc) &
			inst->core->dec_codec_supported)) {
			dprintk(VIDC_ERR,
				"Codec(0x%x) is not present in the supported codecs list(0x%x)\n",
				get_hal_codec_type(fmt->fourcc),
				inst->core->dec_codec_supported);
			rc = -EINVAL;
			goto err_invalid_fmt;
		}

		inst->fmts[fmt->type] = fmt;
		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to open instance\n");
			goto err_invalid_fmt;
		}

		frame_sz.buffer_type = HAL_BUFFER_INPUT;
		frame_sz.width = inst->prop.width[OUTPUT_PORT];
		frame_sz.height = inst->prop.height[OUTPUT_PORT];
		dprintk(VIDC_DBG,
			"buffer type = %d width = %d, height = %d\n",
			frame_sz.buffer_type, frame_sz.width,
			frame_sz.height);
		msm_comm_try_set_prop(inst, HAL_PARAM_FRAME_SIZE, &frame_sz);

		max_input_size = get_frame_size(inst, fmt, f->type, 0);
		if (f->fmt.pix_mp.plane_fmt[0].sizeimage > max_input_size ||
			f->fmt.pix_mp.plane_fmt[0].sizeimage == 0) {
			f->fmt.pix_mp.plane_fmt[0].sizeimage = max_input_size;
		}

		f->fmt.pix_mp.num_planes = fmt->num_planes;
		for (i = 0; i < fmt->num_planes; ++i) {
			inst->bufq[OUTPUT_PORT].vb2_bufq.plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}
	}
err_invalid_fmt:
	return rc;
}

int msm_vdec_querycap(struct msm_vidc_inst *inst, struct v4l2_capability *cap)
{
	if (!inst || !cap) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, cap = %pK\n", inst, cap);
		return -EINVAL;
	}
	strlcpy(cap->driver, MSM_VIDC_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, MSM_VDEC_DVC_NAME, sizeof(cap->card));
	cap->bus_info[0] = 0;
	cap->version = MSM_VIDC_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
						V4L2_CAP_VIDEO_OUTPUT_MPLANE |
						V4L2_CAP_STREAMING;
	memset(cap->reserved, 0, sizeof(cap->reserved));
	return 0;
}

int msm_vdec_enum_fmt(struct msm_vidc_inst *inst, struct v4l2_fmtdesc *f)
{
	const struct msm_vidc_format *fmt = NULL;
	int rc = 0;
	if (!inst || !f) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, f = %pK\n", inst, f);
		return -EINVAL;
	}
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_index(vdec_formats,
			ARRAY_SIZE(vdec_formats), f->index, CAPTURE_PORT);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_index(vdec_formats,
			ARRAY_SIZE(vdec_formats), f->index, OUTPUT_PORT);
		f->flags = V4L2_FMT_FLAG_COMPRESSED;
	}

	memset(f->reserved, 0 , sizeof(f->reserved));
	if (fmt) {
		strlcpy(f->description, fmt->description,
				sizeof(f->description));
		f->pixelformat = fmt->fourcc;
	} else {
		dprintk(VIDC_DBG, "No more formats found\n");
		rc = -EINVAL;
	}
	return rc;
}

static int msm_vdec_queue_setup(struct vb2_queue *q,
				const struct v4l2_format *fmt,
				unsigned int *num_buffers,
				unsigned int *num_planes, unsigned int sizes[],
				void *alloc_ctxs[])
{
	int i, rc = 0;
	struct msm_vidc_inst *inst;
	struct hal_buffer_requirements *bufreq;
	int extra_idx = 0;
	struct hfi_device *hdev;
	struct hal_buffer_count_actual new_buf_count;
	enum hal_property property_id;
	if (!q || !num_buffers || !num_planes
		|| !sizes || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK, %pK, %pK\n",
			q, num_buffers, num_planes);
		return -EINVAL;
	}
	inst = q->drv_priv;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*num_planes = inst->fmts[OUTPUT_PORT]->num_planes;
		if (*num_buffers < MIN_NUM_OUTPUT_BUFFERS ||
				*num_buffers > MAX_NUM_OUTPUT_BUFFERS)
			*num_buffers = MIN_NUM_OUTPUT_BUFFERS;
		for (i = 0; i < *num_planes; i++) {
			sizes[i] = get_frame_size(inst,
					inst->fmts[OUTPUT_PORT], q->type, i);
		}
		property_id = HAL_PARAM_BUFFER_COUNT_ACTUAL;
		new_buf_count.buffer_type = HAL_BUFFER_INPUT;
		new_buf_count.buffer_count_actual = *num_buffers;
		rc = call_hfi_op(hdev, session_set_property,
				inst->session, property_id, &new_buf_count);
		if (rc) {
			dprintk(VIDC_WARN,
				"Failed to set new buffer count(%d) on FW, err: %d\n",
				new_buf_count.buffer_count_actual, rc);
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		dprintk(VIDC_DBG, "Getting bufreqs on capture plane\n");
		*num_planes = inst->fmts[CAPTURE_PORT]->num_planes;
		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to open instance\n");
			break;
		}
		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to get buffer requirements: %d\n", rc);
			break;
		}

		bufreq = get_buff_req_buffer(inst,
			msm_comm_get_hal_output_buffer(inst));
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"No buffer requirement for buffer type %x\n",
				HAL_BUFFER_OUTPUT);
			rc = -EINVAL;
			break;
		}
		*num_buffers = max(*num_buffers, bufreq->buffer_count_min);
		if (*num_buffers != bufreq->buffer_count_actual) {
			property_id = HAL_PARAM_BUFFER_COUNT_ACTUAL;
			new_buf_count.buffer_type =
				msm_comm_get_hal_output_buffer(inst);
			new_buf_count.buffer_count_actual = *num_buffers;
			rc = call_hfi_op(hdev, session_set_property,
				inst->session, property_id, &new_buf_count);
		}

		if (*num_buffers != bufreq->buffer_count_actual) {
			rc = msm_comm_try_get_bufreqs(inst);
			if (rc) {
				dprintk(VIDC_WARN,
					"Failed to get buf req, %d\n", rc);
				break;
			}
		}
		dprintk(VIDC_DBG, "count =  %d, size = %d, alignment = %d\n",
				inst->buff_req.buffer[1].buffer_count_actual,
				inst->buff_req.buffer[1].buffer_size,
				inst->buff_req.buffer[1].buffer_alignment);
		sizes[0] = bufreq->buffer_size;
		extra_idx =
			EXTRADATA_IDX(inst->fmts[CAPTURE_PORT]->num_planes);
		if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
			bufreq = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_OUTPUT);
			if (bufreq)
				sizes[extra_idx] = bufreq->buffer_size;
			else
				sizes[extra_idx] = 0;
		}
		break;
	default:
		dprintk(VIDC_ERR, "Invalid q type = %d\n", q->type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static inline int start_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct vb2_buf_entry *temp;
	struct hfi_device *hdev;
	struct list_head *ptr, *next;

	hdev = inst->core->device;
	inst->in_reconfig = false;
	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY)
		rc = msm_vidc_check_scaling_supported(inst);
	if (rc) {
		dprintk(VIDC_ERR, "H/w scaling is not in valid range\n");
		return -EINVAL;
	}
	rc = msm_comm_set_scratch_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to set scratch buffers: %d\n", rc);
		goto fail_start;
	}
	rc = msm_comm_set_persist_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to set persist buffers: %d\n", rc);
		goto fail_start;
	}

	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_comm_set_output_buffers(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to set output buffers: %d\n", rc);
			goto fail_start;
		}
	}

	msm_comm_scale_clocks_and_bus(inst);

	rc = msm_comm_try_state(inst, MSM_VIDC_START_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to start done state\n", inst);
		goto fail_start;
	}
	msm_comm_init_dcvs_load(inst);
	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_comm_queue_output_buffers(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to queue output buffers: %d\n", rc);
			goto fail_start;
		}
	}
	mutex_lock(&inst->pendingq.lock);
	list_for_each_safe(ptr, next, &inst->pendingq.list) {
		temp = list_entry(ptr, struct vb2_buf_entry, list);
		rc = msm_comm_qbuf(temp->vb);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to qbuf to hardware\n");
			break;
		}
		list_del(&temp->list);
		kfree(temp);
	}
	mutex_unlock(&inst->pendingq.lock);
	return rc;
fail_start:
	return rc;
}

static inline int stop_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;
	rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to start done state\n", inst);
	return rc;
}

static int msm_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	struct hfi_device *hdev;
	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	dprintk(VIDC_DBG,
		"Streamon called on: %d capability\n", q->type);
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (inst->bufq[CAPTURE_PORT].vb2_bufq.streaming)
			rc = start_streaming(inst);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (inst->bufq[OUTPUT_PORT].vb2_bufq.streaming)
			rc = start_streaming(inst);
		break;
	default:
		dprintk(VIDC_ERR, "Q-type is not supported: %d\n", q->type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int msm_vdec_stop_streaming(struct vb2_queue *q)
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	dprintk(VIDC_DBG, "Streamoff called on: %d capability\n", q->type);
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (!inst->bufq[CAPTURE_PORT].vb2_bufq.streaming)
			rc = stop_streaming(inst);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (!inst->bufq[OUTPUT_PORT].vb2_bufq.streaming)
			rc = stop_streaming(inst);
		break;
	default:
		dprintk(VIDC_ERR,
			"Q-type is not supported: %d\n", q->type);
		rc = -EINVAL;
		break;
	}

	msm_comm_scale_clocks_and_bus(inst);

	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK, cap = %d to state: %d\n",
			inst, q->type, MSM_VIDC_RELEASE_RESOURCES_DONE);
	return rc;
}

static void msm_vdec_buf_queue(struct vb2_buffer *vb)
{
	int rc;
	rc = msm_comm_qbuf(vb);
	if (rc)
		dprintk(VIDC_ERR, "Failed to queue buffer: %d\n", rc);
}

int msm_vdec_cmd(struct msm_vidc_inst *inst, struct v4l2_decoder_cmd *dec)
{
	int rc = 0;
	struct msm_vidc_core *core;

	if (!dec || !inst || !inst->core) {
		dprintk(VIDC_ERR, "%s invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	switch (dec->cmd) {
	case V4L2_DEC_QCOM_CMD_FLUSH:
		if (core->state != VIDC_CORE_INVALID &&
			inst->state ==  MSM_VIDC_CORE_INVALID) {
			rc = msm_comm_kill_session(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed to recover from session_error: %d\n",
					rc);
		}
		rc = msm_comm_flush(inst, dec->flags);
		if (rc) {
			dprintk(VIDC_ERR,
					"Failed to flush buffers: %d\n", rc);
		}
		break;
	case V4L2_DEC_CMD_STOP:
		if (core->state != VIDC_CORE_INVALID &&
			inst->state ==  MSM_VIDC_CORE_INVALID) {
			rc = msm_comm_kill_session(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed to recover from session_error: %d\n",
					rc);
		}
		rc = msm_comm_release_scratch_buffers(inst, false);
		if (rc)
			dprintk(VIDC_ERR,
				"Failed to release scratch buffers: %d\n", rc);
		rc = msm_comm_release_persist_buffers(inst);
		if (rc)
			pr_err("Failed to release persist buffers: %d\n", rc);
		if (inst->state == MSM_VIDC_CORE_INVALID ||
			core->state == VIDC_CORE_INVALID) {
			dprintk(VIDC_ERR,
				"Core %pK in bad state, Sending CLOSE event\n",
					core);
			msm_vidc_queue_v4l2_event(inst,
					V4L2_EVENT_MSM_VIDC_CLOSE_DONE);
			goto exit;
		}
		rc = msm_comm_try_state(inst, MSM_VIDC_CLOSE_DONE);
		/* Clients rely on this event for joining poll thread.
		 * This event should be returned even if firmware has
		 * failed to respond */
		msm_vidc_queue_v4l2_event(inst, V4L2_EVENT_MSM_VIDC_CLOSE_DONE);
		break;
	default:
		dprintk(VIDC_ERR, "Unknown Decoder Command\n");
		rc = -ENOTSUPP;
		goto exit;
	}
	if (rc) {
		dprintk(VIDC_ERR, "Failed to exec decoder cmd %d\n", dec->cmd);
		goto exit;
	}
exit:
	return rc;
}


static const struct vb2_ops msm_vdec_vb2q_ops = {
	.queue_setup = msm_vdec_queue_setup,
	.start_streaming = msm_vdec_start_streaming,
	.buf_queue = msm_vdec_buf_queue,
	.stop_streaming = msm_vdec_stop_streaming,
};

const struct vb2_ops *msm_vdec_get_vb2q_ops(void)
{
	return &msm_vdec_vb2q_ops;
}

int msm_vdec_inst_init(struct msm_vidc_inst *inst)
{
	int rc = 0;
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid input = %pK\n", inst);
		return -EINVAL;
	}
	inst->fmts[OUTPUT_PORT] = &vdec_formats[1];
	inst->fmts[CAPTURE_PORT] = &vdec_formats[0];
	inst->prop.height[CAPTURE_PORT] = DEFAULT_HEIGHT;
	inst->prop.width[CAPTURE_PORT] = DEFAULT_WIDTH;
	inst->prop.height[OUTPUT_PORT] = DEFAULT_HEIGHT;
	inst->prop.width[OUTPUT_PORT] = DEFAULT_WIDTH;
	inst->capability.height.min = MIN_SUPPORTED_HEIGHT;
	inst->capability.height.max = DEFAULT_HEIGHT;
	inst->capability.width.min = MIN_SUPPORTED_WIDTH;
	inst->capability.width.max = DEFAULT_WIDTH;
	inst->capability.buffer_mode[OUTPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->capability.buffer_mode[CAPTURE_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->capability.secure_output2_threshold.min = 0;
	inst->capability.secure_output2_threshold.max = 0;
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->buffer_mode_set[CAPTURE_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->prop.fps = 30;
	return rc;
}

static inline enum buffer_mode_type get_buf_type(int val)
{
	switch (val) {
	case V4L2_MPEG_VIDC_VIDEO_STATIC:
		return HAL_BUFFER_MODE_STATIC;
	case V4L2_MPEG_VIDC_VIDEO_RING:
		return HAL_BUFFER_MODE_RING;
	case V4L2_MPEG_VIDC_VIDEO_DYNAMIC:
		return HAL_BUFFER_MODE_DYNAMIC;
	default:
		dprintk(VIDC_ERR, "%s: invalid buf type: %d\n", __func__, val);
	}
	return 0;
}

static int check_tz_dynamic_buffer_support(void)
{
	int rc = 0;
	int version = scm_get_feat_version(TZ_DYNAMIC_BUFFER_FEATURE_ID);

	/*
	 * if the version is < 1.1.0 then dynamic buffer allocation is
	 * not supported
	 */
	if (version < TZ_FEATURE_VERSION(1, 1, 0)) {
		dprintk(VIDC_DBG,
			"Dynamic buffer mode not supported, tz version is : %u vs required : %u\n",
			version, TZ_FEATURE_VERSION(1, 1, 0));
		rc = -ENOTSUPP;
	}
	return rc;
}

static int try_get_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_profile_level profile_level;
	union hal_get_property hprop;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;
	dprintk(VIDC_DBG, "%s ctrl->id:%x\n", __func__, ctrl->id);
	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDC_VIDEO_H263_PROFILE:
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE:
		rc = msm_comm_try_get_prop(inst,
						HAL_PARAM_PROFILE_LEVEL_CURRENT,
						&hprop);
		if (rc) {
			dprintk(VIDC_ERR, "%s Error rc:%d\n", __func__, rc);
			return rc;
		}
		profile_level = hprop.profile_level;
		ctrl->val = profile_level.profile;
		dprintk(VIDC_DBG, "%s: PROFILE ctrl->id:%x ctrl->val:%d\n",
					__func__, ctrl->id, ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_H263_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL:
		rc = msm_comm_try_get_prop(inst,
						HAL_PARAM_PROFILE_LEVEL_CURRENT,
						&hprop);
		if (rc) {
			dprintk(VIDC_ERR, "%s Error rc:%d\n", __func__, rc);
			return rc;
		}
		profile_level = hprop.profile_level;
		ctrl->val = profile_level.level;
		dprintk(VIDC_DBG, "%s: LEVEL ctrl->id:%x ctrl->val:%d\n",
					__func__, ctrl->id, ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE_SCALING_THRESHOLD:
		if (!(inst->flags & VIDC_SECURE) ||
			!inst->capability.secure_output2_threshold.max) {
			dprintk(VIDC_ERR, "%s id:%x invalid configuration\n",
					__func__, ctrl->id);
			rc = -EINVAL;
			break;
		}
		dprintk(VIDC_DBG,
				"Secure Scaling Threshold is : %d",
				inst->capability.secure_output2_threshold.max);
		ctrl->val = inst->capability.secure_output2_threshold.max;
		break;
	default:
		dprintk(VIDC_DBG, "%s id:%x not supported\n",
					__func__, ctrl->id);
		break;
	}
	return rc;
}

static int vdec_v4l2_to_hal(int id, int value)
{
	switch (id) {
		/* H264 */
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
			return HAL_H264_PROFILE_BASELINE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
			return HAL_H264_PROFILE_CONSTRAINED_BASE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
			return HAL_H264_PROFILE_MAIN;
		case V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED:
			return HAL_H264_PROFILE_EXTENDED;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
			return HAL_H264_PROFILE_HIGH;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10:
			return HAL_H264_PROFILE_HIGH10;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422:
			return HAL_H264_PROFILE_HIGH422;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE:
			return HAL_H264_PROFILE_HIGH444;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_0:
			return HAL_H264_LEVEL_1;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1B:
			return HAL_H264_LEVEL_1b;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_1:
			return HAL_H264_LEVEL_11;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_2:
			return HAL_H264_LEVEL_12;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_3:
			return HAL_H264_LEVEL_13;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_0:
			return HAL_H264_LEVEL_2;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_1:
			return HAL_H264_LEVEL_21;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_2:
			return HAL_H264_LEVEL_22;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_0:
			return HAL_H264_LEVEL_3;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_1:
			return HAL_H264_LEVEL_31;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_2:
			return HAL_H264_LEVEL_32;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_0:
			return HAL_H264_LEVEL_4;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_1:
			return HAL_H264_LEVEL_41;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_2:
			return HAL_H264_LEVEL_42;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_0:
			return HAL_H264_LEVEL_5;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_1:
			return HAL_H264_LEVEL_51;
		default:
			goto unknown_value;
		}
	}
unknown_value:
	dprintk(VIDC_WARN, "Unknown control (%x, %d)", id, value);
	return -EINVAL;
}

static struct v4l2_ctrl *get_ctrl_from_cluster(int id,
		struct v4l2_ctrl **cluster, int ncontrols)
{
	int c;
	for (c = 0; c < ncontrols; ++c)
		if (cluster[c]->id == id)
			return cluster[c];
	return NULL;
}

static int try_set_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_nal_stream_format_supported stream_format;
	struct hal_enable_picture enable_picture;
	struct hal_enable hal_property;
	enum hal_property property_id = 0;
	u32 property_val = 0;
	void *pdata = NULL;
	struct hfi_device *hdev;
	struct hal_extradata_enable extra;
	struct hal_buffer_alloc_mode alloc_mode;
	struct hal_multi_stream multi_stream;
	struct hfi_scs_threshold scs_threshold;
	struct hal_mvc_buffer_layout layout;
	struct v4l2_ctrl *temp_ctrl = NULL;
	struct hal_profile_level profile_level;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc = is_ctrl_valid_for_codec(inst, ctrl);
	if (rc)
		return rc;

	/* Small helper macro for quickly getting a control and err checking */
#define TRY_GET_CTRL(__ctrl_id) ({ \
		struct v4l2_ctrl *__temp; \
		__temp = get_ctrl_from_cluster( \
			__ctrl_id, \
			ctrl->cluster, ctrl->ncontrols); \
		if (!__temp) { \
			dprintk(VIDC_ERR, "Can't find %s (%x) in cluster\n", \
				#__ctrl_id, __ctrl_id); \
			/* Clusters are hardcoded, if we can't find */ \
			/* something then things are massively screwed up */ \
			BUG_ON(1); \
		} \
		__temp; \
	})

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT:
		property_id =
		HAL_PARAM_NAL_STREAM_FORMAT_SELECT;
		stream_format.nal_stream_format_supported =
		(0x00000001 << ctrl->val);
		pdata = &stream_format;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER:
		property_id = HAL_PARAM_VDEC_OUTPUT_ORDER;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE:
		property_id =
			HAL_PARAM_VDEC_PICTURE_TYPE_DECODE;
		enable_picture.picture_type = ctrl->val;
		pdata = &enable_picture;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_KEEP_ASPECT_RATIO:
		property_id =
			HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_POST_LOOP_DEBLOCKER_MODE:
		property_id =
			HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_DIVX_FORMAT:
		property_id = HAL_PARAM_DIVX_FORMAT;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MB_ERROR_MAP_REPORTING:
		property_id =
			HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_CONTINUE_DATA_TRANSFER:
		property_id =
			HAL_PARAM_VDEC_CONTINUE_DATA_TRANSFER;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE:
		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE:
			inst->flags &= ~VIDC_THUMBNAIL;
			break;
		case V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_ENABLE:
			inst->flags |= VIDC_THUMBNAIL;
			break;
		}

		property_id = HAL_PARAM_VDEC_SYNC_FRAME_DECODE;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags |= VIDC_SECURE;
		dprintk(VIDC_DBG, "Setting secure mode to: %d\n",
				!!(inst->flags & VIDC_SECURE));
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		property_id = HAL_PARAM_INDEX_EXTRADATA;
		extra.index = msm_comm_get_hal_extradata_index(ctrl->val);
		extra.enable = 1;
		pdata = &extra;
		break;
	case V4L2_CID_MPEG_VIDC_SET_PERF_LEVEL:
		switch (ctrl->val) {
		case V4L2_CID_MPEG_VIDC_PERF_LEVEL_NOMINAL:
			inst->flags &= ~VIDC_TURBO;
			break;
		case V4L2_CID_MPEG_VIDC_PERF_LEVEL_TURBO:
			inst->flags |= VIDC_TURBO;
			break;
		default:
			dprintk(VIDC_ERR, "Perf mode %x not supported\n",
					ctrl->val);
			rc = -ENOTSUPP;
			break;
		}

		msm_comm_scale_clocks_and_bus(inst);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE_INPUT:
		if (ctrl->val == V4L2_MPEG_VIDC_VIDEO_DYNAMIC) {
			rc = -ENOTSUPP;
			break;
		}
		property_id = HAL_PARAM_BUFFER_ALLOC_MODE;
		alloc_mode.buffer_mode = get_buf_type(ctrl->val);
		alloc_mode.buffer_type = HAL_BUFFER_INPUT;
		inst->buffer_mode_set[OUTPUT_PORT] = alloc_mode.buffer_mode;
		pdata = &alloc_mode;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_ASSEMBLY:
		property_id = HAL_PARAM_VDEC_FRAME_ASSEMBLY;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE_OUTPUT:
		property_id = HAL_PARAM_BUFFER_ALLOC_MODE;
		alloc_mode.buffer_mode = get_buf_type(ctrl->val);
		if (!(alloc_mode.buffer_mode &
			inst->capability.buffer_mode[CAPTURE_PORT])) {
			dprintk(VIDC_DBG,
				"buffer mode[%d] not supported for Capture Port\n",
				ctrl->val);
			rc = -ENOTSUPP;
			break;
		}
		if ((alloc_mode.buffer_mode == HAL_BUFFER_MODE_DYNAMIC) &&
			(inst->flags & VIDC_SECURE) &&
			check_tz_dynamic_buffer_support()) {
				rc = -ENOTSUPP;
				break;
		}
		alloc_mode.buffer_type = HAL_BUFFER_OUTPUT;
		pdata = &alloc_mode;
		inst->buffer_mode_set[CAPTURE_PORT] = alloc_mode.buffer_mode;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE:
		if (ctrl->val && !(inst->capability.pixelprocess_capabilities &
				HAL_VIDEO_DECODER_MULTI_STREAM_CAPABILITY)) {
			dprintk(VIDC_ERR, "Downscaling not supported: 0x%x\n",
				ctrl->id);
			rc = -ENOTSUPP;
			break;
		}
		switch (ctrl->val) {
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY:
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT;
			multi_stream.enable = true;
			pdata = &multi_stream;
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_VDEC_MULTI_STREAM,
				pdata);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed : Enabling OUTPUT port : %d\n",
					rc);
				break;
			}
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT2;
			multi_stream.enable = false;
			pdata = &multi_stream;
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_VDEC_MULTI_STREAM,
				pdata);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed:Disabling OUTPUT2 port : %d\n",
					rc);
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_SECONDARY:
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT2;
			multi_stream.enable = true;
			pdata = &multi_stream;
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_VDEC_MULTI_STREAM,
				pdata);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed :Enabling OUTPUT2 port : %d\n",
					rc);
					break;
			}
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT;
			multi_stream.enable = false;
			pdata = &multi_stream;
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_VDEC_MULTI_STREAM,
				pdata);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed :Disabling OUTPUT port : %d\n",
					rc);
			break;
		default:
			dprintk(VIDC_ERR,
				"Failed : Unsupported multi stream setting\n");
			rc = -ENOTSUPP;
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SCS_THRESHOLD:
		property_id = HAL_PARAM_VDEC_SCS_THRESHOLD;
		scs_threshold.threshold_value = ctrl->val;
		pdata = &scs_threshold;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MVC_BUFFER_LAYOUT:
		property_id = HAL_PARAM_MVC_BUFFER_LAYOUT;
		layout.layout_type = msm_comm_get_hal_buffer_layout(ctrl->val);
		layout.bright_view_first = 0;
		layout.ngap = 0;
		pdata = &layout;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR:
		property_id = HAL_PARAM_VDEC_CONCEAL_COLOR;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_LEVEL);
		property_id =
			HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = vdec_v4l2_to_hal(ctrl->id,
				ctrl->val);
		profile_level.level = vdec_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_PROFILE);
		property_id =
			HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = vdec_v4l2_to_hal(ctrl->id,
				ctrl->val);
		profile_level.profile = vdec_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_PROFILE,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BUFFER_SIZE_LIMIT:
		inst->capability.buffer_size_limit = ctrl->val;
		dprintk(VIDC_DBG,
			"Limiting input buffer size to :%u\n", ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_NON_SECURE_OUTPUT2:
		property_id = HAL_PARAM_VDEC_NON_SECURE_OUTPUT2;
		hal_property.enable = ctrl->val;
		dprintk(VIDC_DBG, "%s non_secure output2\n",
			ctrl->val ? "Enabling" : "Disabling");
		pdata = &hal_property;
		break;
	default:
		break;
	}
#undef TRY_GET_CTRL

	if (!rc && property_id) {
		dprintk(VIDC_DBG,
			"Control: HAL property=0x%x,ctrl: id=0x%x,value=0x%x\n",
			property_id, ctrl->id, ctrl->val);
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, property_id, pdata);
	}

	return rc;
}

static int msm_vdec_op_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int rc = 0, c = 0;
	struct msm_vidc_inst *inst = container_of(ctrl->handler,
				struct msm_vidc_inst, ctrl_handler);
	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to start done state\n", inst);
		goto failed_open_done;
	}

	for (c = 0; c < ctrl->ncontrols; ++c) {
		if (ctrl->cluster[c]->is_new) {
			rc = try_set_ctrl(inst, ctrl->cluster[c]);
			if (rc) {
				dprintk(VIDC_ERR, "Failed setting %x\n",
						ctrl->cluster[c]->id);
				break;
			}
		}
	}

failed_open_done:
	if (rc)
		dprintk(VIDC_ERR, "Failed setting control: %x (%s)",
				ctrl->id, v4l2_ctrl_get_name(ctrl->id));
	return rc;
}

static int msm_vdec_op_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int rc = 0, c = 0;
	struct msm_vidc_inst *inst = container_of(ctrl->handler,
				struct msm_vidc_inst, ctrl_handler);
	struct v4l2_ctrl *master = ctrl->cluster[0];

	rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to start done state\n", inst);
		goto failed_open_done;
	}
	for (c = 0; c < master->ncontrols; ++c) {
		int d = 0;
		for (d = 0; d < NUM_CTRLS; ++d) {
			if (master->cluster[c]->id == inst->ctrls[d]->id &&
				inst->ctrls[d]->flags &
				V4L2_CTRL_FLAG_VOLATILE) {
				rc = try_get_ctrl(inst, master->cluster[c]);
				if (rc) {
					dprintk(VIDC_ERR, "Failed getting %x\n",
							master->cluster[c]->id);
					return rc;
				}
				break;
			}
		}
	}
	return rc;

failed_open_done:
	if (rc)
		dprintk(VIDC_ERR, "Failed to get hal property\n");
	return rc;
}

static const struct v4l2_ctrl_ops msm_vdec_ctrl_ops = {

	.s_ctrl = msm_vdec_op_s_ctrl,
	.g_volatile_ctrl = msm_vdec_op_g_volatile_ctrl,
};

const struct v4l2_ctrl_ops *msm_vdec_get_ctrl_ops(void)
{
	return &msm_vdec_ctrl_ops;
}

int msm_vdec_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl)
{
	return v4l2_s_ctrl(NULL, &inst->ctrl_handler, ctrl);
}
int msm_vdec_g_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl)
{
	return v4l2_g_ctrl(&inst->ctrl_handler, ctrl);
}

static struct v4l2_ctrl **get_super_cluster(struct msm_vidc_inst *inst,
				int *size)
{
	int c = 0, sz = 0;
	struct v4l2_ctrl **cluster = kmalloc(sizeof(struct v4l2_ctrl *) *
			NUM_CTRLS, GFP_KERNEL);

	if (!size || !cluster || !inst)
		return NULL;

	for (c = 0; c < NUM_CTRLS; c++)
		cluster[sz++] = inst->ctrls[c];

	*size = sz;
	return cluster;
}

int msm_vdec_ctrl_init(struct msm_vidc_inst *inst)
{
	int idx = 0;
	struct v4l2_ctrl_config ctrl_cfg = {0};
	int ret_val = 0;
	int cluster_size = 0;

	if (!inst) {
		dprintk(VIDC_ERR, "%s - invalid input\n", __func__);
		return -EINVAL;
	}

	inst->ctrls = kzalloc(sizeof(struct v4l2_ctrl *) * NUM_CTRLS,
				GFP_KERNEL);
	if (!inst->ctrls) {
		dprintk(VIDC_ERR, "%s - failed to allocate ctrl\n", __func__);
		return -ENOMEM;
	}

	ret_val = v4l2_ctrl_handler_init(&inst->ctrl_handler, NUM_CTRLS);

	if (ret_val) {
		dprintk(VIDC_ERR, "CTRL ERR: Control handler init failed, %d\n",
				inst->ctrl_handler.error);
		return ret_val;
	}

	for (; idx < NUM_CTRLS; idx++) {
		struct v4l2_ctrl *ctrl = NULL;
		if (IS_PRIV_CTRL(msm_vdec_ctrls[idx].id)) {
			/*add private control*/
			ctrl_cfg.def = msm_vdec_ctrls[idx].default_value;
			ctrl_cfg.flags = 0;
			ctrl_cfg.id = msm_vdec_ctrls[idx].id;
			/* ctrl_cfg.is_private =
			 * msm_vdec_ctrls[idx].is_private;
			 * ctrl_cfg.is_volatile =
			 * msm_vdec_ctrls[idx].is_volatile;*/
			ctrl_cfg.max = msm_vdec_ctrls[idx].maximum;
			ctrl_cfg.min = msm_vdec_ctrls[idx].minimum;
			ctrl_cfg.menu_skip_mask =
				msm_vdec_ctrls[idx].menu_skip_mask;
			ctrl_cfg.name = msm_vdec_ctrls[idx].name;
			ctrl_cfg.ops = &msm_vdec_ctrl_ops;
			ctrl_cfg.step = msm_vdec_ctrls[idx].step;
			ctrl_cfg.type = msm_vdec_ctrls[idx].type;
			ctrl_cfg.qmenu = msm_vdec_ctrls[idx].qmenu;

			ctrl = v4l2_ctrl_new_custom(&inst->ctrl_handler,
					&ctrl_cfg, NULL);
		} else {
			if (msm_vdec_ctrls[idx].type == V4L2_CTRL_TYPE_MENU) {
				ctrl = v4l2_ctrl_new_std_menu(
					&inst->ctrl_handler,
					&msm_vdec_ctrl_ops,
					msm_vdec_ctrls[idx].id,
					msm_vdec_ctrls[idx].maximum,
					msm_vdec_ctrls[idx].menu_skip_mask,
					msm_vdec_ctrls[idx].default_value);
			} else {
				ctrl = v4l2_ctrl_new_std(&inst->ctrl_handler,
					&msm_vdec_ctrl_ops,
					msm_vdec_ctrls[idx].id,
					msm_vdec_ctrls[idx].minimum,
					msm_vdec_ctrls[idx].maximum,
					msm_vdec_ctrls[idx].step,
					msm_vdec_ctrls[idx].default_value);
			}
		}

		if (!ctrl) {
			dprintk(VIDC_ERR, "%s - invalid ctrl\n", __func__);
			return -EINVAL;
		}

		switch (msm_vdec_ctrls[idx].id) {
		case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
		case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		case V4L2_CID_MPEG_VIDC_VIDEO_H263_PROFILE:
		case V4L2_CID_MPEG_VIDC_VIDEO_H263_LEVEL:
		case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		case V4L2_CID_MPEG_VIDC_VIDEO_SECURE_SCALING_THRESHOLD:
			ctrl->flags |= msm_vdec_ctrls[idx].flags;
			break;
		}

		ret_val = inst->ctrl_handler.error;
		if (ret_val) {
			dprintk(VIDC_ERR,
					"Error adding ctrl (%s) to ctrl handle, %d\n",
					msm_vdec_ctrls[idx].name,
					inst->ctrl_handler.error);
			return ret_val;
		}

		inst->ctrls[idx] = ctrl;
	}

	/* Construct a super cluster of all controls */
	inst->cluster = get_super_cluster(inst, &cluster_size);
	if (!inst->cluster || !cluster_size) {
		dprintk(VIDC_WARN,
				"Failed to setup super cluster\n");
		return -EINVAL;
	}

	v4l2_ctrl_cluster(cluster_size, inst->cluster);

	return ret_val;
}

int msm_vdec_ctrl_deinit(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	kfree(inst->ctrls);
	kfree(inst->cluster);
	v4l2_ctrl_handler_free(&inst->ctrl_handler);

	return 0;
}
