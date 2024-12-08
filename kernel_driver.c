#include <linux/init.h>
#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/input-event-codes.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>

#define TMP_FILE_PATH "/tmp/key_pressed"
#define MAX_KEYS 10

struct key_info {
    unsigned int key_code;
    ktime_t press_time;
    bool pressed;
    bool command_executed;  // Track if command has been executed
};

static struct key_info key_states[MAX_KEYS];
static struct workqueue_struct *wq;
static struct work_struct write_key_work;

// Helper function to get the current time in milliseconds
static unsigned long long current_time_ms(void) {
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

// Function to write keys and hold durations to /tmp/key_pressed
void write_key_task(struct work_struct *work) {
    struct file *file;
    char buffer[256];
    int offset = 0, i;

    for (i = 0; i < MAX_KEYS; i++) {
        if (key_states[i].pressed) {
            unsigned long long hold_time = 
                current_time_ms() - key_states[i].press_time;
            offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                               "%u:%llu,", key_states[i].key_code, hold_time);
        }
    }

    if (offset > 0) {
        buffer[offset - 1] = '\n'; // Replace last comma with newline
        file = filp_open(TMP_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (!IS_ERR(file)) {
            loff_t pos = 0;
            kernel_write(file, buffer, offset, &pos);
            filp_close(file, NULL);
            printk(KERN_INFO "Keys written: %s\n", buffer);
        } else {
            printk(KERN_ERR "Failed to write to %s.\n", TMP_FILE_PATH);
        }
    }
}

// Keyboard notifier callback
static int keyboard_notifier_callback(struct notifier_block *nblock, 
                                      unsigned long action, void *data) {
    struct keyboard_notifier_param *param = data;
    int i;

    if (action == KBD_KEYSYM) {
        // "Down" event means key is pressed or autorepeated
        if (param->down) {
            // Find the key in the array first
            int free_index = -1;
            int key_index = -1;
            for (i = 0; i < MAX_KEYS; i++) {
                if (key_states[i].pressed && key_states[i].key_code == param->value) {
                    key_index = i;
                    break;
                }
                if (!key_states[i].pressed && free_index == -1)
                    free_index = i;
            }

            // If the key isn't already pressed
            if (key_index == -1) {
                // New press (not just a repeat)
                if (free_index != -1) {
                    key_states[free_index].key_code = param->value;
                    key_states[free_index].press_time = current_time_ms();
                    key_states[free_index].pressed = true;
                    key_states[free_index].command_executed = false;
                } else {
                    printk(KERN_WARNING "No free slot for key %u\n", param->value);
                }
            } 
            // else if (key_index != -1), the key is already pressed, so do nothing for repeats
        } else {
            // Key release event
            for (i = 0; i < MAX_KEYS; i++) {
                if (key_states[i].key_code == param->value && key_states[i].pressed) {
                    key_states[i].pressed = false;
                    break;
                }
            }
        }

        // Schedule write work after handling
        schedule_work(&write_key_work);
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
MODULE_DESCRIPTION("Keyboard shortcut driver to handle key combinations and durations.");
