# iNode File System
Simple File System based on iNode structures.

This project is similar to the [Simple-File-System](https://github.com/PanK0/Simple-File-System/blob/master/README.md) project built for Operating Systems course at the Bachelor's Degree, but the file system here uses inode structures instead of linked lists.

## HOW TO RUN
To run the software is provided a test in */Tests*. Once there, launch the makefile with *make inodefs_test*.
After the compilation it's possible to run the software.
Looking at the output on the shell is possible to differentiate two other colors: red text inticates the choosen running option, yellow text gives information of what about the software is going to do.

### Running options
There is a shell option that provides to test the File System.
Open folder iNode_FS/Tests and choose a running option.

- **Shell Mode** : run *./inodefs_test shell* to initialize the FS and test by your own using a simple provided shell. Type *help* to show up all the shell commands.
