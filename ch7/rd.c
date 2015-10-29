#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/blkdev.h>
#include <asm/uaccess.h>

#define DEVICE_SECTOR_SIZE	512
#define DEVICE_SIZE					(4*1024*1024)
#define DEVICE_SECTOR_TOTAL	(DEVICE_SIZE/DEVICE_SECTOR_SIZE)

struct root_device_t{
	unsigned char *data;
	struct request_queue *queue;
	struct gendisk *gd
}root_device;

static unsigned char *vdisk;

static int device_make_request(struct request_queue *q, struct bio *bio){
	struct root_device_t *root_dev;

	char *hdd_data;
	char *buffer;
	struct bio_vec bvec;
	struct bvec_iter iter;
	int i;

	if(((bio->bi_iter.bi_sector * DEVICE_SECTOR_SIZE) + bio->bi_iter.bi_size) > DEVICE_SIZE){
		bi_endio((bio), -EIO);
		return 0;
	}

	root_dev = (struct root_device_t *)bio->bi_bdev->bd_disk->private_data;

	hdd_data = root_dev->data + (bio->bi_iter.bi_sector * DEVICE_SECTOR_SIZE);

	bio_for_each_segment(bvec, bio, iter){
		buffer = kmap(bvec.bv_page) + bvec.bv_offset;

		switch(bio_data_dir(bio)){
			case READA:
			case READ:
				memcpy(buffer, hdd_data, bvec.bv_len);
				break;
			case WRITE:
				memcpy(hdd_data, buffer, bvec.bv_len);
				break;
			default :
				kunmap(bvec.bv_page);
				bi_endio((bio), -EIO);
				return 0;
		}
		kunmap(bvec.bv_page);
		hdd_data += bvec.bv_len;
	}

	bi_endio((bio), -EIO);
	return 0;
}

static int device_open(struct inode *inode, struct file *filp){
	return 0;
}

static int device_release(struct inode *inode, struct file *filp){
	return 0;
}

static int device_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg){
	return -EIO;
}

static struct block_device_operations bdops = {
	.owner = THIS_MODULE,
	.open = device_open,
	.release = device_release,
	.iotcl = device_ioctl
};

int device_init(void){
	register_blkdev(253, "vrd");

	vdisk = vmalloc(DEVICE_SIZE);

	root_device.data = vdisk;
	root_device.gd = alloc_disk(1);
	root_device.queue = blk_alloc_queue(GFP_KERNEL);

	blk_queue_make_request(root_device.queue, &device_make_request);

	root_device.gd->major = 253;
	root_device.gd->fops  = &bdops;
	root_device.gd->queue = root_device.queue;
	root_device.gd->private_data = &vdisk;
	set_capacity(root_device.gd, DEVICE_SECTOR_TOTAL);

	add_disk(root_device.gd);

	return 0;
}

void device_exit(void){
	del_gendisk(root_device.gd);
	put_disk(root_device.gd);
	unregister_blkdev(253, "vrd");
	vfree(vdisk);
}
