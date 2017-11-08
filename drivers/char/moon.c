#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/fs.h>       /* everything... */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <linux/cdev.h>
#include <asm/system.h>     /* cli(), *_flags */
#include <asm/uaccess.h>    /* copy_*_user */

#include <linux/slab.h>
#include <linux/device.h>


#include <linux/semaphore.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/div64.h>


struct si32176_dev{
    int column;
    int page;

    struct cdev cdev;
};

int slic_major = 0;
int slic_minor = 0;

static struct class *my_class;
struct si32176_dev *slic_device;

char test_data[]="yaomoon";



/*
int slic_dri_open(struct inode *inode, struct file *filp)
{   
    struct si32176_dev *dev; 

    dev = container_of(inode->i_cdev, struct si32176_dev, cdev);
    filp->private_data = dev;

    return 0;
}
*/


static int slic_dri_write(
    struct file *filp,
    const char __user *buf,
    size_t count,
    loff_t *ppos)
{
    unsigned long err;
    char* kbuf;
    
    kbuf = kmalloc(count+1, GFP_KERNEL);    
    err = copy_from_user(kbuf, buf, count);

    if(strncmp(buf,"enable",6) == 0)
    {
        printk(KERN_WARNING "yaomoon: echo enableeeeeeeeeeeeeeee\n"); 
    }
    else
    {
        printk(KERN_WARNING "yaomoon: echo disableeeeeeeeeeeeeeee\n"); 
    }
        
    kfree(kbuf);

    return err ? -EINVAL : count;
}

#define REG_GET 0
#define REG_SET 1


struct file_operations si32176_slic_fops = {
    .owner = THIS_MODULE,
    //.open = slic_dri_open,
    .write = slic_dri_write,
};

static void slic_dri_setup_cdev(struct si32176_dev *dev)
{
    int err; 
    int devno = MKDEV(slic_major, slic_minor);

    cdev_init(&dev->cdev, &si32176_slic_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add (&dev->cdev, devno, 1);
    /* Fail gracefully if need be */
    if (err)
        printk(KERN_NOTICE "Error %d adding si32176", err);
}

static int __init slic_dri_init(void)
{
    int result;
    dev_t dev = 0;

    printk(KERN_WARNING "si32176: yaomoon debug\n");


    if (slic_major) {
        dev = MKDEV(slic_major, slic_minor);
        result = register_chrdev_region(dev, 1, "wifiled");
    } else {
        result = alloc_chrdev_region(&dev, slic_minor, 1,
                "wifiled");
        slic_major = MAJOR(dev);
    }
    if (result < 0) {
        printk(KERN_WARNING "si32176 slic: can't get major %d\n", slic_major);
        return result;
    }

    slic_device = kmalloc(sizeof(struct si32176_dev), GFP_KERNEL);
    if (!slic_device) {
        result = -ENOMEM;
        goto fail;  /* Make this more graceful */
    }
    memset(slic_device, 0, sizeof(struct si32176_dev));

    slic_dri_setup_cdev(slic_device);

    my_class = class_create(THIS_MODULE,"si32176_slic");
    if(IS_ERR(my_class))
    {
        printk(KERN_WARNING "si32176 slic:failed in creating class.\n");
        return -1;
    }
    device_create(my_class,NULL,MKDEV(slic_major, 0), NULL,"wifiled");

    return 0; /* succeed */

fail:
    return result;
}

static void __exit slic_module_cleanup(void)
{
    dev_t devno = MKDEV(slic_major, slic_minor);

    device_destroy(my_class, devno);
    class_destroy(my_class);

    /* Get rid of our char dev entries */
    if (slic_device) {
        cdev_del(&slic_device[0].cdev);
        kfree(slic_device);
    }

    /* cleanup_module is never called if registering failed */
    unregister_chrdev_region(devno, 1);

    printk("Good-bye, si32176 slic module was removed!\n");
}

module_init(slic_dri_init);
module_exit(slic_module_cleanup);

MODULE_LICENSE("GPL");
