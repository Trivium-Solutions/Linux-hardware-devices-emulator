#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/printk.h>

#include "vcpsim.h"

#define I2C_CHIP_SIZE	256

#define I2C_FUNCTIONALITY \
	(I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE | \
	 I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA | \
	 I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BLOCK_DATA)

struct vs_chip {
	u8 pos;
	u8 dat[I2C_CHIP_SIZE];
};

struct vs_dev_priv {
	bool in_use;
	struct vs_dev * vsdev;
	struct i2c_adapter adapter;
	long index;
	u8 resp[VS_MAX_RESPONSE];
	u8 * resp_ptr;
	size_t resp_size;
	struct vs_chip chip;
	struct list_head devices;
};

#define to_priv(adap) container_of(adap, struct vs_dev_priv, adapter)

static struct list_head devices;

#define NODEV_ERROR ENODEV

static int do_master_xfer(struct i2c_adapter * adap, struct i2c_msg * msgs, int num)
{
	struct vs_dev_priv * dev = to_priv(adap);
	int err = num;
	int i;

	for (i = 0; i < num; i++) {
		struct i2c_msg * m = &msgs[i];

		if (m->flags & I2C_M_RD) {
			/* reading */
			if (dev->resp_size) {
				size_t sz = dev->resp_size > m->len ?
					m->len : dev->resp_size;

				memcpy(m->buf, dev->resp_ptr, sz);

				/* VcpSdkCmd may read in chunks of sizes
				 * less than resp_size. */
				dev->resp_size -= sz;
				dev->resp_ptr += sz;

				vs_log_response(VS_I2C, dev->index, m->buf, m->len);
			}
			else {
				dev_dbg_ratelimited(&adap->dev, "attempt to read %d byte(s)\n", m->len);
				m->len = 0;
				err = -EINVAL;
			}
		}
		else {
			/* writing */
			struct vs_pair * pair;

			pair = find_response(dev->vsdev, m->buf, m->len);

			if (dev->resp_size)
				dev_err_ratelimited(&adap->dev, "new request arrived "
					"while previous one is pending; "
					"possible data loss\n");

			if (pair) {
				memcpy(dev->resp, pair->resp, pair->resp_size);
				dev->resp_size = pair->resp_size;
				dev->resp_ptr = dev->resp;
			}
			else
				dev->resp_size = 0;

			vs_log_request(VS_I2C, dev->index, m->buf, m->len, !!pair);
		}
	}

	return err;
}

static int vcpi2c_master_xfer(struct i2c_adapter * adap, struct i2c_msg * msgs, int num)
{
	struct vs_dev_priv * dev = to_priv(adap);
	int err = -NODEV_ERROR;

	/* If the transfer happens while the device is being removed,
	 * and we use lock_iface_devs(), we may get into a deadlock
	 * (somewhere in i2c_del_adapter()). In order to avoid this,
	 * we mark the device as unused before removal and handle
	 * the transfer only if the device is in use. */
	if (dev->in_use) {
		lock_iface_devs(VS_I2C);

		err = do_master_xfer(adap, msgs, num);

		unlock_iface_devs(VS_I2C);
	}

	return err;
}

static int do_smbus_xfer(struct i2c_adapter * adap, u16 addr, unsigned short flags,
	char read_write, u8 command, int size, union i2c_smbus_data *data)
{
	struct vs_dev_priv * dev = to_priv(adap);
	struct vs_chip * chip = &dev->chip;
	s32 err = 0;
	int i, len;

	switch (size) {

		case I2C_SMBUS_QUICK:

			dev_dbg(&adap->dev, "I2C_SMBUS_QUICK: addr=0x%02x, %c\n", addr,
					read_write == I2C_SMBUS_WRITE ? 'W' : 'R');
			break;

		case I2C_SMBUS_BYTE:

			if (read_write == I2C_SMBUS_WRITE) {
				chip->pos = command;
				dev_dbg(&adap->dev,
					"I2C_SMBUS_BYTE: addr=0x%02x, set pos 0x%02x\n",
					addr, command);
			}
			else {
				data->byte = chip->dat[chip->pos];
				dev_dbg(&adap->dev,
					"I2C_SMBUS_BYTE: addr=0x%02x, read 0x%02x at 0x%02x\n",
					addr, data->byte, chip->pos);
				chip->pos++;
			}

			break;

		case I2C_SMBUS_BYTE_DATA:

			if (read_write == I2C_SMBUS_WRITE) {
				chip->dat[command] = data->byte;

				dev_dbg(&adap->dev,
					"I2C_SMBUS_BYTE_DATA: addr=0x%02x, wrote 0x%02x at 0x%02x\n",
					addr, data->byte, command);

			}
			else {
				data->byte = chip->dat[command];

				dev_dbg(&adap->dev,
					"I2C_SMBUS_BYTE_DATA: addr=0x%02x, read  0x%02x at 0x%02x\n",
					addr, data->byte, command);
			}

			chip->pos = command + 1;

			break;

		case I2C_SMBUS_WORD_DATA:

			if (read_write == I2C_SMBUS_WRITE) {
				/* XXX handle possible overrun */
				if (command < 0xFF)
					*(u16*)&chip->dat[command] = data->word;
				else
					chip->dat[command] = (u8)data->word;

				dev_dbg(&adap->dev,
					"I2C_SMBUS_WORD_DATA: addr=0x%02x, wrote 0x%04x at 0x%02x\n",
					addr, data->word, command);
			}
			else {
				/* XXX handle possible overrun */
				data->word = (command < 0xFF) ?
						*(u16*)&chip->dat[command] :
						(u16)chip->dat[command];

				dev_dbg(&adap->dev,
					"I2C_SMBUS_WORD_DATA: addr=0x%02x, read 0x%04x at 0x%02x\n",
					addr, data->word, command);
			}

			break;

		case I2C_SMBUS_I2C_BLOCK_DATA:
		case I2C_SMBUS_BLOCK_DATA:

			/* FIXME sanity check; is this necessary? */
			if (data->block[0] > I2C_SMBUS_BLOCK_MAX)
				data->block[0] = I2C_SMBUS_BLOCK_MAX;

			if (data->block[0] > I2C_CHIP_SIZE - command)
				data->block[0] = I2C_CHIP_SIZE - command;

			len = data->block[0];

			if (read_write == I2C_SMBUS_WRITE) {
				for (i = 0; i < len; i++) {
					chip->dat[command + i] = data->block[1 + i];
				}

				dev_dbg(&adap->dev,
					"I2C_SMBUS_%sBLOCK_DATA: addr=0x%02x, wrote %d bytes at 0x%02x\n",
					size == I2C_SMBUS_I2C_BLOCK_DATA ? "I2C_" : "", addr, len, command);
			}
			else {
				for (i = 0; i < len; i++) {
					data->block[1 + i] =
						chip->dat[command + i];
				}

				dev_dbg(&adap->dev,
					"I2C_SMBUS_%sBLOCK_DATA: addr=0x%02x, read %d bytes at 0x%02x\n",
					size == I2C_SMBUS_I2C_BLOCK_DATA ? "I2C_" : "", addr, len, command);
			}

			break;

		default:
			dev_dbg(&adap->dev, "Unsupported I2C/SMBus command\n");
			err = -EOPNOTSUPP;
			break;
	}

	return err;
}

static int vcpi2c_smbus_xfer(struct i2c_adapter * adap, u16 addr, unsigned short flags,
	char read_write, u8 command, int size, union i2c_smbus_data *data)
{
	struct vs_dev_priv * dev = to_priv(adap);
	int err = -NODEV_ERROR;

	if (dev->in_use) {
		lock_iface_devs(VS_I2C);

		err = do_smbus_xfer(adap, addr, flags, read_write,
			command, size, data);

		unlock_iface_devs(VS_I2C);
	}

	return err;
}

static u32 vcpi2c_func(struct i2c_adapter * adapter)
{
	return I2C_FUNCTIONALITY;
}

static const struct i2c_algorithm smbus_algorithm = {
	/* VcpSdkCmd */
	.master_xfer	= vcpi2c_master_xfer,
	/* i2c-tools*/
	.smbus_xfer	= vcpi2c_smbus_xfer,
	.functionality	= vcpi2c_func,
};

static void init_dev(struct vs_dev_priv * dev, struct vs_dev * vsdev,
	long index)
{
	dev->in_use = true;
	dev->vsdev = vsdev;
	dev->index = index;
	dev->resp_size = 0;
	memset(&dev->chip, 0, sizeof(dev->chip));
	dev->adapter.owner = THIS_MODULE;
	dev->adapter.class = I2C_CLASS_HWMON | I2C_CLASS_SPD;
	dev->adapter.algo = &smbus_algorithm;
	snprintf(dev->adapter.name,  sizeof(dev->adapter.name),
		"vcpsim i2c adapter %ld", index);
}

static struct vs_dev_priv * find_unused_dev(void)
{
	struct vs_dev_priv * dev;

	list_for_each_entry (dev, &devices, devices)
		if (!dev->in_use)
			return dev;
	return NULL;
}

static struct vs_dev_priv * alloc_dev(struct vs_dev * vsdev, long index)
{
	struct vs_dev_priv * ret = find_unused_dev();
	bool is_new = !ret;

	if (is_new && !(ret = kzalloc(sizeof(*ret), GFP_KERNEL)))
		return ret;

	init_dev(ret, vsdev, index);

	if (is_new)
		list_add(&ret->devices, &devices);

	return ret;
}

static void del_dev(struct vs_dev_priv * dev)
{
	list_del(&dev->devices);
	kfree(dev);
}

struct vs_dev_priv * vs_create_i2c_device(struct vs_dev * vsdev, long index)
{
	struct vs_dev_priv * dev;
	int err;

	if (index < 0 || index >= VS_MAX_DEVICES)
		/* can't happen? */
		pr_err("%s%ld: device not created; index out of range!",
			iface_to_str(VS_I2C), index);
	else
	if (!(dev = alloc_dev(vsdev, index)))
		pr_err("%s%ld: device not created; out of memory!",
			iface_to_str(VS_I2C), index);
	else
	if (!!(err = i2c_add_adapter(&dev->adapter))) {
		pr_err("%s%ld: i2c_add_adapter() failed (error code %d)",
			iface_to_str(VS_I2C), index, err);
		del_dev(dev);
		dev = NULL;
	}

	return dev;
}

void vs_destroy_i2c_device(struct vs_dev_priv * device)
{

	device->in_use = false;

	i2c_del_adapter(&device->adapter);
}

int vs_init_i2c(void)
{
	int err = 0;

	pr_debug("loading i2c driver\n");

	INIT_LIST_HEAD(&devices);

	pr_info("i2c driver loaded\n");

	return err;
}

void vs_cleanup_i2c(void)
{
	struct list_head * e;
	struct list_head * tmp;

	list_for_each_safe(e, tmp, &devices) {
		struct vs_dev_priv * dev = list_entry(e, struct vs_dev_priv, devices);

		/* sanity check */
		if (dev->in_use) {
			pr_err("%s%ld was not destroyed before driver unload!\n",
				iface_to_str(VS_I2C), dev->index);
			vs_destroy_i2c_device(dev);
		}

		del_dev(dev);
	}

	pr_info("i2c driver unloaded\n");
}


