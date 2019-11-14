# beechfuzz/nweb

An updated version of [ankushagarwal/nweb](https://github.com/ankushagarwal/nweb).  Although you can read about the original nweb [here](https://www.ibm.com/developerworks/systems/library/es-nweb/index.html), below are a few snippets:


>Have you ever wanted to run a tiny, safe web server without worrying about using a fully blown web server that could be complex to install and configure? Do you wonder how to write a program that accepts incoming messages with a network socket? Have you ever just wanted your own Web server to experiment and learn with? Further updates in 2012 to support recent web-server and browser standards and a code refresh.
>
>Well, look no further -- nweb is what you need. This is a simple Web server that has only 200 lines of C source code. It runs as a regular user and can't run any server-side scripts or programs, so it can't open up any special privileges or security holes.
>
>nweb only transmits the following types of files to the browser :
>* Static Web pages with extensions .html or .htm
>* Graphical images such as .gif, .png, .jgp, or .jpeg
>* Compressed binary files and archives such as .zip, .gz, and .tar
>
>If your favorite static file type is not in this list, you can simply add it in the source code and recompile to allow it.


&nbsp;

# Features/Changes I've made:

Action|Description
-|-
Change|Made the source easier to read
Add|`getopt` argument parser
Add|Can specify location of log file
Add|Can daemonize process

# Compiling

Just need a basic C compiler.  `gcc -O2 nweb.c -o nweb --static` should suffice.

# Usage

    Usage: nweb [OPTION...]
    
    Options:
        -d          Run in the background as a daemon.
        -l LOGFILE  Specify log location. If option is not used, then log will be at '/var/log/nweb/nweb.log'.
        -p PORT     Specify port.  If option is not used, then nweb will serve on port 80.
        -r ROOTDIR  Specify directory where index.html is located. If option is not used, then nweb will use the current working directory.
        -h          Display the full help.
    
    Arguments:
        LOGFILE   Full path to log file. (Ex: '/var/log/nweb/nweb.log')
        PORT      Port number that nweb will serve files on. (Ex: '80')
        ROOTDIR   Full path to the directory where index.thml is located. (Ex: '$PWD')

