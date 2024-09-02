V_number: V00979752
Section: A02
Name: Karan Gosal

Files: diskinfo.c, disklist.c, diskget.c, diskput.c, diskutils.h, diskutils.c, Makefile

Before compiling and running, please make sure you are in same dir as are the Files.

To compile:
    Run the command: make all

To Run:

1. Disk Info - To get the basic information like disk Label, OS Name, Total Size, Free Size etc.

    ./diskinfo <disk name>


2. Disk List - To display the contents of the root directory and all sub-directories in the file system.

    ./disklist <disk name>

3. Disk Get - To copy a file from the root directory of the file system to the current directory in Linux.

    ./diskget <disk name> <file name>

4. Disk Put - To copy a file from the current Linux directory into specified directory (i.e.,the root directory or a sub-directory) of the file system.
    
    Root Directory:              ./diskput <disk name> <file name>
    Some other Directory:        ./diskput <disk name> <Path To Dir in disk/file name>
