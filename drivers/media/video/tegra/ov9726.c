/*
 * ov9726.c - ov9726 sensor driver
 *
 * Copyright (c) 2011, NVIDIA, All Rights Reserved.
 *
 * Contributors:
 *	  Charlie Huang <chahuang@nvidia.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <mach/iomap.h>
#include <asm/atomic.h>

#include <media/ov9726.h>

struct ov9726_devinfo {
	struct miscdevice		miscdev_info;
	struct i2c_client		*i2c_client;
	struct ov9726_platform_data	*pdata;
	atomic_t			in_use;
	__u32				mode;
};

/* 2 regs to program frame length */
static inline void
ov9726_get_frame_length_regs(struct ov9726_reg *regs, u32 frame_length)
{
	regs->addr = OV9726_REG_FRAME_LENGTH_HI;
	regs->val = (frame_length >> 8) & 0xff;
	regs++;
	regs->addr = OV9726_REG_FRAME_LENGTH_LO;
	regs->val = frame_length & 0xff;
}

/* 3 regs to program coarse time */
static inline void
ov9726_get_coarse_time_regs(struct ov9726_reg *regs, u32 coarse_time)
{
	regs->addr = OV9726_REG_COARSE_TIME_HI;
	regs->val = (coarse_time >> 8) & 0xff;
	regs++;
	regs->addr = OV9726_REG_COARSE_TIME_LO;
	regs->val = coarse_time & 0xff;
}

/* 1 reg to program gain */
static inline void
ov9726_get_gain_reg(struct ov9726_reg *regs, u16 gain)
{
	regs->addr = OV9726_REG_GAIN_HI;
	regs->val = (gain >> 8) & 0xff;
	regs++;
	regs->addr = OV9726_REG_GAIN_LO;
	regs->val = gain & 0xff;
}

static inline void
msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base*1000, delay_base*1000 + 500);
}

static int
ov9726_read_reg(struct i2c_client *client, u16 addr, u8 *val)
{
	int	err;
	struct i2c_msg  msg[2];
	unsigned char   data[3];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8)(addr >> 8);
	data[1] = (u8)(addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		err = -EINVAL;
	else	{
		*val = data[2];
		err = 0;
	}

	return err;
}

static int
ov9726_write_reg(struct i2c_client *client, u16 addr, u8 val)
{
	int		err;
	struct i2c_msg  msg;
	unsigned char   data[3];
	int		retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8)(addr >> 8);
	data[1] = (u8)(addr & 0xff);
	data[2] = (u8)(val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			break;

		retry++;
		dev_err(&client->dev,
			"ov9726: i2c transfer failed, retrying %x %x\n", addr, val);
		msleep_range(3);
	} while (retry <= OV9726_MAX_RETRIES);

	return (err != 1);
}

static int
ov9726_write_reg16(struct i2c_client *client, u16 addr, u16 val)
{
	int shift = (sizeof(u16)/sizeof(u8) - 1) * 8;
	int i, ret;

	for (i = 0; i < sizeof(u16)/sizeof(u8); i++)  {
		ret = ov9726_write_reg(client, addr, (u8)((val >> shift) & 0xff));
		if (ret)
			break;

		addr++;
		shift -= 8;
	}

	return ret;
}

static int
ov9726_write_table(
	struct i2c_client *client,
	struct ov9726_reg table[],
	struct ov9726_reg override_list[],
	int num_override_regs)
{
	const struct ov9726_reg	*next;
	int			err = 0;
	int			i;
	u16			val;

	dev_info(&client->dev, "ov9726_write_table\n");

	for (next = table; next->addr != OV9726_TABLE_END; next++) {

		if (next->addr == OV9726_TABLE_WAIT_MS) {
			msleep_range(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = ov9726_write_reg(client, next->addr, val);
		if (err)
			break;
	}

	return err;
}

static int
ov9726_set_mode(
	struct ov9726_devinfo *dev,
	struct ov9726_mode *mode,
	struct ov9726_reg *reg_list)
{
	struct i2c_client 	*i2c_client = dev->i2c_client;
	int 			err = 0;

	dev_info(&i2c_client->dev, "%s: xres %u yres %u framelength %u coarsetime %u gain %u\n",
			__func__, mode->xres, mode->yres, mode->frame_length,
			mode->coarse_time, mode->gain);

	if (!reg_list) {
		dev_err(&i2c_client->dev, "%s: empty reg list\n", __func__);
		return -EINVAL;
	}

	if (dev->mode != mode->mode_id) {
		err = ov9726_write_table(i2c_client, reg_list, NULL, 0);
		if (!err)
			dev->mode = mode->mode_id;
	}

	return err;
}

static int
ov9726_set_frame_length(struct ov9726_devinfo *dev, u32 frame_length)
{
	return ov9726_write_reg16(dev->i2c_client,
				  OV9726_REG_FRAME_LENGTH_HI,
				  frame_length);
}

static int
ov9726_set_coarse_time(struct ov9726_devinfo *dev,u32 coarse_time)
{
	int ret, ret_hold;

	// hold register value
	ret_hold = ov9726_write_reg(dev->i2c_client, 0x104, 0x01);
	if (ret_hold)
		return ret_hold;

	ret = ov9726_write_reg16(dev->i2c_client,
			  OV9726_REG_COARSE_TIME_HI,
			  coarse_time);

	// release hold, update register value
	ret_hold = ov9726_write_reg(dev->i2c_client, 0x104, 0x00);

	if (ret)
		return ret;
	else
		return ret_hold;
}

static int ov9726_set_gain(struct ov9726_devinfo *dev, u16 gain)
{
	return ov9726_write_reg16(dev->i2c_client, OV9726_REG_GAIN_HI, gain);
}

static int ov9726_get_status(struct ov9726_devinfo *dev, u8 *status)
{
	int err;

	err = ov9726_read_reg(dev->i2c_client, 0x003, status);
	*status = 0;
	return err;
}

static long
ov9726_ioctl(struct file *file,unsigned int cmd,unsigned long arg)
{
	struct ov9726_devinfo	*dev = file->private_data;
	struct i2c_client 	*i2c_client = dev->i2c_client;
	int			 err = 0;

	switch (cmd) {
		case OV9726_IOCTL_SET_MODE:
		{
			struct ov9726_cust_mode	cust_mode;
			struct ov9726_reg	*reg_list = NULL;

			if (copy_from_user(&cust_mode,
					   (const void __user *)arg,
					   sizeof(struct ov9726_cust_mode)))
				err = -EFAULT;

			if (err)
				break;

			reg_list = (struct ov9726_reg *)kzalloc(
				sizeof(struct ov9726_reg) * cust_mode.reg_num,
				GFP_KERNEL);
			if (!reg_list) {
				dev_err(&i2c_client->dev,
					"ov9726: Unable to allocate memory!\n");
				err = -ENOMEM;
			}

			if (err)
				break;

			if (copy_from_user(reg_list,
				   (const void __user *)cust_mode.reg_seq,
				   sizeof(struct ov9726_reg) * cust_mode.reg_num))
				err = -EFAULT;

			if (!err)	{
				err = ov9726_set_mode(dev, &(cust_mode.mode), reg_list);
			}

			if (reg_list)
				kfree(reg_list);
			break;
		}

		case OV9726_IOCTL_SET_FRAME_LENGTH:
			err = ov9726_set_frame_length(dev, (u32)arg);
			break;

		case OV9726_IOCTL_SET_COARSE_TIME:
			err = ov9726_set_coarse_time(dev, (u32)arg);
			break;

		case OV9726_IOCTL_SET_GAIN:
			err = ov9726_set_gain(dev, (u16)arg);
			break;

		case OV9726_IOCTL_GET_STATUS:
		{
			u8 status;

			err = ov9726_get_status(dev, &status);
			if (!err) {
				if (copy_to_user((void __user *)arg,
						&status, sizeof(status)))
					err = -EFAULT;
			}
			break;
		}

		default:
			err = -EINVAL;
			break;
	}

	return err;
}

static int ov9726_open(struct inode *inode, struct file *file)
{
	struct miscdevice 	*miscdev = file->private_data;
	struct ov9726_devinfo 	*dev_info;

	dev_info =container_of(miscdev, struct ov9726_devinfo, miscdev_info);
	// check if device is in use
	if (atomic_xchg(&dev_info->in_use, 1))
		return -EBUSY;

	if (dev_info && dev_info->pdata && dev_info->pdata->power_on)
		dev_info->pdata->power_on();
	file->private_data = dev_info;

	return 0;
}

int ov9726_release(struct inode *inode, struct file *file)
{
	struct ov9726_devinfo *dev_info;

	dev_info = file->private_data;
	if (dev_info && dev_info->pdata && dev_info->pdata->power_off)
		dev_info->pdata->power_off();

	file->private_data = NULL;
	// warn if device already released
	WARN_ON(!atomic_xchg(&dev_info->in_use, 0));
	return 0;
}

static const struct file_operations ov9726_fileops = {
	.owner		= THIS_MODULE,
	.open		= ov9726_open,
	.unlocked_ioctl	= ov9726_ioctl,
	.release	= ov9726_release,
};

static struct miscdevice ov9726_device = {
	.name		= "ov9726",
	.minor		= MISC_DYNAMIC_MINOR,
	.fops		= &ov9726_fileops,
};

static int
ov9726_probe(struct i2c_client *client,	const struct i2c_device_id *id)
{
	struct ov9726_devinfo 	*dev_info;
	int 			err = 0;

	dev_info(&client->dev, "ov9726: probing sensor.\n");

	dev_info = kzalloc(sizeof(struct ov9726_devinfo), GFP_KERNEL);
	if (!dev_info) {
		dev_err(&client->dev, "ov9726: Unable to allocate memory!\n");
		err = -ENOMEM;
		goto probe_end;
	}

	memcpy(&(dev_info->miscdev_info), &ov9726_device, sizeof(struct miscdevice));
	err = misc_register(&(dev_info->miscdev_info));
	if (err) {
		dev_err(&client->dev, "ov9726: Unable to register misc device!\n");
		goto probe_end;
	}

	dev_info->pdata = client->dev.platform_data;
	dev_info->i2c_client = client;
	atomic_set(&dev_info->in_use, 0);
	dev_info->mode = (__u32)-1;
	i2c_set_clientdata(client, dev_info);

probe_end:
	if (err) {
		if (dev_info)
			kfree(dev_info);
		dev_err(&client->dev, "failed.\n");
	}

	return err;
}

static int ov9726_remove(struct i2c_client *client)
{
	struct ov9726_devinfo *dev_info;

	dev_info = i2c_get_clientdata(client);
	i2c_set_clientdata(client, NULL);
	misc_deregister(&ov9726_device);
	kfree(dev_info);

	return 0;
}

static const struct i2c_device_id ov9726_id[] = {
	{"ov9726", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ov9726_id);

static struct i2c_driver ov9726_i2c_driver = {
	.driver	= {
		.name   = "ov9726",
		.owner  = THIS_MODULE,
		},
	.probe  = ov9726_probe,
	.remove = ov9726_remove,
	.id_table   = ov9726_id,
};

static int __init ov9726_init(void)
{
	pr_info("ov9726 sensor driver loading\n");
	return i2c_add_driver(&ov9726_i2c_driver);
}

static void __exit ov9726_exit(void)
{
	i2c_del_driver(&ov9726_i2c_driver);
}

module_init(ov9726_init);
module_exit(ov9726_exit);
