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

//----- Normalize the keycode
unsigned int normalize_keycode(unsigned int keycode) {
    // Check if the keycode is an extended keycode (typically > KEY_MAX)
    if (keycode >= KEY_OK) {  // Extended keycodes typically start at KEY_OK (153)
        // Return the normalized base keycode for the extended key (e.g., extended Tab -> KEY_TAB)
        return TARGET_KEY;
    }
    return keycode;  // For standard keycodes, return as-is
}

static int keyboard_notifier_callback(struct notifier_block *nblock,
                                      unsigned long action, void *data) {
    struct keyboard_notifier_param *param = data;

    // Normalize the keycode of the pressed key
    unsigned int normalized_value = normalize_keycode(param->value);

    // Normalize TARGET_KEY to handle both base and extended versions
    unsigned int normalized_target_key = normalize_keycode(TARGET_KEY);

    // Debug: Log all key events
    if (action == KBD_KEYSYM) {
        printk(KERN_INFO "Key event: raw_value=%d normalized_value=%d TARGET_KEY=%d down=%d\n",
               param->value, normalized_value, normalized_target_key, param->down);

        // Debug: Check if the raw or normalized key matches TARGET_KEY
        if (param->value == TARGET_KEY) {
            printk(KERN_INFO "Match found: raw_value matches TARGET_KEY\n");
        }
        if (normalized_value == normalized_target_key) {
            printk(KERN_INFO "Match found: normalized_value matches TARGET_KEY\n");
        }

        // Check for the target key
        if (param->value == TARGET_KEY || normalized_value == normalized_target_key) {
            if (param->down) {
                printk(KERN_INFO "Target key (%d or %d) pressed\n", param->value, normalized_value);
            } else {
                printk(KERN_INFO "Target key (%d or %d) released\n", param->value, normalized_value);
            }
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