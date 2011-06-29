/**
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __OV9726_H__
#define __OV9726_H__

#include <linux/ioctl.h>

#define OV9726_I2C_ADDR			0x20

#define OV9726_IOCTL_SET_MODE		_IOW('o', 1, struct ov9726_mode)
#define OV9726_IOCTL_SET_FRAME_LENGTH	_IOW('o', 2, __u32)
#define OV9726_IOCTL_SET_COARSE_TIME	_IOW('o', 3, __u32)
#define OV9726_IOCTL_SET_GAIN		_IOW('o', 4, __u16)
#define OV9726_IOCTL_GET_STATUS		_IOR('o', 5, __u8)

struct ov9726_mode {
	int	mode_id;
	int	xres;
	int	yres;
	__u32	frame_length;
	__u32	coarse_time;
	__u16	gain;
};

struct ov9726_reg {
	__u16	addr;
	__u16	val;
};

struct ov9726_cust_mode {
	struct ov9726_mode	mode;
	__u16			reg_num;
	struct ov9726_reg	*reg_seq;
};

#define OV9726_TABLE_WAIT_MS		0
#define OV9726_TABLE_END		1

#ifdef __KERNEL__
#define OV9726_REG_FRAME_LENGTH_HI	0x340
#define OV9726_REG_FRAME_LENGTH_LO	0x341
#define OV9726_REG_COARSE_TIME_HI	0x202
#define OV9726_REG_COARSE_TIME_LO	0x203
#define OV9726_REG_GAIN_HI		0x204
#define OV9726_REG_GAIN_LO		0x205

#define OV9726_MAX_RETRIES		3

struct ov9726_platform_data {
	int	(*power_on)(void);
	int	(*power_off)(void);
};
#endif /* __KERNEL__ */

#endif  /* __OV9726_H__ */

