/*
 *  fs/partitions/nvtegria.c
 *  Copyright (c) 2010 Gilles Grandou
 *
 *  Nvidia uses for its Tegra2 SOCs a proprietary partition system which is 
 *  unfortunately undocumented.
 *
 *  Typically a Tegra2 system embedds an internal Flash memory (MTD or MMC).
 *  The bottom of this memory contains the initial bootstrap code which 
 *  implements a communication protocol (typically over usb) which allows a 
 *  host system (through a tool called nvflash) to access, read, write and 
 *  partition the internal flash.
 *
 *  The partition table format is not publicaly documented, and usually 
 *  partition description is passed to kernel through the command line
 *  (with tegrapart= argument whose support is available in nv-tegra tree,
 *  see http://nv-tegra.nvidia.com/ )
 *
 *  Rewriting partition table or even switching to a standard msdos is 
 *  theorically possible, but it would mean loosing support from nvflash 
 *  and from bootloader, while no real alternative exists yet.
 *
 *  Partition table format has been reverse-engineered from analysis of
 *  an existing partition table as found on Toshiba AC100/Dynabook AZ. All 
 *  fields have been guessed and there is no guarantee that it will work 
 *  in all situation nor in all other Tegra2 based products.
 *
 *
 *  The standard partitions which can be found on an AC100 are the next 
 *  ones:
 *
 *  sector size = 2048 bytes
 *
 *  Id  Name    Start   Size            Comment
 *              sector  sectors
 *
 *  1           0       1024            unreachable (bootstrap ?)
 *  2   BCT     1024    512             Boot Configuration Table
 *  3   PT      1536    256             Partition Table
 *  4   EBT     1792    1024            Boot Loader
 *  5   SOS     2816    2560            Recovery Kernel
 *  6   LNX     5376    4096            System Kernel
 *  7   MBR     9472    512             MBR - msdos partition table 
 *                                      for the rest of the disk
 *  8   APP     9984    153600          OS root filesystem
 *  ...
 * 
 *  the 1024 first sectors are hidden to the hardware one booted
 *  (so 1024 should be removed from numbers found in the partition
 *  table)
 *
 *  There is no standard magic value which can be used for sure
 *  to say that there is a nvtegra partition table at sector 512.
 *  Hence, as an heuristic, we check if the value which would be 
 *  found for the BCT partition entry are valid.
 *
 */


#include "check.h"
#include "nvtegra.h"

//#define BRIEF

typedef struct {
        unsigned id;
        char     name[4];
        unsigned type;
        unsigned unk1[2];
        char     name2[4];
        unsigned unk2[4];
        unsigned start;
        unsigned unk3;
        unsigned size;
        unsigned unk4[7];
} t_nvtegra_partinfo;


typedef struct {
        unsigned           unknown[18];
        t_nvtegra_partinfo partinfo_bct;
        t_nvtegra_partinfo partinfo[23];
} t_nvtegra_ptable;


typedef struct {
        int      valid;
        char     name[4];
        unsigned start;
        unsigned size;
} t_partinfo;


char* hidden_parts_str = CONFIG_NVTEGRA_HIDE_PARTS;



static size_t
read_dev_bytes(struct block_device *bdev, unsigned sector, char* buffer, size_t count)
{
        size_t totalreadcount = 0;

        if (!bdev || !buffer)
          return 0;

        while (count) {
                int copied = 512;
                Sector sect;
                unsigned char *data = read_dev_sector(bdev, sector++, &sect);
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



int
nvtegra_partition(struct parsed_partitions *state)
{
        t_nvtegra_ptable *pt;
        t_nvtegra_partinfo* p;
        t_partinfo* parts;
        t_partinfo* part;
        struct block_device *bdev;

        int count;
        int i;
        char *s;

        bdev = state->bdev;

        pt = kzalloc(2048, GFP_KERNEL);
        if (!pt)
                return -1;

        if (read_dev_bytes(bdev, 4096, (char*)pt, 2048) != 2048) {
                kfree(pt);
                return 0;
        }

        /* check if BCT partinfo looks correct */
        p = &pt->partinfo_bct;
        if (p->id != 2)
                return 0;
        if (memcmp(p->name, "BCT\0", 4))
                return 0;
        if (p->type != 18)
                return 0;
        if (memcmp(p->name2, "BCT\0", 4))
                return 0;

        parts = kzalloc(23*sizeof(t_partinfo), GFP_KERNEL);
        if (!parts)
                return -1;

        /* walk the partition table */
        p = pt->partinfo;
        part = parts;

        count=1;
        for (i=1; (p->id < 128) && (i<=23); i++) {
                memcpy(part->name, p->name, 4);
                part->valid = 1;
                part->start = p->start*4 - 0x800;
                part->size  = p->size*4;
                p++;
                part++;
        }

        /* hide partitions */
        s = hidden_parts_str;
        while(*s) {
                unsigned len;

                len = strcspn(s, ",: ");
                part = parts;

                for(i=1; i<=23; i++) {
                        if (part->valid) {
                                if (!strncmp(part->name, s, len) && ((len>=4) || (part->name[len]=='\0'))) {
                                        part->valid = 0;
                                        break;
                                }
                        }
                        part++;
                }
                s += len;
                s += strspn(s, ",: ");
        }

        if (*hidden_parts_str)
#ifdef BRIEF
          printk(KERN_INFO "\n");
          printk(KERN_INFO "nvtegrapart: hidden_parts = %s\n", hidden_parts_str);
#endif
        /* log partitions */
        part = parts;
        for(i=1; i<=23; i++) {
          if (part->valid)
#ifdef BRIEF
                printk(KERN_INFO "nvtegrapart: #%d [%-4.4s] start=%u size=%u\n",
                        i, part->name, part->start, part->size);
#endif
          part++;
        }

        /* finally register valid partitions */
        count = 1;
        part = parts;
        for(i=1; i<=23; i++) {
          if (part->valid)
                put_partition(state, count++, part->start, part->size);
          part++;
        }

        kfree(parts);
        kfree(pt);
        return 1;
}

static int __init nvtegra_hideparts_setup(char *options) {
        if (options)
                hidden_parts_str = options;
        return 0;
}

__setup("nvtegra_hideparts=", nvtegra_hideparts_setup);

