#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anish Boddu, Evan Digles");
MODULE_DESCRIPTION("A Simple Linux Keyboard Shortcut Driver");

static int my_driver_init(void)
{
    printk(KERN_INFO "Keyboard driver module loaded");
	return 0;
}

static void my_driver_exit(void)
{
    printk(KERN_INFO "Keyboard driver module unloaded");
    return;
}

module_init(my_driver_init);
module_exit(my_driver_exit);