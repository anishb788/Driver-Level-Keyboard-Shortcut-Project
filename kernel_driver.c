#include <linux/init.h>
#include <linux/module.h> // We're working in VSCode, and VSCode doesn't like this. Ignore them.
#include <linux/keyboard.h>
#include <linux/input-event-codes.h>
#include <linux/notifier.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anish Boddu, Evan Digles");
MODULE_DESCRIPTION("A Simple Linux Keyboard Shortcut Driver");
MODULE_VERSION("0.1");

#define DEVICE_NAME "key_event_device"
#define TARGET_KEY KEY_TAB  // Change this to the key you want to listen for. We're using TAB for testing right now.
#define BUFFER_SIZE 16  // Maximum number of events to store

struct key_event {
    char message[256];  // Store the event message
};

static struct key_event ring_buffer[BUFFER_SIZE];
static int write_pos = 0;  // Next position to write
static int read_pos = 0;   // Next position to read
static int buffer_count = 0;  // Number of events in the buffer

static dev_t dev_number;       // Device number
static struct cdev c_dev;      // Character device structure
static char message[256];      // Buffer for message
static int message_size = 0;   // Size of the message

//----- Normalize the keycode. This section was garbarge in the past. Thanks for fixing it.
unsigned int normalize_keycode(unsigned int keycode) {
    // Check if the keycode is an extended keycode (typically > KEY_MAX, there's some cases where this is false, but we can worry about that if... Yeah we'll cross that bridge when we get to that.)
    if (keycode >= KEY_OK) {  // Extended keycodes should start at KEY_OK (153)
        // Return the normalized base keycode for the extended key (e.g., extended Tab -> KEY_TAB)
        return TARGET_KEY;
    }
    return keycode;  // For standard keycodes, we ball.
}

static int keyboard_notifier_callback(struct notifier_block *nblock,
                                      unsigned long action, void *data) {
    struct keyboard_notifier_param *param = data;
    unsigned int normalized_value = normalize_keycode(param->value);

    if (action == KBD_KEYSYM && param->down) {
        if (normalized_value == normalize_keycode(TARGET_KEY)) {
            snprintf(ring_buffer[write_pos].message, sizeof(ring_buffer[write_pos].message),
                     "Key %d pressed\n", TARGET_KEY);

            write_pos = (write_pos + 1) % BUFFER_SIZE;
            if (buffer_count < BUFFER_SIZE) {
                buffer_count++;
            } else {
                read_pos = (read_pos + 1) % BUFFER_SIZE;
            }
        }
    }

    return NOTIFY_OK;
}


// **Device File Operations**

static ssize_t device_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    if (buffer_count == 0) {
        return 0;  // No data to read
    }

    if (read_pos < 0 || read_pos >= BUFFER_SIZE) {
        return -EFAULT;  // Invalid read position
    }

    // Ensure we include a newline for each message
    char message_with_newline[260]; // 256 message + 1 newline + 1 null terminator
    snprintf(message_with_newline, sizeof(message_with_newline), "%s\n", ring_buffer[read_pos].message);

    size_t message_len = strlen(message_with_newline);
    if (len < message_len) {
        return -EINVAL;  // User buffer too small
    }

    if (copy_to_user(buf, message_with_newline, message_len)) {
        return -EFAULT;
    }

    read_pos = (read_pos + 1) % BUFFER_SIZE;
    buffer_count--;
    return message_len;
}


static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = device_read,
};


// Notifier block
static struct notifier_block nb = {
    .notifier_call = keyboard_notifier_callback,
};


static struct class *key_event_class;  // Class for the device
static struct device *key_event_device;  // Device structure

// init function, make sure we have everything that we're using in here.
static int my_driver_init(void) {
    int ret;

    // Allocate a device number
    if (alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ERR "Failed to allocate device number\n");
        return -1;
    }

    // Initialize the character device
    cdev_init(&c_dev, &fops);
    if (cdev_add(&c_dev, dev_number, 1) < 0) {
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "Failed to add character device\n");
        return -1;
    }

    // Create a device class
    key_event_class = class_create("key_event_class");
    if (IS_ERR(key_event_class)) {
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "Failed to create device class\n");
        return PTR_ERR(key_event_class);
    }

    // Create the device
    key_event_device = device_create(key_event_class, NULL, dev_number, NULL, DEVICE_NAME);
    if (IS_ERR(key_event_device)) {
        class_destroy(key_event_class);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "Failed to create device\n");
        return PTR_ERR(key_event_device);
    }

    // Register the keyboard notifier
    nb.notifier_call = keyboard_notifier_callback;
    if (register_keyboard_notifier(&nb)) {
        device_destroy(key_event_class, dev_number);
        class_destroy(key_event_class);
        cdev_del(&c_dev);
        unregister_chrdev_region(dev_number, 1);
        printk(KERN_ERR "Failed to register keyboard notifier\n");
        return -1;
    }

    printk(KERN_INFO "Character device created at /dev/%s\n", DEVICE_NAME);
    return 0;
}


// This removes the driver. Make sure this is correct to the init.
static void my_driver_exit(void)
{
    unregister_keyboard_notifier(&nb);
    device_destroy(key_event_class, dev_number);  // Remove device
    class_destroy(key_event_class);              // Remove class
    cdev_del(&c_dev);                             // Remove cdev
    unregister_chrdev_region(dev_number, 1);      // Unregister device number
    printk(KERN_INFO "Keyboard driver module unloaded\n");
}

module_init(my_driver_init);
module_exit(my_driver_exit);