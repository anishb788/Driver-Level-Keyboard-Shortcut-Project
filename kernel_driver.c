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

    // Normalize the keycode of the pressed key, so that it works with the thing below this.
    unsigned int normalized_value = normalize_keycode(param->value);

    // Normalize TARGET_KEY to handle both base and extended versions.
    unsigned int normalized_target_key = normalize_keycode(TARGET_KEY);

    // Check all key events. We'll need to change this later for only the specifics, but we can do that once we're more certain of our implementation.
    if (action == KBD_KEYSYM) {
        
        // DEBUG: FOllowing section(s) before the taget key check are for showing our key values with the normalizer.
        printk(KERN_INFO "Key event: raw_value=%d normalized_value=%d TARGET_KEY=%d down=%d\n",
               param->value, normalized_value, normalized_target_key, param->down);

        // This lets you know if our target key is pressed. Should work.
        if (param->value == TARGET_KEY) {
            printk(KERN_INFO "Match found: raw_value matches TARGET_KEY\n");
        }
        if (normalized_value == normalized_target_key) {
            printk(KERN_INFO "Match found: normalized_value matches TARGET_KEY\n");
        }

        // Check for the target key. This is how we want our actual implementation to be.
        if (param->value == TARGET_KEY || normalized_value == normalized_target_key) {
            if (param->down) {
                printk(KERN_INFO "Target key (%d or %d) pressed\n", param->value, normalized_value);
                snprintf(message, sizeof(message), "Key %d pressed\n", TARGET_KEY);
                message_size = strlen(message);
                printk(KERN_INFO "Key event recorded: %s", message);
            } else {
                printk(KERN_INFO "Target key (%d or %d) released\n", param->value, normalized_value);
            }
        }
    }

    return NOTIFY_OK;
}


// **Device File Operations**

static ssize_t device_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    if (*off >= message_size) {
        return 0;  // End of file
    }

    if (copy_to_user(buf, message, message_size)) {
        return -EFAULT;
    }

    *off += message_size;
    return message_size;
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