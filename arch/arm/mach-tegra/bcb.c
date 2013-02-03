/*
 * bcb.c: Reboot hook to write bootloader commands to
 *        the Android bootloader control block
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

#include <linux/mtd/nand.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/mtd.h>

#define BCB_MAGIC	0xFEEDFACE

/* Persistent area written by Android recovery console and Linux bcb driver
 * reboot hook for communication with the bootloader. Bootloader must
 * gracefully handle this area being unitinitailzed */
struct bootloader_message {
	/* Directive to the bootloader on what it needs to do next.
	 * Possible values:
	 *   boot-NNN - Automatically boot into label NNN
	 *   bootonce-NNN - Automatically boot into label NNN, clearing this
	 *     field afterwards
	 *   anything else / garbage - Boot default label */
	char command[32];

	/* Storage area for error codes when using the BCB to boot into special
	 * boot targets, e.g. for baseband update. Not used here. */
	char status[32];

	/* Area for recovery console to stash its command line arguments
	 * in case it is reset and the cache command file is erased. */
	char recovery[1024];

	/* Magic sentinel value written by the bootloader; don't update this
	 * if not equalto to BCB_MAGIC */
	uint32_t magic;
};

static int misc_write(struct bootloader_message *boot);

static void mmtdchar_erase_callback (struct erase_info *instr)
{
        wake_up((wait_queue_head_t *)instr->priv);
}

static int misc_write(struct bootloader_message *boot)
{
        int i = 0;
        struct mtd_info *mtd = NULL;
	struct erase_info *erase;
	unsigned int total_block;
	loff_t offs = 0;
	int ret = 0;
	char *data;

	mtd = get_mtd_device_nm("misc");

	if(IS_ERR(mtd)) {
		pr_err("bcb: could not find misc partition\n");
		return -1;
	}
	
	total_block = (uint32_t)(mtd->size) / mtd->erasesize;

	for(i=0; i<total_block; i++) {
		offs = mtd->erasesize * i; 
		/* Check if it's bad block */
		if(!(mtd->block_isbad)) {
			if(mtd->block_isbad(mtd, offs) > 0) {
				continue;
			}
		}
		
		/* Erase this block */
		erase=kzalloc(sizeof(struct erase_info),GFP_KERNEL);
		if (!erase) {
			pr_err("bcb: out of memory\n");
			return -1;
		} else {
			wait_queue_head_t waitq;
			
			DECLARE_WAITQUEUE(wait, current);
			
			init_waitqueue_head(&waitq);
			
			erase->addr = offs;
			erase->len = mtd->erasesize;
			erase->mtd = mtd;
			erase->callback = mmtdchar_erase_callback;
			erase->priv = (unsigned long)&waitq;
			
			/*
                          FIXME: Allow INTERRUPTIBLE. Which means
                          not having the wait_queue head on the stack.
			  
                          If the wq_head is on the stack, and we
                          leave because we got interrupted, then the
                          wq_head is no longer there when the
                          callback routine tries to wake us up.
			*/
			ret = mtd->erase(mtd, erase);
			if (!ret) {
				set_current_state(TASK_UNINTERRUPTIBLE);
				add_wait_queue(&waitq, &wait);
				if (erase->state != MTD_ERASE_DONE &&
				    erase->state != MTD_ERASE_FAILED)
					schedule();
				remove_wait_queue(&waitq, &wait);
				set_current_state(TASK_RUNNING);
				
				ret = (erase->state == MTD_ERASE_FAILED)?-EIO:0;
			} else {
				pr_info("Erased MISC partition successfully\n");
			}
			kfree(erase);
		}
	}

	data=kmalloc(mtd->erasesize, GFP_KERNEL);

	if(!data) {
		pr_err("bcb: out of memory\n");
		return -1;
	}

	memset(data, 0, mtd->erasesize);
	memcpy((data+0x800), (char *)boot, sizeof(*boot));
	
	/* one block is enough */
	i = 0;
	while(i<total_block){
		unsigned int retlen = 0;
		
		offs = mtd->erasesize * i;
		i++;
                /* Check if it's bad block */
                if(!(mtd->block_isbad)) {
                        if(mtd->block_isbad(mtd, offs) > 0) {
                                continue;
                        }
                }
		
		ret = (*(mtd->write))(mtd, offs, mtd->erasesize, &retlen, data);
		
		if(!ret) {
			pr_info("Write misc block %d successfully.\n", i);
			break;
		} else {
			pr_info("Failed to write misc block %d\n", i);
			continue;
		}
	}

	return 0;
}

static int bcb_reboot_notifier_call(
		struct notifier_block *notifier,
		unsigned long what, void *data)
{
	int ret = NOTIFY_DONE;
	char *cmd = (char *)data;
	struct bootloader_message *bcb = NULL;

	if (what != SYS_RESTART || !data)
		goto out;

	bcb = kmalloc(sizeof(*bcb), GFP_KERNEL);
	if (!bcb) {
		pr_err("bcb: out of memory\n");
		goto out;
	}

	memset(bcb, 0, sizeof(*bcb));
	snprintf(bcb->command, sizeof(bcb->command), "boot-%s", cmd);
	snprintf(bcb->recovery, sizeof(bcb->recovery), "%s", cmd);
	misc_write(bcb);

	ret = NOTIFY_OK;
out:
	if(bcb)
		kfree(bcb);

	return ret;
}

static struct notifier_block bcb_reboot_notifier = {
	.notifier_call = bcb_reboot_notifier_call,
};

static int __init bcb_init(void)
{
	if (register_reboot_notifier(&bcb_reboot_notifier)) {
		pr_err("bcb: unable to register reboot notifier\n");
		return -1;
	}
	pr_info("bcb: registered reboot notifier\n");
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
