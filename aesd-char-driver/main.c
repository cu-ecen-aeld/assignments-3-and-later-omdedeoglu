/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include <linux/slab.h>      // kmalloc, kfree
#include <linux/uaccess.h>   // copy_from_user, copy_to_user
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("omdedeoglu"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    PDEBUG("open dev=%p", dev);
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev = filp->private_data;
    PDEBUG("read %zu bytes with offset %lld dev=%p", count, *f_pos, dev);
    struct aesd_buffer_entry *entry;
    size_t entry_offset = 0;
    size_t bytes_available;
    size_t bytes_to_copy;

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset);
    if (!entry) {
        PDEBUG("Entry could not found! f_pos=%lld", *f_pos);
        retval = 0; /* EOF */
        goto out;
    }

    bytes_available = entry->size - entry_offset;
    bytes_to_copy = min(count, bytes_available);
    PDEBUG("bytes_to_copy=%d", bytes_to_copy);

    if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_copy)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

out:
    mutex_unlock(&dev->lock);    
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
    struct aesd_dev *dev = filp->private_data;
    PDEBUG("write %zu bytes with offset %lld dev=%p",count,*f_pos, dev);
    struct aesd_buffer_entry new_entry;
    retval = count;
    char *new_pending;
    char *newline_ptr;
    if(count == 0){
        return 0;
    }

    if (mutex_lock_interruptible(&dev->lock)) {
        return -ERESTARTSYS;
    }

    new_pending = krealloc(dev->pending_write_buffer, dev->pending_write_size + count, GFP_KERNEL);
    if (!new_pending) {
        retval = -ENOMEM;
        goto out;
    }
    dev->pending_write_buffer = new_pending;

    if (copy_from_user(dev->pending_write_buffer + dev->pending_write_size, buf, count)) {
        retval = -EFAULT;
        goto out;
    }
    dev->pending_write_size += count;

    newline_ptr = memchr(dev->pending_write_buffer, '\n', dev->pending_write_size);

    if (!newline_ptr) {
        PDEBUG("partial write stored, pending size=%zu", dev->pending_write_size);
        retval = count;
        goto out;
    }

    new_entry.buffptr = dev->pending_write_buffer;
    new_entry.size = dev->pending_write_size;

    /*
     * If the circular buffer is full, adding a new entry will overwrite
     * the entry at in_offs. Free that old command first.
     */
    if (dev->buffer.full) {
        kfree(dev->buffer.entry[dev->buffer.in_offs].buffptr);
        dev->buffer.entry[dev->buffer.in_offs].buffptr = NULL;
        dev->buffer.entry[dev->buffer.in_offs].size = 0;
    }

    aesd_circular_buffer_add_entry(&dev->buffer, &new_entry);

    PDEBUG("complete command stored, size=%zu", new_entry.size);

    /*
     * Ownership moved to circular buffer.
     * Do not kfree(dev->pending_write_buffer).
     */
    dev->pending_write_buffer = NULL;
    dev->pending_write_size = 0;

out:    
    mutex_unlock(&dev->lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    aesd_circular_buffer_init(&aesd_device.buffer);
    aesd_device.pending_write_buffer = NULL;
    aesd_device.pending_write_size = 0;

    mutex_init(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

static void aesd_cleanup_device(struct aesd_dev *dev)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;

    mutex_lock(&dev->lock);

    kfree(dev->pending_write_buffer);
    dev->pending_write_buffer = NULL;
    dev->pending_write_size = 0;

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->buffer, index) {
        kfree(entry->buffptr);
        entry->buffptr = NULL;
        entry->size = 0;
    }

    mutex_unlock(&dev->lock);
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    aesd_cleanup_device(&aesd_device);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
