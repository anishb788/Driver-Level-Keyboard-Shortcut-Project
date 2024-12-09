# Driver-Level-Keyboard-Shortcut-Project
This is a project that will allow someone to install a system level driver to linux that will let them map keybinds of their choosing to perform certain actions.

You will need to install inotify-tools for this driver to work.

sudo apt update
sudo apt install inotify-tools

Please note the following when running our project:

1. the kernel driver will need to be compiled locally on your machine via the included makefile
2. The listener script needs to be run seperately in a bash shell (WITHOUT ROOT)
3. if you would like to modify the keyboard shortcuts, those are it's own seperate program

Now the instructions to run the project:
1. Open the project directory with all the files in the terminal and run "make"
2. Run: "sudo insmod kernel_driver.ko" in the terminal window
3. Run the file named UI by typing: "./ui" (if you wish, feel free to compile it from the source)
4. Finally, run the listener.sh file
5. Enjoy the driver!

UI Instructions:
 - To add a new shortcut, press add and then double click directly under the space towards the top
 - (note: due to some minor issues, the driver can be finicky and register keys that were not pressed, if this happens reload the driver and all issues should be fixed)
 - modifying a shortcut is as simple as clicking into whichever cell you want to change
 - Delete button will delete whatever entry is highlighted
 - changes do not take effect until the save button is pressed.
