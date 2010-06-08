/* drivers/input/altboot.c
 *
 * Copyright (C) 2010 Gr0gmint, Inc.
 * Copyright (C) 2008 Google, Inc.
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

#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/syscalls.h>

struct altboot_platform_data {
	int *keys_up;
	int keys_down[]; /* 0 terminated */
};
extern void enable_altboot();

struct altboot_state {
	struct input_handler input_handler;
	unsigned long keybit[BITS_TO_LONGS(KEY_CNT)];
	unsigned long upbit[BITS_TO_LONGS(KEY_CNT)];
	unsigned long key[BITS_TO_LONGS(KEY_CNT)];
	spinlock_t lock;
	int key_down_target;
	int key_down;
	int key_up;
	int restart_disabled;
};

static void altboot_event(struct input_handle *handle, unsigned int type,
			   unsigned int code, int value)
{
	unsigned long flags;
	struct altboot_state *state = handle->private;

	if (type != EV_KEY)
		return;

	//printk(KERN_CRIT "altboot_event %u %u %d  \n", type, code, value);
	if (code == KEY_HOME) {
		enable_altboot();
	}
}

static int altboot_connect(struct input_handler *handler,
					  struct input_dev *dev,
					  const struct input_device_id *id)
{
	int i;
	int ret;
	struct input_handle *handle;
	struct altboot_state *state =
		container_of(handler, struct altboot_state, input_handler);


	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "altboot";
	handle->private = state;

	ret = input_register_handle(handle);
	if (ret)
		goto err_input_register_handle;

	ret = input_open_device(handle);
	if (ret)
		goto err_input_open_device;

	pr_info("using input dev %s for altboot\n", dev->name);

	return 0;

err_input_open_device:
	input_unregister_handle(handle);
err_input_register_handle:
	kfree(handle);
	return ret;
}

static void altboot_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id altboot_ids[] = {
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ },
};
MODULE_DEVICE_TABLE(input, altboot_ids);

static int altboot_probe(struct platform_device *pdev)
{
	int ret;
	int key, *keyp;
	struct altboot_state *state;
	struct altboot_platform_data *pdata = pdev->dev.platform_data;

	if (!pdata)
		return -EINVAL;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	spin_lock_init(&state->lock);
	keyp = pdata->keys_down;
	state->input_handler.event = altboot_event;
	state->input_handler.connect = altboot_connect;
	state->input_handler.disconnect = altboot_disconnect;
	state->input_handler.name = "altboot";
	state->input_handler.id_table = altboot_ids;
	ret = input_register_handler(&state->input_handler);
	if (ret) {
		kfree(state);
		return ret;
	}
	platform_set_drvdata(pdev, state);
	return 0;
}

int altboot_remove(struct platform_device *pdev)
{
	struct altboot_state *state = platform_get_drvdata(pdev);
	input_unregister_handler(&state->input_handler);
	kfree(state);
	return 0;
}


struct platform_driver altboot_driver = {
	.driver.name = "altboot",
	.probe = altboot_probe,
	.remove = altboot_remove,
};

static int __init altboot_init(void)
{
	return platform_driver_register(&altboot_driver);
}

static void __exit altboot_exit(void)
{
	return platform_driver_unregister(&altboot_driver);
}

module_init(altboot_init);
module_exit(altboot_exit);
