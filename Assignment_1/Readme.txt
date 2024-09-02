V_number: V00979752
Section: A02
Name: Karan Gosal

Files included in this Assignment: linked_list.c, linked_list.h, main.c, Makefile, Readme.txt

Before compiling and running, please make sure you are in same dir as are the Files.

To complile:
    Run the command: make all

To Run:
    ./pman

Note:
In case of termination of a process outside the terminal(without using bgkill), the process killed 
will show if enter is hit while using pman or any new command is entered. This helps to keep
track of all the processes, in case killed outside the program.

Commands:

1. bg - To create a background process for the executable provided.

    bg {executable_name}
        OR
    bg {executable_path}

    NOTE: For your own executable, pass the name with or without arguments after compiling.
    The program works for both the name as well if passed as a path.

2. bglist - To list the background processes.

    bglist

    NOTE: No arguments are needed.

3. bgkill - To kill the background process using pid.

    bgkill {pid}

    Example: bgkill 1234567. Replace the pid with a valid pid.

4.  bgstop - To stop the background process using pid.

    bgstop {pid}

    Example: bgstop 1234567. Replace the pid with a valid pid.

5.  bgstart - To kill the background process using pid.

    bgstart {pid}

    Example: bgstart 1234567. Replace the pid with a valid pid.

6.  pstat - To get the stats for the background process using pid.

    pstat {pid}
    
    Example: pstat 1234567. Replace the pid with a valid pid.

