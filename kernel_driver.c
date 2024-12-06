#include <linux/init.h>
#include <linux/module.h> // We're working in VSCode, and VSCode doesn't like this. Ignore them.
#include <linux/keyboard.h>
#include <linux/input-event-codes.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define TARGET_KEY 64353 // Replace with desired key

//----- Normalize the keycode. This section was garbarge in the past. Thanks for fixing it.
unsigned int normalizer(unsigned int keycode) {
    return keycode;
}


// Recommended by ChatGPT to prevent infinite recursion:
static struct workqueue_struct *wq;
static struct work_struct open_firefox_work;

void open_firefox_task(struct work_struct *work) {
    struct file *file;
    file = filp_open("/tmp/trigger_action", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!IS_ERR(file)) {
        kernel_write(file, "open_firefox\n", 13, &file->f_pos);
        filp_close(file, NULL);
        printk(KERN_INFO "File Created: /tmp/trigger_action\n");
    } else {
        printk(KERN_ERR "Failed to write to /tmp/trigger_action.\n");
    }
}

// End of this block, but more related below.

//Normalized version of the kbd normalinator
static int keyboard_notifier_callback(struct notifier_block *nblock,
                                      unsigned long action, void *data) {
    struct keyboard_notifier_param *param = data;
    unsigned int normalized_value = normalizer(param->value);

    if (action == KBD_KEYSYM && param->down) {
        /* Previously:
        if (normalized_value == normalizer(TARGET_KEY)) {
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
        */
        // Now, recommended by ChatGPT:
        if (normalized_value == normalizer(TARGET_KEY)) {
            printk(KERN_INFO "Shortcut detected: F8 key pressed.\n");
            schedule_work(&open_firefox_work);  // Defer work to the worker function
        }
        // End of ChatGPT recommended segment.
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
    // ChatGPT recommended.
    wq = create_workqueue("shortcut_wq");
    if (!wq) {
        printk(KERN_ERR "Failed to create workqueue.\n");
        return -ENOMEM;
    }
    INIT_WORK(&open_firefox_work, open_firefox_task);  // Initialize the work struct
    // End of block.
    printk(KERN_INFO "Shortcut driver initialized successfully.\n");
    return 0;
}

// Cleanup module
static void __exit shortcut_exit(void) {
    unregister_keyboard_notifier(&nb);
    // ChatGPT recommended segment.
    flush_workqueue(wq);  // Ensure all scheduled work is completed
    destroy_workqueue(wq); // Destroy the work queue
    // End of block.
    printk(KERN_INFO "Shortcut driver unloaded.\n");
}

module_init(shortcut_init);
module_exit(shortcut_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anish Boddu");
MODULE_DESCRIPTION("Keyboard shortcut driver to open Firefox with a URL");