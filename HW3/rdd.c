/*
 *	A sample, extra-simple block driver. Updated for kernel 2.6.31.
 *
 * 	(C) 2003 Eklektix, Inc.
 *	(C) 2010 Pat Patterson <pat at superpat dot com>
 * 	Redistributable under the terms of the GNU GPL.
 *
 *	CS 444 HW 3 - Encrypted Block Device
 *	By: Kyle and Jonathan
 *	Method: Pair Programming
 *
 *	Resources:
 *	http://lwn.net/Kernel/LDD3/
 *	http://blog.superpat.com/2010/05/04/a-simple-block-driver-for-linux-kernel-2-6-31/	
 *	http://lxr.free-electrons.com/source/include/linux/crypto.h 
 *	http://lxr.free-electrons.com/source/Documentation/crypto/api-intro.txt
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h> /* printk() */
#include <linux/fs.h>     /* everything... */
#include <linux/errno.h>  /* error codes */
#include <linux/types.h>  /* size_t */
#include <linux/vmalloc.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/crypto.h>


MODULE_LICENSE("Dual BSD/GPL");
static char *Version = "1.4";

// Our note:
// Module param takes variable name, its type and permission.
// Allow arguments to be passed to your module

static int major_num = 0;
module_param(major_num, int, 0);
static int logical_block_size = 512;
module_param(logical_block_size, int, 0);
static int nsectors = 1024; /* How big the drive is */
module_param(nsectors, int, 0);

/*
 * We can tweak our hardware sector size, but the kernel talks to us
 * in terms of small sectors, always.
 */
#define KERNEL_SECTOR_SIZE 512

static char *KEY = "harambeharambeha";
static int KEYLEN = 16;
struct crypto_cipher *tfm;


/*
 * Our request queue.
 */
static struct request_queue *Queue;

/*
 * The internal representation of our device.
 */
static struct rdd_device {
	unsigned long size;
	spinlock_t lock;
	u8 *data;
	struct gendisk *gd;
} Device;

/*
 * Handle an I/O request.
 */

// Data Transfer
static void rdd_transfer(struct rdd_device *dev, sector_t sector,
		unsigned long nsect, char *buffer, int write) {
	unsigned long offset = sector * logical_block_size;
	unsigned long nbytes = nsect * logical_block_size;
	unsigned long block_size = crypto_cipher_blocksize(tfm);
	int i;
	crypto_cipher_setkey(tfm,KEY,KEYLEN);
	if ((offset + nbytes) > dev->size) {
		printk (KERN_NOTICE "rdd: Beyond-end write (%ld %ld)\n", offset, nbytes);
		return;
	}
	if (write) {
		// ***insert encrypt here.
		u8 *destination = dev->data + offset;
		u8 *source = buffer;
		for(i = 0; i < nbytes; i+=block_size)
		{	 
			crypto_cipher_encrypt_one(tfm, destination + i, source + i);
		//	memcpy(dev->data + offset, buffer, nbytes);
		}
	} else 	{
		u8 *source = dev->data + offset;
		u8 *destination = buffer;
		for(i = 0; i < nbytes; i+=block_size)
		{	 
			crypto_cipher_decrypt_one(tfm, destination + i, source + i);
		}
	//		memcpy(buffer, dev->data + offset, nbytes);
	}
}

// Specify the request function to handle entries in request queue
static void rdd_request(struct request_queue *q) {
	struct request *req;

	req = blk_fetch_request(q);
	while (req != NULL) {
		// blk_fs_request() was removed in 2.6.36 - many thanks to
		// Christian Paro for the heads up and fix...
		//if (!blk_fs_request(req)) {
		if (req == NULL || (req->cmd_type != REQ_TYPE_FS)) {
			printk (KERN_NOTICE "Skip non-CMD request\n");
			__blk_end_request_all(req, -EIO);
			continue;
		}
		rdd_transfer(&Device, blk_rq_pos(req), blk_rq_cur_sectors(req),
				req->buffer, rq_data_dir(req));
		if ( ! __blk_end_request_cur(req, 0) ) {
			req = blk_fetch_request(q);
		}
	}
}

/*
 * The HDIO_GETGEO ioctl is handled in blkdev_ioctl(), which
 * calls this. We need to implement getgeo, since we can't
 * use tools such as fdisk to partition the drive otherwise.
 */
int rdd_getgeo(struct block_device * block_device, struct hd_geometry * geo) {
	long size;

	/* We have no real geometry, of course, so make something up. */
	size = Device.size * (logical_block_size / KERNEL_SECTOR_SIZE);
	geo->cylinders = (size & ~0x3f) >> 6;
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 0;
	return 0;
}

/*
 * The device operations structure.
 */
static struct block_device_operations rdd_ops = {
		.owner  = THIS_MODULE,
		.getgeo = rdd_getgeo
};

static int __init rdd_init(void) {
	/*
	 * Set up our internal device.
	 */
	tfm = crypto_alloc_cipher("sha1",0,0);
	Device.size = nsectors * logical_block_size;
	spin_lock_init(&Device.lock);
	Device.data = vmalloc(Device.size);
	if (Device.data == NULL)
		return -ENOMEM;
	/*
	 * Get a request queue.
	 *
	 * blk_init_queue allocates and initializes a spinlock. 
	 * 
	 *
	 */
	Queue = blk_init_queue(rdd_request, &Device.lock);
	if (Queue == NULL)
		goto out;
	blk_queue_logical_block_size(Queue, logical_block_size);
	/*
	 * Get registered.
	 */
	major_num = register_blkdev(major_num, "rdd");

	/*register_blkdev takes a major number as input and the name of the new block device. 
  	If the inputted major number is 0 then a new major number will be allocated and returned.
  	If the inputted major number is not 0 then 0 will be returned on success.
  	If there was an error in registering the new block device then a negative value will be returned.
  	Major numbers represent the driver that is associated with the device.
 
 	Per documentation
	block	RAM disk
		  0 = /dev/ram0		First RAM disk
		  1 = /dev/ram1		Second RAM disk
		    ...
		250 = /dev/initrd	Initial RAM disk

		Older kernels had /dev/ramdisk (1, 1) here.
		/dev/initrd refers to a RAM disk which was preloaded
		by the boot loader; newer kernels use /dev/ram0 for
		the initrd.*/
	
	if (major_num < 0) {
		printk(KERN_WARNING "rdd: unable to get major number\n");
		goto out;
	}
	/*
	 * And the gendisk structure.
	 */
	Device.gd = alloc_disk(16);

	if (!Device.gd)
		goto out_unregister;
	Device.gd->major = major_num;
	Device.gd->first_minor = 0;
	Device.gd->fops = &rdd_ops;
	Device.gd->private_data = &Device;
	strcpy(Device.gd->disk_name, "rdd0");
	set_capacity(Device.gd, nsectors);
	Device.gd->queue = Queue;
	add_disk(Device.gd);

	return 0;

out_unregister:
	unregister_blkdev(major_num, "rdd");
out:
	vfree(Device.data);
	return -ENOMEM;
}

// Cleanup
static void __exit rdd_exit(void)
{
	crypto_free_cipher(tfm);
	del_gendisk(Device.gd);
	put_disk(Device.gd);
	unregister_blkdev(major_num, "rdd");
	blk_cleanup_queue(Queue);
	vfree(Device.data);
}

module_init(rdd_init);
module_exit(rdd_exit);
