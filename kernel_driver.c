#include <linux/init.h>
#include <linux/module.h> // We're working in VSCode, and VSCode doesn't like this. Ignore them.
#include <linux/keyboard.h>
#include <linux/input-event-codes.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define TARGET_KEY KEY_F8 // Replace with desired key

//----- Normalize the keycode. This section was garbarge in the past. Thanks for fixing it.
unsigned int normalize_keycode(unsigned int keycode) {
    // Check if the keycode is an extended keycode (typically > KEY_MAX, there's some cases where this is false, but we can worry about that if... Yeah we'll cross that bridge when we get to that.)
    if (keycode >= KEY_OK) {  // Extended keycodes should start at KEY_OK (153)
        // Return the normalized base keycode for the extended key (e.g., extended Tab -> KEY_TAB)
        return TARGET_KEY;
    }
    return keycode;  // For standard keycodes, we ball.
}

//Normalized version of the kbd normalinator
static int keyboard_notifier_callback(struct notifier_block *nblock,
                                      unsigned long action, void *data) {
    struct keyboard_notifier_param *param = data;
    unsigned int normalized_value = normalize_keycode(param->value);

    if (action == KBD_KEYSYM && param->down) {
        if (normalized_value == normalize_keycode(TARGET_KEY)) {
            printk(KERN_INFO "Shortcut detected: F8 key pressed.\n");

            // Do userspace action goes here
            // this is such a massive pain in the ass to get working
            struct file *file;
            file = filp_open("/tmp/trigger_action", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (!IS_ERR(file)) {
                kernel_write(file, "open_firefox\n", 13, &file->f_pos);
                filp_close(file, NULL);
                printk(KERN_INFO "File Created \n");
            } else {
                printk(KERN_ERR "Failed to write to /tmp/trigger_action.\n");
            }
        }
    }

    return NOTIFY_OK;
}

// Notifier block
static struct notifier_block nb = {
    .notifier_call = keyboard_notifier_callback
};

// Initialize module
static int __init shortcut_init(void) {
    int ret = register_keyboard_notifier(&nb);
    if (ret) {
        printk(KERN_ERR "Failed to register keyboard notifier.\n");
        return ret;
    }
    printk(KERN_INFO "Shortcut driver initialized successfully.\n");
    return 0;
}

// Cleanup module
static void __exit shortcut_exit(void) {
    unregister_keyboard_notifier(&nb);
    printk(KERN_INFO "Shortcut driver unloaded.\n");
}

module_init(shortcut_init);
module_exit(shortcut_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anish Boddu");
MODULE_DESCRIPTION("Keyboard shortcut driver to open Firefox with a URL");
