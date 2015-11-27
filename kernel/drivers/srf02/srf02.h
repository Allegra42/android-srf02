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


#ifndef __srf02_H__
#define __srf02_H__


#define CMD_COMMAND_REG      (0x00)
#define CMD_RANGE_HIGH_BYTE  (0x02)
#define CMD_RANGE_LOW_BYTE   (0x03)
#define CMD_RESULT_IN_INCHES (0x50)
#define CMD_RESULT_IN_CM     (0x51)
#define CMD_RESULT_IN_MS     (0x52)


struct srf02_priv;
static struct i2c_board_info srf02_info;


static int srf02_i2c_probe (struct i2c_client *client, const struct i2c_device_id *id);
static int srf02_i2c_remove (struct i2c_client *client);

static int srf02_open (struct inode *inode, struct file *file);
static int srf02_release (struct inode *inode, struct file *file);

static ssize_t srf02_write (struct file *file, const char *buf, size_t length, loff_t *offset);
static ssize_t srf02_read (struct file *file, char *buf, size_t length, loff_t *ppos);

static const struct file_operations srf02_fops;

static void srf02_early_suspend (struct early_suspend *suspend);
static void srf02_later_resume (struct early_suspend *suspend);

#endif
