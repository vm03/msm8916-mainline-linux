// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2020-2021, Michael Srba

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>

#define CM36652_REGMAP_NAME	"cm36652_regmap"

/* register addresses */

#define REG_CS_CONF		0x00

#define REG_PS_CONF1		0x03 //the higher 8 bytes are CONF2
#define REG_PS_CONF3		0x04
#define REG_PS_THD		0x05
#define REG_PS_CANC		0x06
#define REG_PS_DATA		0x07

#define REG_CS_RED_DATA	0x08
#define REG_CS_GREEN_DATA	0x09
#define REG_CS_BLUE_DATA	0x0A
#define REG_CS_WHITE_DATA	0x0B

#define REG_INT_FLAG		0x0C

#define REG_DEV_ID		0x0D

/* register settings */

#define CS_CONF_ENABLE		(0<<0)
#define CS_CONF_DISABLE		(1<<0)

#define CS_CONF_INT_ENABLE	(1<<1)// whether to trigger an interrupt when the value of
#define CS_CONF_INT_DISABLE	(0<<1)// REG_CS_GREEN_DATA goes over a threshold

#define CS_CONF_PERS_1		(0<<2) // only assert an interrupt if value is over the set
#define CS_CONF_PERS_2		(1<<2) // threshold consistently for N cycles
#define CS_CONF_PERS_4		(2<<2)
#define CS_CONF_PERS_8		(3<<2)

#define CS_CONF_IT_80		(0<<4) // "integration time" - time of sensor exposition to light
#define CS_CONF_IT_160		(1<<4) // more integration time means higher resolution but lower
#define CS_CONF_IT_320		(2<<4) // detection range
#define CS_CONF_IT_640		(3<<4) // units - ms

#define PS_CONF1_PERS_1		(0<<2) // only assert an interrupt if value is over the set
#define PS_CONF1_PERS_2		(1<<2) // threshold consistently for N cycles
#define PS_CONF1_PERS_3		(2<<2)
#define PS_CONF1_PERS_4		(3<<2)

#define PS_CONF1_DR_1_80	(0<<6) // IR LED duty rate (1/n)
#define PS_CONF1_DR_1_160	(1<<6) // period T = n * tₒₙ
#define PS_CONF1_DR_1_320	(2<<6) // LED is on for tₒₙ
#define PS_CONF1_DR_1_640	(3<<6) // and off for the rest of period T

#define PS_CONF1_IT_1T		(0<<4) // "integration time" - see above
#define PS_CONF1_IT_1_3T	(1<<4) // units - n * T
#define PS_CONF1_IT_1_6T	(2<<4) // T is PWM period
#define PS_CONF1_IT_2T		(3<<4)

#define PS_CONF2_INT_DISABLE	((0<<0)<<8)
#define PS_CONF2_INT_ENABLE	((2<<0)<<8)
#define PS_CONF2_INT_AS_BOOLEAN	((3<<0)<<8) // logic LOW = object in proximity; disables CS interrupt

#define PS_CONF2_LED_100mA	((0<<2)<<8) // IR LED current - may be changed
#define PS_CONF2_LED_115mA	((1<<2)<<8) // in conjunction with the duty
#define PS_CONF2_LED_130mA	((2<<2)<<8) // cycle in order to micromanage
#define PS_CONF2_LED_140mA	((3<<2)<<8) // the characteristics
#define PS_CONF2_LED_160mA	((4<<2)<<8)
#define PS_CONF2_LED_200mA	((5<<2)<<8)
#define PS_CONF2_LED_75mA	((6<<2)<<8)
#define PS_CONF2_LED_50mA	((7<<2)<<8)

#define PS_CONF2_SMART_PERS_OFF	((0<<5)<<8) // "smart persistence" - automagically
#define PS_CONF2_SMART_PERS_ON	((1<<5)<<8) // figure out the right PERS value (?)

#define PS_CONF2_ITB_HALF	((0<<6)<<8) // tₒₙ = 60μs
#define PS_CONF2_ITB_DEFAULT	((1<<6)<<8) // tₒₙ = 120μs
#define PS_CONF2_ITB_DOUBLE	((2<<6)<<8) // tₒₙ = 240μs
#define PS_CONF2_ITB_QUADRUPLE	((3<<6)<<8) // tₒₙ = 480μs

#define REG_INT_PS_AWAY		(BIT(0)<<8)
#define REG_INT_PS_CLOSE	(BIT(1)<<8)

#define PS_THD(low_thd, high_thd) ((high_thd<<8)|low_thd)

enum cm36652_command {
	CM36652_CMD_READ_RAW_LIGHT,
	CM36652_CMD_READ_RAW_PROXIMITY,
	CM36652_CMD_PROX_EV_EN,
	CM36652_CMD_PROX_EV_DIS,
};

#define CM36652_LIGHT_CHANNEL(_color, _idx) {		\
	.type = IIO_LIGHT,				\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),	\
	.address = _idx,				\
	.modified = 1,					\
	.channel2 = IIO_MOD_LIGHT_##_color,		\
}							\

static const struct iio_event_spec cm36652_event_spec[] = {
	{
		.type = IIO_EV_TYPE_THRESH,
		.dir = IIO_EV_DIR_EITHER,
		.mask_separate = BIT(IIO_EV_INFO_VALUE) |
				BIT(IIO_EV_INFO_ENABLE),
	}
};

static const struct iio_chan_spec cm36652_channels[] = {
	{
		.type = IIO_PROXIMITY,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.address = REG_PS_DATA,
		.event_spec = cm36652_event_spec,
		.num_event_specs = ARRAY_SIZE(cm36652_event_spec),
	},
	CM36652_LIGHT_CHANNEL(RED, REG_CS_RED_DATA),
	CM36652_LIGHT_CHANNEL(GREEN, REG_CS_GREEN_DATA),
	CM36652_LIGHT_CHANNEL(BLUE, REG_CS_BLUE_DATA),
	CM36652_LIGHT_CHANNEL(CLEAR, REG_CS_WHITE_DATA),
};

struct cm36652_data {
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex lock;
	u8 power_state;
	struct regulator_bulk_data supplies[3];
};

static int cm36652_read_raw(struct iio_dev *iio_dev,
			    struct iio_chan_spec const *chan,
			    int *val, int *val2, long mask)
{
	struct cm36652_data *cm36652 = iio_priv(iio_dev);
	unsigned int buf = 0;
	int ret = IIO_VAL_INT;
	int err = 0;

	mutex_lock(&cm36652->lock);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (chan->type == IIO_LIGHT || chan->type == IIO_PROXIMITY) {
			err = regmap_read(cm36652->regmap, chan->address, &buf);
			if (err < 0)
				return -EINVAL;
			*val = buf;
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&cm36652->lock);

	return ret;
}

static const struct iio_info cm36652_info = {
	.read_raw		= &cm36652_read_raw,
};

irqreturn_t cm36652_irq_handler(int irq, void *data)
{
	struct iio_dev *iio_dev = data;
	struct cm36652_data *cm36652 = iio_priv(iio_dev);
	struct i2c_client *client = cm36652->client;
	int ps_data = 0;
	int int_flag = 0;
	int ev_dir;
	u64 ev_code;
	int ret;

	ret = regmap_read(cm36652->regmap, REG_PS_DATA, &ps_data);
	if (ret < 0) {
		dev_err(&client->dev, "irq: failed to read proximity sensor data: %d\n", ret);
		return ret;
	}

	ret = regmap_read(cm36652->regmap, REG_INT_FLAG, &int_flag); // the act of reading this register out acks the interrupt
	if (ret < 0) {
		dev_err(&client->dev, "irq: failed to read interrupt flag: %d\n", ret);
		return ret;
	}

	dev_info(&client->dev, "--- INT --- ps: %d, int: %x\n", ps_data, int_flag);

	if (int_flag & REG_INT_PS_AWAY)
		ev_dir = IIO_EV_DIR_FALLING;
	else if (int_flag & REG_INT_PS_CLOSE)
		ev_dir = IIO_EV_DIR_RISING;
	else
		dev_err(&client->dev, "irq: unknown interrupt reason; flags: 0x%2x\n", int_flag<<8);

	ev_code = IIO_UNMOD_EVENT_CODE(IIO_PROXIMITY,
				       CM36652_CMD_READ_RAW_PROXIMITY,
				       IIO_EV_TYPE_THRESH, ev_dir);

	iio_push_event(iio_dev, ev_code, iio_get_time_ns(iio_dev));

	return IRQ_HANDLED;
}

static int cm36652_setup_reg(struct cm36652_data *cm36652)
{
	struct i2c_client *client = cm36652->client;
	int ret = 0;

	/* CS initialization */
	ret = regmap_write(cm36652->regmap, REG_CS_CONF, CS_CONF_ENABLE);
	if (ret < 0) {
		dev_err(&client->dev, "failed to enable color sensor: %d\n", ret);
		return ret;
	}

	ret = regmap_write(cm36652->regmap, REG_PS_CONF1, PS_CONF1_PERS_3 | PS_CONF1_DR_1_640 | PS_CONF2_INT_ENABLE | PS_CONF2_LED_160mA | PS_CONF2_ITB_QUADRUPLE);
	if (ret < 0) {
		dev_err(&client->dev, "PS_CONF1 register setup failed: %d\n", ret);
		return ret;
	}

	ret = regmap_write(cm36652->regmap, REG_PS_THD, PS_THD(17, 20));
	if (ret < 0) {
		dev_err(&client->dev, "proximity sensor treshold setup failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static int cm36652_setup_supplies(struct cm36652_data *cm36652)
{
	struct i2c_client *client = cm36652->client;
	int ret = 0;

	cm36652->supplies[0].supply = "vdd";
	cm36652->supplies[1].supply = "vddio";
	cm36652->supplies[2].supply = "vled";
	ret = devm_regulator_bulk_get(&client->dev, ARRAY_SIZE(cm36652->supplies),
				      cm36652->supplies);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to get regulators: %d\n", ret);
		return ret;
	}

	return 0;
}

static bool cm36652_is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case REG_PS_DATA:
	case REG_CS_RED_DATA:
	case REG_CS_GREEN_DATA:
	case REG_CS_BLUE_DATA:
	case REG_CS_WHITE_DATA:
	case REG_INT_FLAG:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config cm36652_regmap_config = {
	.name		= CM36652_REGMAP_NAME,
	.reg_bits	= 8,
	.val_bits	= 16,
	.max_register	= REG_DEV_ID,
	.cache_type	= REGCACHE_RBTREE,
	.volatile_reg	= cm36652_is_volatile_reg,
	.val_format_endian = REGMAP_ENDIAN_LITTLE,
};

static int cm36652_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct cm36652_data *cm36652 = NULL;
	struct iio_dev *iio_dev;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c functionality check failed!\n");
		return ret;
	}

	iio_dev = devm_iio_device_alloc(&client->dev, sizeof(*cm36652));
	if (!iio_dev) {
		dev_err(&client->dev, "failed to allocate memory for cm36652 module data\n");
		return -ENOMEM;
	}

	cm36652 = iio_priv(iio_dev);

	cm36652->client = client;
	i2c_set_clientdata(client, iio_dev);

	cm36652->regmap = devm_regmap_init_i2c(client, &cm36652_regmap_config);
	if (IS_ERR(cm36652->regmap)) {
		dev_err(&client->dev, "regmap_init failed!\n");
		return PTR_ERR(cm36652->regmap);
	}

	ret = cm36652_setup_supplies(cm36652);
	if (ret < 0) {
		dev_err(&client->dev, "PROBE - Failed to enable regulators: %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(cm36652->supplies), cm36652->supplies);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	mutex_init(&cm36652->lock);
	iio_dev->dev.parent = &client->dev;
	iio_dev->channels = cm36652_channels;
	iio_dev->num_channels = ARRAY_SIZE(cm36652_channels);
	iio_dev->info = &cm36652_info;
	iio_dev->name = id->name;
	iio_dev->modes = INDIO_DIRECT_MODE;

	ret = cm36652_setup_reg(cm36652);
	if (ret < 0)
		goto out;

	ret = request_threaded_irq(client->irq, NULL, cm36652_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "cm36652", iio_dev);
	if (ret < 0) {
		pr_err("failed to request irq: %d\n", ret);
		goto out;
	}

	ret = iio_device_register(iio_dev);
	if (ret < 0) {
		pr_err("failed to register iio device: %d\n", ret);
		goto out;
	}

	return 0;
out:

	ret = regulator_bulk_disable(ARRAY_SIZE(cm36652->supplies), cm36652->supplies);
	if (ret < 0) {
		dev_err(&client->dev, "Failed to disable regulators: %d\n", ret);
		return ret;
	}

	return ret;
}

static int cm36652_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id cm36652_id[] = {
	{"cm36652", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cm36652_id);

static const struct of_device_id cm36652_of_match[] = {
	{ .compatible = "capella,cm36652",},
	{},
};

MODULE_DEVICE_TABLE(of, cm36652_of_match);

static struct i2c_driver cm36652_driver = {
	.driver = {
		   .name = "cm36652",
		   .owner = THIS_MODULE,
		   .of_match_table = cm36652_of_match,
	},
	.probe = cm36652_i2c_probe,
	.remove = cm36652_i2c_remove,
	.id_table = cm36652_id,
};

module_i2c_driver(cm36652_driver);

MODULE_AUTHOR("Michael Srba");
MODULE_DESCRIPTION("cm36652 ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");
