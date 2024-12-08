// Evan Digles and Anish Boddu's Kernel Keyboard Shortcut Driver.
// COSC 439
// File name should be kernel_driver, not that it's the most fitting name, but because it's what we started with and we don't want to change it.
// So that's Project 6: Advanced Customizable Keyboard Shortcut Mapping Device Driver
// We have some suggested naming conventions from ChatGPT that we'll be using so we don't confuse eachother and to make bug testing easier
// since the professor said our use of ChatGPT to check for bugs was fine. See those naming conventions below.
// Defines should be all upercase with underscores for spaces, other variables should be full words with underscores between where necessary.

// Also, if we change the order of the functions, which sometimes happens, things can break. Just leave the order please.

// Imports for our use.
#include <linux/init.h>
#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/input-event-codes.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h> // I really hate this one, but it's what's working and what we decided to keep with.

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anish Boddu, Evan Digles");
MODULE_DESCRIPTION("A Simple Linux Keyboard Shortcut Driver");
MODULE_VERSION("0.5");

// A temp file for communicating our key info.
#define TMP_FILE_PATH "/tmp/key_pressed"

// How many keys we want to be able to have in our combinations. Defualt is "10", we should lower this in the future.
#define MAX_KEYS 10

// A strucutre for getting the information of a key, including what key it is, how long it has been pressed, if it is currently pressed down, and if the key has been used in a command.
struct key_info {
    // The order of these keeps getting reset...
    unsigned int key_code; // Works for both extended and non-extended.
    ktime_t hold_time;
    bool down_press;
    bool command_executed;  // Track if command has been executed, currently doesn't really work, but we're out of time and this is still used for something.

};

// Key combinations rely on this array. Our old implementation sucked and I don't want to talk about it.
static struct key_info key_states[MAX_KEYS];

// Structure definitions for working.
static struct workqueue_struct *work_queue; // If you change this, don't forget to change the related things too.
static struct work_struct write_key_work;



// Function to get the current time in milliseconds, we need this for our hold durations.
// Had a lot of bugs with this one, not proud of this being used at all.
// If we find a better alternative, it'd be better to use that alternative.
static unsigned long long current_time_ms(void) {
    // This somewhat unclear code is a replacement for much more unclear code that was written at 2AM that didn't work right.
    struct timespec64 ts;
    ktime_get_real_ts64(&ts);
    return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL); // Basically, we add the number of milliseconds in the current count of seconds, and the number of 
                                                            // milliseconds in the current count of nanoseconds beyond the seconds. It kinda sucks.

}



// Function to write keys and hold durations to /tmp/key_pressed
// Basically, anything in the key information structure will be printed to the temp file.
void write_key_task(struct work_struct *work) { // Be careful with this, since this basically exists to prevent some runtime errors.
    // Simple-ish variables.
    struct file *file;
    char buffer[256]; // This buffer may be a little overkill but it's not too demanding.
    int offset = 0, i;

    // For each key, we're going to send it into the offset.
    for (i = 0; i < MAX_KEYS; i++) {
        if (key_states[i].down_press) {
            unsigned long long /* 18.4 QUINTILLION!?!? But if we don't leave it this way, ChatGPT sends us threatening messages...*/hold_time = current_time_ms() - key_states[i].hold_time;
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%u:%llu,", key_states[i].key_code, hold_time);
        }

    }

    // After we send our keys to the offset, if we have anything in the offset we prepare it to be sent to the temp file.
    if (offset > 0) {
        buffer[offset - 1] = '\n'; // Replace last comma with newline, helps with reading it.
        file = filp_open(TMP_FILE_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (!IS_ERR(file)) {
            loff_t pos = 0;
            kernel_write(file, buffer, offset, &pos);
            filp_close(file, NULL);
            // debug messages below:
            printk(KERN_INFO "Keys written: %s\n", buffer); // DEBUG: What key did we write?
        } else {
            printk(KERN_ERR "Failed to write to %s.\n", TMP_FILE_PATH); // Error message if something went wrong.
        }
        
    }


}

// This is what actually does the hard work in getting the key information.
static int keyboard_notifier_callback(struct notifier_block *nblock, unsigned long action, void *data) {

    // We're going to need a structure.
    struct keyboard_notifier_param *param = data;
    // Might as well declare i here, since we'll be reseting it in the loops themselves.
    int i;

    if (action == KBD_KEYSYM) {
        if (param->down) { // If we pressed it down
            // Find the key in the array first, or at least try to.
            int free_keys_index = -1;
            int key_index = -1;
            for (i = 0; i < MAX_KEYS; i++) { // Basically, this loop checks if we're already holding down the key.
                if (key_states[i].down_press && key_states[i].key_code == param->value) {
                    key_index = i;
                    break;
                }
                if (!key_states[i].down_press && free_keys_index == -1) { // I know we don't NEED the curly brackets here, but if these are removed again
                                                                  // I am going to burst a vein.
                    free_keys_index = i; // IT'S ONE LINE.
                }
            }

            // If the key isn't already pressed, according to the loop above.
            if (key_index == -1) {
                if (free_keys_index != -1) { // New press (not just the same key that has been held down!)
                    key_states[free_keys_index].key_code = param->value;
                    key_states[free_keys_index].hold_time = current_time_ms();
                    key_states[free_keys_index].down_press = true;
                    key_states[free_keys_index].command_executed = false;
                } else {
                    printk(KERN_WARNING "no free slot for key %u\n", param->value); // Our size for keys is excessively large already. We should never reach this realistically.
                }
            } 
            // else if (key_index != -1), the key is already pressed, so do nothing for repeats
        } else {
            // Key no longer being held down.
            for (i = 0; i < MAX_KEYS; i++) {
                if (key_states[i].key_code == param->value && key_states[i].down_press) {
                    key_states[i].down_press = false;
                    break;
                }
            }
        }

        // Handled? Do the work.
        schedule_work(&write_key_work);
    }

    return NOTIFY_OK;
}

// Set up the notifier block for the initialization, supposedly we NEED to do this.
static struct notifier_block ready_notifier_block = { // Not the best name for it, but it works.
    .notifier_call = keyboard_notifier_callback
};

// Initialize module.
static int __init kernel_driver_init(void) {

    // do NOT unify this with how work_queue works. They work fundamentally different.
    int ret = register_keyboard_notifier(&ready_notifier_block);
    if (ret) { // DEBUG: we were having a spot of irritation before. This helps.
        printk(KERN_ERR "Failed to register keyboard notifier.\n");
        return ret;
    }

    work_queue = create_workqueue("shortcut_work_queue");
    if (!work_queue) {
        printk(KERN_ERR "Failed to create workqueue.\n");
        return -ENOMEM; // ChatGPT gave this return, and the previous 2 kernel error print messages since we were having a bug here before. We're not questioning it.
    }
    INIT_WORK(&write_key_work, write_key_task);

    printk(KERN_INFO "kernel driver initialized successfully.\n"); // It works? It works!!!
    return 0; // 0 for correctly working.
}

// De-initialize module. Make sure this matches for everything initialized above.
static void __exit kernel_driver_exit(void) {
    // Keyboard notifier needs to be unregistered.
    unregister_keyboard_notifier(&ready_notifier_block);

    // Work queue needs to be flushed and destroyed.
    flush_workqueue(work_queue);
    destroy_workqueue(work_queue);


    printk(KERN_INFO "kernel driver unloaded.\n");
}


// The actual init and exit that is done when we insmod or rmmod.
module_init(kernel_driver_init);
module_exit(kernel_driver_exit);

