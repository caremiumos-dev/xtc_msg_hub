// SPDX-License-Identifier: GPL-2.0
/*
 * XTC message hub
 *
 * Original driver: Owill Liu
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#define XTC_MSG_HUB_FIFO_SIZE	(64 * 1024)

static struct kobject *xtc_msg_hub_kobj;
static struct kfifo xtc_msg_fifo;
static DEFINE_MUTEX(xtc_msg_lock);
static bool xtc_msg_switch = true;

static ssize_t xtc_msg_hub_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	unsigned int len;
	unsigned int copied;
	unsigned int space;

	mutex_lock(&xtc_msg_lock);
	len = min_t(unsigned int, kfifo_len(&xtc_msg_fifo), PAGE_SIZE - 1);
	copied = kfifo_out(&xtc_msg_fifo, buf, len);
	space = kfifo_avail(&xtc_msg_fifo);
	mutex_unlock(&xtc_msg_lock);

	buf[copied] = '\0';
	pr_debug("%s:%d native function call xtc_msg_hub_show ok\n",
		 __func__, __LINE__);
	pr_debug("xtc_msg_hub size:%u space:%u\n", copied, space);

	return copied;
}

static ssize_t xtc_msg_hub_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned int len = count;

	if (!READ_ONCE(xtc_msg_switch))
		return count;

	if (len > kfifo_size(&xtc_msg_fifo))
		return -EINVAL;

	mutex_lock(&xtc_msg_lock);
	if (kfifo_avail(&xtc_msg_fifo) < len) {
		mutex_unlock(&xtc_msg_lock);
		pr_warn("xtc_msg_hub queue is full, dropping message\n");
		return -ENOSPC;
	}

	kfifo_in(&xtc_msg_fifo, buf, len);
	mutex_unlock(&xtc_msg_lock);

	pr_info("xtc_msg_hub receive msg from userspace -------------!\n");
	pr_info("xtc_msg_hub receive message (pid:%d):%s", current->pid, buf);

	return count;
}

static ssize_t xtc_msg_switch_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "msg hub switch state: %d\n",
			 READ_ONCE(xtc_msg_switch));
}

static ssize_t xtc_msg_switch_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	char tmp[16];
	size_t len = min_t(size_t, count, sizeof(tmp) - 1);
	int value;
	int ret;

	memcpy(tmp, buf, len);
	tmp[len] = '\0';

	ret = kstrtoint(strim(tmp), 0, &value);
	if (ret)
		return ret;

	WRITE_ONCE(xtc_msg_switch, value != 0);
	pr_info("hello xtc_msg_switch = %d !\n", READ_ONCE(xtc_msg_switch));

	return count;
}

static struct kobj_attribute xtc_msg_hub_attr =
	__ATTR(xtc_msg_hub, 0644, xtc_msg_hub_show, xtc_msg_hub_store);

static struct kobj_attribute xtc_msg_switch_attr =
	__ATTR(xtc_msg_switch, 0644, xtc_msg_switch_show,
	       xtc_msg_switch_store);

static int __init xtc_msg_hub_init(void)
{
	int ret;

	ret = kfifo_alloc(&xtc_msg_fifo, XTC_MSG_HUB_FIFO_SIZE, GFP_KERNEL);
	if (ret)
		return ret;

	xtc_msg_hub_kobj = kobject_create_and_add("msg_hub", kernel_kobj);
	if (!xtc_msg_hub_kobj) {
		pr_err("xtc_msg_hub create xtc_msg_hub_kobj failed!\n");
		ret = -ENOMEM;
		goto err_fifo;
	}

	ret = sysfs_create_file(xtc_msg_hub_kobj, &xtc_msg_hub_attr.attr);
	if (ret) {
		pr_err("xtc_msg_hub create sysfs failed!\n");
		goto err_kobj;
	}

	ret = sysfs_create_file(xtc_msg_hub_kobj, &xtc_msg_switch_attr.attr);
	if (ret) {
		pr_err("xtc_msg_hub create sysfs failed!\n");
		goto err_hub_attr;
	}

	pr_info("xtc_msg_hub_init init now!\n");
	pr_info("xtc_msg_hub size:%u space:%u\n",
		kfifo_size(&xtc_msg_fifo), kfifo_avail(&xtc_msg_fifo));

	return 0;

err_hub_attr:
	sysfs_remove_file(xtc_msg_hub_kobj, &xtc_msg_hub_attr.attr);
err_kobj:
	kobject_put(xtc_msg_hub_kobj);
err_fifo:
	kfifo_free(&xtc_msg_fifo);
	return ret;
}

static void __exit xtc_msg_hub_exit(void)
{
	sysfs_remove_file(xtc_msg_hub_kobj, &xtc_msg_switch_attr.attr);
	sysfs_remove_file(xtc_msg_hub_kobj, &xtc_msg_hub_attr.attr);
	kobject_put(xtc_msg_hub_kobj);
	kfifo_free(&xtc_msg_fifo);
}

module_init(xtc_msg_hub_init);
module_exit(xtc_msg_hub_exit);

MODULE_AUTHOR("Owill Liu");
MODULE_DESCRIPTION("XTC msg hub");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
