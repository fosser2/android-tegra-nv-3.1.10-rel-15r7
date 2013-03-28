/*
 * bcb.c: Reboot hook to write bootloader commands to
 * the Android bootloader control block
 *
 * (C) Copyright 2012 Intel Corporation
 * Author: Andrew Boie <andrew.p.boie@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/blkdev.h>
#include <linux/reboot.h>

#define BCB_MAGIC 0xFEEDFACE

/* Persistent area written by Android recovery console and Linux bcb driver
 * reboot hook for communication with the bootloader. Bootloader must
 * gracefully handle this area being unitinitailzed */
struct bootloader_message {
 /* Directive to the bootloader on what it needs to do next.
 * Possible values:
 * boot-NNN - Automatically boot into label NNN
 * bootonce-NNN - Automatically boot into label NNN, clearing this
 * field afterwards
 * anything else / garbage - Boot default label */
 char command[32];

 /* Storage area for error codes when using the BCB to boot into special
 * boot targets, e.g. for baseband update. Not used here. */
 char status[32];

 /* Area for recovery console to stash its command line arguments
 * in case it is reset and the cache command file is erased.
 * Not used here. */
 char recovery[1024];

 /* Magic sentinel value written by the bootloader; don't update this
 * if not equalto to BCB_MAGIC */
 uint32_t magic;
};

/* TODO: device names/partition numbers are unstable. Add support for looking
 * by GPT partition UUIDs */
static char *bootdev = "mmcblk0";
module_param(bootdev, charp, S_IRUGO);
MODULE_PARM_DESC(bootdev, "Block device for bootloader communication");

static int partno = 5;
module_param(partno, int, S_IRUGO);
MODULE_PARM_DESC(partno, "Partition number for bootloader communication");

static int device_match(struct device *dev, void *data)
{
 if (strcmp(dev_name(dev), bootdev) == 0)
 return 1;
 return 0;
}

static struct block_device *get_emmc_bdev(void)
{
 struct block_device *bdev;
 struct device *disk_device;

 disk_device = class_find_device(&block_class, NULL, NULL, device_match);
 if (!disk_device) {
 pr_err("bcb: device %s not found!", bootdev);
 return NULL;
 }
 bdev = bdget_disk(dev_to_disk(disk_device), partno);
 if (!bdev) {
 dev_err(disk_device, "bcb: unable to get disk (%s,%d)",
 bootdev, partno);
 return NULL;
 }
 /* Note: this bdev ref will be freed after first
 bdev_get/bdev_put cycle */
 return bdev;
}

static u64 last_lba(struct block_device *bdev)
{
 if (!bdev || !bdev->bd_inode)
 return 0;
 return div_u64(bdev->bd_inode->i_size,
 bdev_logical_block_size(bdev)) - 1ULL;
}

static size_t read_lba(struct block_device *bdev,
 u64 lba, u8 *buffer, size_t count)
{
 size_t totalreadcount = 0;
 sector_t n = lba * (bdev_logical_block_size(bdev) / 512);

 if (!buffer || lba > last_lba(bdev))
 return 0;

 while (count) {
 int copied = 512;
 Sector sect;
 unsigned char *data = read_dev_sector(bdev, n++, &sect);
 if (!data)
 break;
 if (copied > count)
 copied = count;
 memcpy(buffer, data, copied);
 put_dev_sector(sect);
 buffer += copied;
 totalreadcount += copied;
 count -= copied;
 }
 return totalreadcount;
}

static size_t write_lba(struct block_device *bdev,
 u64 lba, u8 *buffer, size_t count)
{
 size_t totalwritecount = 0;
 sector_t n = lba * (bdev_logical_block_size(bdev) / 512);

 if (!buffer || lba > last_lba(bdev))
 return 0;

 while (count) {
 int copied = 512;
 Sector sect;
 unsigned char *data = read_dev_sector(bdev, n++, &sect);
 if (!data)
 break;
 if (copied > count)
 copied = count;
 memcpy(data, buffer, copied);
 set_page_dirty(sect.v);
 unlock_page(sect.v);
 put_dev_sector(sect);
 buffer += copied;
 totalwritecount += copied;
 count -= copied;
 }
 sync_blockdev(bdev);
 return totalwritecount;
}

static int bcb_reboot_notifier_call(
 struct notifier_block *notifier,
 unsigned long what, void *data)
{
 int ret = NOTIFY_DONE;
 char *cmd = (char *)data;
 struct block_device *bdev = NULL;
 struct bootloader_message *bcb = NULL;

 if (what != SYS_RESTART || !data)
 goto out;

 bdev = get_emmc_bdev();
 if (!bdev)
 goto out;

 /* make sure the block device is open rw */
 if (blkdev_get(bdev, FMODE_READ | FMODE_WRITE, NULL) < 0) {
 pr_err("bcb: blk_dev_get failed!");
 goto out;
 }

 bcb = kmalloc(sizeof(*bcb), GFP_KERNEL);
 if (!bcb) {
 pr_err("bcb: out of memory");
 goto out;
 }

 if (read_lba(bdev, 0, (u8 *)bcb, sizeof(*bcb)) != sizeof(*bcb)) {
 pr_err("bcb: couldn't read bootloader control block");
 goto out;
 }

 if (bcb->magic != BCB_MAGIC) {
 pr_err("bcb: bootloader control block corrupted, not writing");
 goto out;
 }

 /* When the bootloader reads this area, it will null-terminate it
 * and check if it matches any existing boot labels */
 snprintf(bcb->command, sizeof(bcb->command), "bootonce-%s", cmd);

 if (write_lba(bdev, 0, (u8 *)bcb, sizeof(*bcb)) != sizeof(*bcb)) {
 pr_err("bcb: couldn't write bootloader control block");
 goto out;
 }

 ret = NOTIFY_OK;
out:
 kfree(bcb);
 if (bdev)
 blkdev_put(bdev, FMODE_READ | FMODE_WRITE);

 return ret;
}

static struct notifier_block bcb_reboot_notifier = {
 .notifier_call = bcb_reboot_notifier_call,
};

static int __init bcb_init(void)
{
 if (partno < 1) {
 pr_err("bcb: partition number not specified");
 return -1;
 }
 if (register_reboot_notifier(&bcb_reboot_notifier)) {
 pr_err("bcb: unable to register reboot notifier");
 return -1;
 }
 pr_info("bcb: writing commands to (%s,%d)",
 bootdev, partno);
 return 0;
}
module_init(bcb_init);

static void __exit bcb_exit(void)
{
 unregister_reboot_notifier(&bcb_reboot_notifier);
}
module_exit(bcb_exit);

MODULE_AUTHOR("Andrew Boie <andrew.p.boie@intel.com>");
MODULE_DESCRIPTION("bootloader communication module");
MODULE_LICENSE("GPL v2");
