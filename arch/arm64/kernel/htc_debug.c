/*
 * Copyright (C) 2010 HTC, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/seq_file.h>

#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/platform_device.h>

mm_segment_t oldfs;

#define PROCNAME "driver/hdf"
#define FLAG_LEN 64
#define HTC_DEBUG_HDF_DEV_NAME "htc,htc_debug_hdf"

static struct of_device_id htc_debug_hdf_match_table[] = {
    { .compatible = HTC_DEBUG_HDF_DEV_NAME },
    {}
};


#if 0
#define SECMSG(s...) pr_info("[SECURITY] "s)

#else
#define SECMSG(s...) do{} while(0)
#endif

static char htc_debug_flag[FLAG_LEN+1]={0};
extern int get_partition_num_by_name(char *name);
static int offset=628;
static int first_read=1;

static int htc_debug_read(struct seq_file *m, void *v)
{
    char filename[32] = "";
    char RfMisc[FLAG_LEN+3]={0};
    struct file *filp = NULL;
    ssize_t nread;
    int pnum;

    if(first_read){
        pnum = get_partition_num_by_name("misc");

        if (pnum < 0) {
            printk(KERN_ERR"unknown partition number for misc partition\n");
            return 0;
        }

        snprintf(filename, 32, "/dev/block/mmcblk0p%d", pnum);

        filp = filp_open(filename, O_RDWR, 0);
        if (IS_ERR(filp)) {
            printk(KERN_ERR"unable to open file: %s\n", filename);
            return PTR_ERR(filp);
        }

        SECMSG("%s: offset :%d\n", __func__, offset);
        filp->f_pos = offset;

        nread = kernel_read(filp, filp->f_pos, RfMisc, FLAG_LEN+2);

        memset(htc_debug_flag,0,FLAG_LEN+1);
        memcpy(htc_debug_flag,RfMisc+2,FLAG_LEN);

        SECMSG("%s: RfMisc        :%s (%zd)\n", __func__,RfMisc, nread);
        SECMSG("%s: htc_debug_flag:%s \n", __func__, htc_debug_flag);

        seq_printf(m, "0X%s\n",htc_debug_flag);

        if (filp)
            filp_close(filp, NULL);

        first_read = 0;
    }else
        seq_printf(m, "0X%s\n",htc_debug_flag);

    return 0;
}

static ssize_t htc_debug_write(struct file *file, const char __user *buffer,
                        size_t count, loff_t *ppos)
{
    char buf[FLAG_LEN+3];
    char filename[32] = "";
    struct file *filp = NULL;
    ssize_t nread;
    int pnum;

    SECMSG("%s called (count:%d)\n", __func__, (int)count);

    if (count != sizeof(buf)){
        printk(KERN_ERR"count != sizeof(buf)\n");
        return -EFAULT;
    }

    if (copy_from_user(buf, buffer, count))
        return -EFAULT;

    memset(htc_debug_flag,0,FLAG_LEN+1);
    memcpy(htc_debug_flag,buf+2,FLAG_LEN);

    SECMSG("Receive :%s\n",buf);
    SECMSG("Flag    :%s\n",htc_debug_flag);

    pnum = get_partition_num_by_name("misc");

    if (pnum < 0) {
        printk(KERN_ERR"unknown partition number for misc partition\n");
        return 0;
    }

    snprintf(filename, 32, "/dev/block/mmcblk0p%d", pnum);

    filp = filp_open(filename, O_RDWR, 0);
    if (IS_ERR(filp)) {
        printk(KERN_ERR"unable to open file: %s\n", filename);
        return PTR_ERR(filp);
    }

    SECMSG("%s: offset :%d\n", __func__, offset);
    filp->f_pos = offset;
    nread = kernel_write(filp, buf, FLAG_LEN+2, filp->f_pos);
    SECMSG("%s:wrire buf: %s (%zd)\n", __func__, buf, nread);

    if (filp)
        filp_close(filp, NULL);

    return count;
}

static int htc_debug_open(struct inode *inode, struct file *file)
{
    return single_open(file, htc_debug_read, NULL);
}

static const struct file_operations htc_debug_fops = {
    .owner      = THIS_MODULE,
    .open       = htc_debug_open,
    .write      = htc_debug_write,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};

static int htc_debug_parse_tbl(struct device *dev, char *prop)
{
    int ret, prop_len;
    char *data = NULL;

    if (!of_find_property(dev->of_node, prop, &prop_len))
        return -EINVAL;

    pr_info("%s - length: %d\n", __func__, prop_len);
    if(prop_len < FLAG_LEN)
        return -EINVAL;

    ret = of_property_read_string(dev->of_node, prop, (const char **)&data);
    pr_info("%s - loglevel: %s\n", __func__, data);

    memset(htc_debug_flag, 0, (FLAG_LEN + 1));
    memcpy(htc_debug_flag, data, FLAG_LEN);

    
    first_read = 0;
    return prop_len;
}

static int htc_debug_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    pr_info("%s\n", __func__);

    htc_debug_parse_tbl(dev, "htc,loglevel");
    return 0;
}

static struct platform_driver htc_debug_driver =
{
    .probe = htc_debug_probe,
    .driver = {
        .name = HTC_DEBUG_HDF_DEV_NAME,
        .owner = THIS_MODULE,
        .of_match_table = htc_debug_hdf_match_table,
    },
};

static int __init sysinfo_proc_init(void)
{
    struct proc_dir_entry *entry = NULL;

    pr_info("%s: Init HTC Debug Flag proc interface.\r\n", __func__);

    
    entry = proc_create_data(PROCNAME, 0660, NULL, &htc_debug_fops, NULL);
    if (entry == NULL) {
        printk(KERN_ERR "%s: unable to create /proc%s entry\n", __func__,PROCNAME);
        return -ENOMEM;
    }

    platform_driver_register(&htc_debug_driver);
    return 0;
}

module_init(sysinfo_proc_init);
MODULE_AUTHOR("Medad Chang <medad_chang@htc.com>");
MODULE_DESCRIPTION("HTC Debug Interface");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
