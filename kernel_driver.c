#include <linux/init.h>
#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/input-event-codes.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define TMP_FILE_PATH "/tmp/key_pressed"

static struct workqueue_struct *wq;
static struct work_struct write_key_work;

static unsigned int last_key = 0;

// Function to write key press to /tmp/key_pressed
void write_key_task(struct work_struct *work) {
    struct file *file;
    char key_str[16];
    loff_t pos = 0;

    snprintf(key_str, sizeof(key_str), "%u\n", last_key);

    file = filp_open(TMP_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!IS_ERR(file)) {
        kernel_write(file, key_str, strlen(key_str), &pos);
        filp_close(file, NULL);
        printk(KERN_INFO "Key pressed: %s\n", key_str);
    } else {
        printk(KERN_ERR "Failed to write to %s.\n", TMP_FILE_PATH);
    }
}

// Keyboard notifier callback
static int keyboard_notifier_callback(struct notifier_block *nblock, unsigned long action, void *data) {
    struct keyboard_notifier_param *param = data;

    if (action == KBD_KEYSYM && param->down) {
        last_key = param->value;
        schedule_work(&write_key_work); // Schedule the work to write key to /tmp/key_pressed
    }

    return NOTIFY_OK;
}

// Notifier block
static struct notifier_block nb = {
    .notifier_call = keyboard_notifier_callback
};

// Initialize module
static int __init shortcut_init(void) {
    int ret;

    ret = register_keyboard_notifier(&nb);
    if (ret) {
        printk(KERN_ERR "Failed to register keyboard notifier.\n");
        return ret;
    }

    wq = create_workqueue("shortcut_wq");
    if (!wq) {
        printk(KERN_ERR "Failed to create workqueue.\n");
        return -ENOMEM;
    }
    INIT_WORK(&write_key_work, write_key_task);

    printk(KERN_INFO "Shortcut driver initialized successfully.\n");
    return 0;
}

// Cleanup module
static void __exit shortcut_exit(void) {
    unregister_keyboard_notifier(&nb);
    flush_workqueue(wq);
    destroy_workqueue(wq);
    printk(KERN_INFO "Shortcut driver unloaded.\n");
}

module_init(shortcut_init);
module_exit(shortcut_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anish Boddu, Evan Digles");
MODULE_DESCRIPTION("Keyboard shortcut driver to execute commands dynamically.");
