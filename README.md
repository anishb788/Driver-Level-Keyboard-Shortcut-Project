# Driver-Level-Keyboard-Shortcut-Project
This is a project that will allow someone to install a system level driver to linux that will let them map keybinds of their choosing to perform certain actions.

You will need to install inotify-tools for this driver to work.

sudo apt update
sudo apt install inotify-tools

Please note the following when running our project:

1. the kernel driver will need to be compiled locally on your machine via the included makefile
2. The listener script needs to be run seperately in a bash shell (WITHOUT ROOT)
3. if you would like to modify the keyboard shortcuts, those are it's own seperate program
