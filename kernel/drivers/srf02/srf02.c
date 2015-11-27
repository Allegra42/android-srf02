/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2015 Anna-Lena Marx

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */


#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/input.h> //needed for /dev/input/event
#include <linux/earlysuspend.h>  //needed for suspend

#include "srf02.h"



MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver for range sensor / proximity sensor srf02");
MODULE_AUTHOR("Anna-Lena Marx");



#define DEVICE_NAME "srf02"
#define BUFFER_SIZE 64

/**
 * New way to initalize spinlocks
 */
static DEFINE_SPINLOCK (mylock);


static struct class *srf02_class = NULL;

static struct i2c_client *srf02_client = NULL;

struct srf02_priv {
	struct i2c_client *client;
	// android suspend
#ifdef CONFIG_EARLYSUSPEND
	struct early_suspend es_handler;
#endif
};


/**
 * For getting events in /dev/input/event*
 */
static struct input_dev *srf02_input_dev;

/**
 * Important to use right adress of sensor here, if not, the srf02_i2c_probe() function will not be called
 */
static const struct i2c_device_id srf02_id [] = {
		{"srf02", 0x70},
		{},
};

static dev_t dev_num;
static int major_number;
struct cdev *srf02dev;
static int i_device_open = 0;
static char command_buffer [BUFFER_SIZE];
static int index_command_buffer;
static struct device *dev_static;

MODULE_DEVICE_TABLE (i2c, srf02_id);

static struct i2c_driver srf02_i2c_driver = {

		.probe = srf02_i2c_probe,
		.remove = __devexit_p (srf02_i2c_remove),
		.id_table = srf02_id,
		.driver = {
				.name = "srf02",
				.owner = THIS_MODULE,
		},
};


s32 value_nonstop = -1;
int active = 0;


/**
 * Initalising workqueue for cyclic measurement if enabled
 */
static void workq_fn (struct delayed_work *work);

static struct workqueue_struct *my_wq;
typedef struct {
	struct delayed_work my_work;
}my_work_t;
my_work_t *work;


/**
 * Function is called cyclic by kworker. Store measured value in value_nonstop if active.
 */
void workq_fn (struct delayed_work *work) {
// Work Queue seens hating spinlocks


	int ret = 0;
	s32 i2cRet = 0;
	int value_reg1 = 0;
	int value_reg2 = 0;

	//atomic_long_set(&(workq->data),10);
	//printk (KERN_INFO "srf02 - in workq-function \n");
	//printk (KERN_INFO "srf02 - doing a cyclic measurement \n");

	struct i2c_client *client = to_i2c_client(dev_static);
	//Starting measurement in cm
	//write to command register that result shall be in cm
	i2cRet = i2c_smbus_write_byte_data (client, CMD_COMMAND_REG, CMD_RESULT_IN_CM);

	msleep(100);

	//Reading result
	value_reg1 = i2c_smbus_read_byte_data (client, CMD_RANGE_HIGH_BYTE);
	value_reg2 = i2c_smbus_read_byte_data (client, CMD_RANGE_LOW_BYTE);

	printk (KERN_INFO "srf02 - value is : %d \n", ((value_reg1 * 256) + value_reg2));
	value_nonstop = (value_reg1 * 256) + value_reg2;
	input_event(srf02_input_dev, EV_ABS, ABS_DISTANCE, value_nonstop);
	input_sync(srf02_input_dev);


	ret = queue_delayed_work(my_wq, (struct delayed_work *)work, msecs_to_jiffies(100));

}



/**
 * Show actual value in value_nonstop to calling user, returns -1 if disabled
 */
static ssize_t srf02_get_values_cyclic (struct device *dev, struct device_attribute *attr, char *buf) {
	// write here value to sysfs if it is asked for -> value is in a variable which is updated nonstop

	if (value_nonstop < 0) {
		//printk (KERN_INFO "srf02 - nonstop measurement seems to be disabled \n");
	}
	else {
		printk (KERN_INFO "srf02 - value is : %d (with work queue) \n", value_nonstop);
	}

	return sprintf (buf, "%d \n", value_nonstop);
}

/**
 * Writing 1 in sysfs "value_now" enabling cyclic measurement, 0 disabling
 */
static ssize_t srf02_store_values_cyclic (struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {

	unsigned long value;
	int ret;

	printk (KERN_INFO "srf02 - 1 for enabling nonstop measurement, 0 for disabling \n");

	value = simple_strtoul (buf, NULL, 10);

	//enable
	if (value > 0) {
		dev_static = dev; //to use dev in ueue function?

		my_wq = create_workqueue("my_workqueue");
		if (my_wq) {
			work = (my_work_t *) kmalloc (sizeof (my_work_t), GFP_KERNEL);
			if (work) {
				INIT_DELAYED_WORK ((struct delayed_work *) work, workq_fn);
				ret = queue_delayed_work(my_wq, (struct delayed_work *) work, msecs_to_jiffies(100));
			}
		}
		value_nonstop = 0;
	}

	if (value == 0) {
		printk (KERN_INFO "srf02 - disabling after flush_wq \n");

		cancel_delayed_work((struct delayed_work *)work);

		value_nonstop = -1; // for disabling -
	}

	return size;
}

//register in sysfs
static DEVICE_ATTR (value_now, 0644, srf02_get_values_cyclic, srf02_store_values_cyclic);


/**
 * Starting measurement, writing value in sysfs "srf02value" and give it back to userspace
 */
static ssize_t srf02_get_value (struct device *dev, struct device_attribute *attr, char *buf) {

	//printk (KERN_INFO "srf02 - try to read value \n");

	s32 i2cRet;
	int value_reg1 = 0;
	int value_reg2 = 0;
	int ret_lock = 0;

	ret_lock = spin_trylock(&mylock);

	//Spinlock acquired
	if (ret_lock) {
		//printk (KERN_INFO "srf02 - spinlock acquired, start measurement \n");
		struct i2c_client *client = to_i2c_client(dev);
		//Starting measurement in cm
		// write to command register that measurement shall be in cm
		i2cRet = i2c_smbus_write_byte_data (client, CMD_COMMAND_REG, CMD_RESULT_IN_CM);

		//delete spinlock
		spin_unlock (&mylock);

		if (i2cRet < 0) {
			printk (KERN_INFO "srf02 - failed setting mode \n");
			return 0;
		}
		//wait for result
		msleep(100);

		//Reading result
		value_reg1 = i2c_smbus_read_byte_data (client, CMD_RANGE_HIGH_BYTE);
		value_reg2 = i2c_smbus_read_byte_data (client, CMD_RANGE_LOW_BYTE);

		printk (KERN_INFO "srf02 - value is : %d \n", ((value_reg1 * 256) + value_reg2));
		return sprintf (buf, "%d \n", ((value_reg1 * 256) + value_reg2));
	}
	//can not get spinlock
	else {
		printk (KERN_INFO "srf02 - could not hold spinlock, measurement failed \n");
		return 0;
	}
}

/**
 * For writing a value to srf02, just to have it, shall not be used in my plan
 */
static ssize_t srf02_store_value (struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
	
	struct i2c_client *client = to_i2c_client (dev);
	uint8_t value;
	struct srf02_priv *srf02_p = i2c_get_clientdata (client);

	s32 value_reg1;
	s32 value_reg2;
	s32 i2cRet;
	// (to read a value from userspace)
	value = simple_strtoul (buf, NULL, 10);

	//Reading result from i2c bus
	value_reg1 = i2c_smbus_read_byte_data (client, CMD_RANGE_HIGH_BYTE);
	value_reg2 = i2c_smbus_read_byte_data (client, CMD_RANGE_LOW_BYTE);

	i2cRet = (value_reg1 * 256) + value_reg2;

	//printk (KERN_INFO "srf02 - you try to write : %d \n", value);
	//printk (KERN_INFO "srf02 - value is : %d \n", i2cRet);

	return size;
}

/**
 * Set srf02value as attribute with given rights and methods for reading and writing the attribute file
 */
static DEVICE_ATTR (srf02value, 0644, srf02_get_value, srf02_store_value);

static const struct attribute *srf02_attrs[] = {
		&dev_attr_value_now.attr,
		&dev_attr_srf02value.attr,
		NULL,
};

static const struct attribute_group srf02_attr_group = {
		.attrs = srf02_attrs,
};

/**
 * Init Method for srf02 module
 */
static int __init srf02_init (void) {
	int ret;
	struct device *srf02_device;

	ret = alloc_chrdev_region (&dev_num, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		printk (KERN_INFO "srf02 - failed to allocate major number \n ");
		return ret;
	}
	major_number = MAJOR(dev_num);

	//printk (KERN_INFO "srf02 - initialising \n");

	// Creating class in sysfs
	srf02_class = class_create (THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(srf02_class)) {
		ret = PTR_ERR(srf02_class);
		goto exit_failed_class_create;
	}
	//printk (KERN_INFO "srf02 - class in sysfs created \n");

	srf02dev = cdev_alloc();
	if (srf02dev == NULL) {
		ret = -1;
		goto exit_failed_cdev_alloc;
	}

	srf02dev->ops = &srf02_fops;
	srf02dev->owner = THIS_MODULE;

	ret = cdev_add (srf02dev, dev_num, 1);
	if (ret < 0) {
		printk (KERN_INFO "srf02 - adding device to kernel failed \n");
		goto exit_failed_cdev_add;
	}
	//printk (KERN_INFO "srf02 - adding device to kernel successful \n ");

	srf02_device = device_create (srf02_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR (srf02_device)) {
		printk (KERN_INFO "srf02 - device in sysfs failed \n");
		ret = PTR_ERR(srf02_device);
		goto exit_failed_device_create;
	}
	//printk (KERN_INFO "srf02 - device in sysfs created \n");

	ret = i2c_add_driver (&srf02_i2c_driver);
	if (ret != 0) {
		printk (KERN_INFO "srf02 - i2c_add_driver failed \n");
		goto exit_failed_i2c_add_driver;
	}
	printk (KERN_INFO "srf02 - i2c_add_driver successful \n");

	//for input events
	srf02_input_dev = input_allocate_device();
	if (!srf02_input_dev) {
		printk (KERN_INFO "srf02 - can not allocate memory for input-device \n");
		ret = -ENOMEM;
	}
	srf02_input_dev->evbit[0] = BIT_MASK(EV_ABS);
	srf02_input_dev->name = "SRF02 input event module";
	input_set_abs_params(srf02_input_dev, ABS_DISTANCE, 15, 700, 1, 0);

	ret = input_register_device(srf02_input_dev);

	if (ret) {
		printk (KERN_INFO "srf02 - failed to register input device \n");
		goto exit_free_dev;
	}


	return 0;


	exit_free_dev:
		input_free_device(srf02_input_dev);

	exit_failed_i2c_add_driver:

	exit_failed_device_create:
		if (srf02dev) {
			cdev_del (srf02dev);
		}

	exit_failed_cdev_add:

	exit_failed_cdev_alloc:
		if (srf02_class) {
			class_destroy(srf02_class);
		}

	exit_failed_class_create:
		unregister_chrdev_region(dev_num, 1);
		return ret;
}

/**
 * Exit Method for srf02
 */
static void __exit srf02_exit (void) {

	input_unregister_device(srf02_input_dev);

	if (srf02dev) {
		cdev_del (srf02dev);
	}

	if (srf02_class) {
		class_destroy (srf02_class);
	}
	//printk (KERN_INFO "srf02 - class in sysfs destroyed  \n");

	unregister_chrdev_region(dev_num, 1);

	i2c_del_driver(&srf02_i2c_driver);
}

module_init(srf02_init);
module_exit(srf02_exit);


/**
 * Modprobe function for srf02 - create srf02value file!
 */
static int srf02_i2c_probe (struct i2c_client *client, const struct i2c_device_id *id) {
	struct srf02_priv *srf02_p;
	int ret = 0;

	//printk (KERN_INFO "srf02 - entered probe \n ");

	srf02_p = kzalloc (sizeof (struct srf02_priv), GFP_KERNEL);
	if(!srf02_p) {
		return -ENOMEM;
	}

	srf02_p->client = client;

	i2c_set_clientdata(client, srf02_p);
	srf02_p = i2c_get_clientdata(client);

	//printk (KERN_INFO "Client address %d \n", client->addr);
	//printk (KERN_INFO "Client name %s \n", client->name);
	//printk (KERN_INFO "srf02->client->addr %d \n", srf02_p->client->addr);


	srf02_client = srf02_p->client;

	//create srf02value entry
	ret = sysfs_create_group (&client -> dev.kobj, &srf02_attr_group);
	if (ret) {
		printk (KERN_INFO "srf02 - init sysfs failed \n ");
		goto exit_failed_init_sysfs;
	}
	//printk (KERN_INFO "srf02 - init sysfs probe function success \n");

#ifdef CONFIG_EARLYSUSPEND
	srf02_p->es_handler.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	srf02_p->es_handler.suspend = srf02_early_suspend;
	srf02_p->es_handler.resume = srf02_later_resume;
	srf02_p->es_handler.data = (void *)client;
	register_early_suspend(&srf02_p->es_handler);

#endif

	return 0;

	exit_failed_init_sysfs:
		kfree(srf02_p);
		srf02_client = NULL;
		return -1;
}

/**
 * Remove module
 */
static int srf02_i2c_remove (struct i2c_client *client) {
	struct srf02_priv *srf02_p = i2c_get_clientdata (client);

#ifdef CONFIG_EARLYSUSPEND
	unregister_early_suspend(&srf02_p->es_handler);
#endif

	sysfs_remove_group(&client->dev.kobj, &srf02_attr_group);
	//printk (KERN_INFO "srf02 - removed sysfs group \n");
	kfree (srf02_p);
	return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND

static void srf02_early_suspend (struct early_suspend *suspend) {
	struct srf02_priv *srf02_p;
	s32 i2cRet;

	//printk (KERN_INFO "srf02 - early suspend started \n");

	if (suspend->data) {
		srf02_p = i2c_get_clientdata((struct i2c_client *) suspend->data);
		// save all important things here for starting suspend mode
		// stop queue
		cancel_delayed_work ((struct delayed_work *)work);
	}
}

static void srf02_later_resume (struct early_suspend *suspend) {
	struct srf02_priv *srf02_p;
	s32 i2cRet;

	if (suspend->data) {
		srf02_p = i2c_get_clientdata ((struct i2c_client *) suspend->data);
		// wake up all important things, restore saved values...
		// write workfunction again to the queue
		my_wq = create_workqueue("my_workqueue");
       		if (my_wq) {
        		work = (my_work_t *) kmalloc (sizeof (my_work_t), GFP_KERNEL);
               		if (work) {
                               	INIT_DELAYED_WORK ((struct delayed_work *) work, workq_fn);
                                queue_delayed_work(my_wq, (struct delayed_work *) work,	msecs_to_jiffies(100));
                       	}
                 }
	}
}

#else

static void srf02_early_suspend (struct early_suspend *suspend) {
	// nothing to do
}

static void srf02_later_resume (struct early_suspend *suspend) {
	// nothing to do
}

#endif


/**
 * Standard functions to access the driver
 */
static const struct file_operations srf02_fops = {
		.owner = THIS_MODULE,
		.open = srf02_open,
		.release = srf02_release,
		.write = srf02_write,
		.read = srf02_read,
};


static int srf02_open (struct inode *inode, struct file *file) {
	//printk (KERN_INFO "srf02 - try to open file \n");
	if (i_device_open) {
		return -EBUSY;
	}
	i_device_open++;
	return 0;
}

static int srf02_release (struct inode *inode, struct file *file) {
	//printk (KERN_INFO "srf02 - try to release file \n");
	i_device_open--;
	return 0;
}

static ssize_t srf02_write (struct file *file, const char *buf, size_t length, loff_t *offset) {
	//printk (KERN_INFO "srf02 - try to write file - i do not like if you try to change measured values \n");

	struct srf02_priv *srf02_p;
	struct i2c_client *client;
	s32 i2cRet;

	int bytes_written;
	int max_bytes = 2;
	uint8_t buffer[2];

	if (srf02_client) {
		srf02_p = i2c_get_clientdata (srf02_client);
		if (srf02_p) {
			client = srf02_p->client;
		}
	}

	if (client == NULL) {
		printk (KERN_INFO "srf02 - write () - client failed \n");
		return -1;
	}

	//printk (KERN_INFO "Write Function: Client address %d\n", client->addr);
	//printk (KERN_INFO "Write Function: Client name %s\n", client->name);
	//printk (KERN_INFO "Write Function: Client flags %d\n", client->flags);
	//printk (KERN_INFO "Write Function: Adapter Nummer %d\n", client->adapter->nr);
	//printk (KERN_INFO "Write Function: Adapter Name %s\n", client->adapter->name);

	if (length == max_bytes) {
		bytes_written = copy_from_user(buffer, buf, 2);
		//printk (KERN_INFO "srf02 - write () - in progress \n");

		command_buffer [index_command_buffer] = buffer [0];
		command_buffer [index_command_buffer +1] = buffer [1];

		if (index_command_buffer == (BUFFER_SIZE - 2)) {
			index_command_buffer = 0;
		}
		else {
			index_command_buffer = index_command_buffer + 2;
		}

		i2cRet = i2c_smbus_write_byte_data (client, buffer [0], buffer [1]);
		//printk (KERN_INFO "srf02 - write () - i2c_smbus_write_byte_data : %d \n", i2cRet);

		return i2cRet;
	}
	else {
		printk (KERN_INFO "srf02 - write () - length doesnt fit \n");
		return -2;
	}
}

static ssize_t srf02_read (struct file *file, char *buf, size_t length, loff_t *ppos) {
	//printk (KERN_INFO "srf02 - try to read file \n");

	int bytes_to_read;
	int bytes_read_first, bytes_read_second;
	int read_length_second_copy;
	int local_index_command_buffer;

	local_index_command_buffer = index_command_buffer;

	if (length > BUFFER_SIZE) {
		bytes_to_read = BUFFER_SIZE;
	}
	else {
		bytes_to_read = length;
	}

	if ((local_index_command_buffer - bytes_to_read) < 0) {
		//printk (KERN_INFO "srf02 - read () - need two copies \n");

		bytes_read_first = copy_to_user (buf, command_buffer, local_index_command_buffer);
		//printk (KERN_INFO "srf02 - read () - first copy in progress	\n");

		read_length_second_copy = (bytes_to_read - local_index_command_buffer);

		bytes_read_second = copy_to_user ((buf + local_index_command_buffer), (command_buffer + BUFFER_SIZE) - read_length_second_copy, read_length_second_copy);
		//printk (KERN_INFO "srf02 - read () - second copy in progress \n");

		return (bytes_read_first + bytes_read_second);
	}
	else {
		//printk (KERN_INFO "srf02 - read () - just one copy needed \n");
		bytes_read_first = copy_to_user (buf, command_buffer, bytes_to_read);
		//printk (KERN_INFO "srf02 - read () - ? \n");

		return bytes_read_first;
	}
}


