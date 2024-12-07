#include <linux/init.h>
#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/input-event-codes.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define MAX_CMD_LEN 256
#define MAX_KEYS 10

struct key_command_mapping {
    unsigned int key_code;
    char command[MAX_CMD_LEN];
};

static struct key_command_mapping key_map[MAX_KEYS] = {
    {64353, "/usr/bin/firefox \"https://www.youtube.com/watch?v=qWNQUvIk954\"\n"},
    {64354, "xfce4-terminal\n"}
};

// Recommended by ChatGPT to prevent infinite recursion:
static struct workqueue_struct *wq;
static struct work_struct write_command_work;

static char current_command[MAX_CMD_LEN];

void write_command_task(struct work_struct *work) {
    struct file *file;
    file = filp_open("/tmp/trigger_action", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!IS_ERR(file)) {
        kernel_write(file, current_command, strlen(current_command), &file->f_pos);
        filp_close(file, NULL);
        printk(KERN_INFO "Command written: %s\n", current_command);
    } else {
        printk(KERN_ERR "Failed to write to /tmp/trigger_action.\n");
    }
}

// Keyboard notifier callback
static int keyboard_notifier_callback(struct notifier_block *nblock,
                                      unsigned long action, void *data) {
    struct keyboard_notifier_param *param = data;
    int i;

    if (action == KBD_KEYSYM && param->down) {
        for (i = 0; i < MAX_KEYS; i++) {
            if (key_map[i].key_code == param->value) {
                printk(KERN_INFO "Shortcut detected: Key %u pressed.\n", param->value);
                strncpy(current_command, key_map[i].command, MAX_CMD_LEN);
                current_command[MAX_CMD_LEN - 1] = '\0'; // Ensure null termination
                schedule_work(&write_command_work);  // Schedule the work
                break;
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

    wq = create_workqueue("shortcut_wq");
    if (!wq) {
        printk(KERN_ERR "Failed to create workqueue.\n");
        return -ENOMEM;
    }
    INIT_WORK(&write_command_work, write_command_task);
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
MODULE_DESCRIPTION("Keyboard shortcut driver to execute commands");
