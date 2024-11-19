#include <linux/init.h>
#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/input-event-codes.h>
#include <linux/notifier.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anish Boddu, Evan Digles");
MODULE_DESCRIPTION("A Simple Linux Keyboard Shortcut Driver");
MODULE_VERSION("1.0");

#define TARGET_KEY KEY_TAB  // Change this to the key you want to listen for

static int keyboard_notifier_callback(struct notifier_block *nblock,
                                      unsigned long action, void *data) {
    struct keyboard_notifier_param *param = data;

    // Check if the event is a key event (KBD_KEYSYM)
    if (action == KBD_KEYSYM && param->value == TARGET_KEY) {
        if (param->down) {
            printk(KERN_INFO "Target key (%d) pressed\n", TARGET_KEY);
        } else {
            printk(KERN_INFO "Target key (%d) released\n", TARGET_KEY);
        }
    }

    return NOTIFY_OK;
}


// Notifier block
static struct notifier_block nb = {
    .notifier_call = keyboard_notifier_callback,
};


static int my_driver_init(void) {
    int ret = register_keyboard_notifier(&nb);
    if (ret) {
        printk(KERN_ERR "Failed to register keyboard notifier\n");
        return ret;
    }
    printk(KERN_INFO "Keyboard driver module loaded\n");
    return 0;
}

static void my_driver_exit(void)
{
    unregister_keyboard_notifier(&nb);
    printk(KERN_INFO "Keyboard driver module unloaded\n");
    return;
}

module_init(my_driver_init);
module_exit(my_driver_exit);