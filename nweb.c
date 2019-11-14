#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/limits.h>
#define VERSION 1.0f
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404


/***************************************************************
****************************************************************
**
**  GLOBAL VARIABLES
**  ----------------
*/

char *g_logfile;    /* Logfile location */


/***************************************************************
****************************************************************
**
**  SUPPORTED EXTENSIONS STRUCTURE
**  ------------------------------
*/

/*
 *  This structure is used to define supported file extensions that
 *  nweb can serve to users.
 */
struct extensionInfo {
    char *ext;
    char *filetype;
};
struct extensionInfo extensions [] = {
    {"gif", "image/gif" },
    {"jpg", "image/jpg" },
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"ico", "image/ico" },
    {"zip", "image/zip" },
    {"gz",  "image/gz"  },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"crt", "application/x-x509-ca-cert" },
    {"der", "application/x-x509-ca-cert" },
    {"html","text/html" },
    {0,0}
};


/***************************************************************
****************************************************************
**
**  FUNCTIONS
**  ---------
*/

/*
 *  usage()
 *  -------
 *  Used for displaying the usage options for nweb
 *
 */
void usage(){
    printf("Usage: nweb [OPTION...]\n\n"
            "Options:\n"
            "    -d          Run in the background as a daemon.\n"
            "    -l LOGFILE  Specify log location. If option is not used, then log will be at '/var/log/nweb/nweb.log'.\n"
            "    -p PORT     Specify port.  If option is not used, then nweb will serve on port 80.\n"
            "    -r ROOTDIR  Specify directory where index.html is located. If option is not used, then nweb will use the current working directory.\n"
            "    -h          Display the full help.\n\n"
            "Arguments:\n"
            "    LOGFILE   Full path to log file. (Ex: '/var/log/nweb/nweb.log')\n"
            "    PORT      Port number that nweb will serve files on. (Ex: '80')\n"
            "    ROOTDIR   Full path to the directory where index.thml is located. (Ex: '$PWD')\n\n");
    exit(1);
}
/*
 *  help()
 *  ------
 *  Used for displaying the help context
 *
 */
void help(){
    char supportedExtensions[1024];
    struct extensionInfo *ext;

    /* Print quick intro */
    printf("\nnweb -- A small and very safe mini web server.\n\n"
                    "nweb only serves out files and web pages with the extensions named below, "
                    "and only from the ROOTDIR folder and its subdirectories.  There are no "
                    "fancy features; just simple, safe, and secure.\n");

    /* Print list of supported file extensions */
    strncat(supportedExtensions,"Supported file extensions:", sizeof(supportedExtensions) -1);
    for (ext = extensions; ext->ext != 0; ext++){
        strncat(supportedExtensions, "\n    .", sizeof(supportedExtensions) - 1);
        strncat(supportedExtensions, ext->ext,  sizeof(supportedExtensions) - 1);
    }
    printf("\n%s\n\n",supportedExtensions);

    /* Print usage */
    usage();
}

/*     
 *  runAsDaemon()
 *  -------------
 *  Daemonizes nweb to run in the background instead of the foreground
 *     
 */    
       
static int runAsDaemon(){
    int i;
       
    if(fork() != 0){
        /* parent returns OK to shell */
        return 0;
    }  
       
    /* ignore child death and terminal hangups */
    (void)signal(SIGCHLD, SIG_IGN);
    (void)signal(SIGHUP, SIG_IGN);
       
    /* Close open files */
    for(i=0;i<32;i++){
        (void)close(i);
    }  
       
    /* Break away from process group */
    (void)setpgrp();
}      

/*
 *  logger()
 *  --------
 *  Used for logging messages to the logfile
 *
 */
void logger(int type, char *s1, char *s2, int socket_fd){
    int fd ;
    char logbuffer[BUFSIZE*2];

    switch (type) {
        case ERROR:
            (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid());
            break;
        case FORBIDDEN:
            (void)write(socket_fd, "HTTP/1.1 403 Forbidden\n"
                                   "Content-Length: 185\n"
                                   "Connection: close\n"
                                   "Content-Type: text/html\n\n"
                                   "<html>"
                                   "<head>\n<title>403 Forbidden</title>\n</head>"
                                   "<body>\n<h1>Forbidden</h1>\n"
                                   "The requested URL, file type or operation is not allowed "
                                   "on this simple static file webserver.\n</body>"
                                   "</html>\n",271);
            (void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2);
            break;
        case NOTFOUND:
            (void)write(socket_fd, "HTTP/1.1 404 Not Found\n"
                                   "Content-Length: 136\n"
                                   "Connection: close\n"
                                   "Content-Type: text/html\n\n"
                                   "<html>"
                                   "<head>\n<title>404 Not Found</title>\n</head>"
                                   "<body>\n<h1>Not Found</h1>\n"
                                   "The requested URL was not found on this server.\n</body>"
                                   "</html>\n",224);
            (void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2);
            break;
        case LOG:
            (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,socket_fd);
            break;
    }

    /* Write to log.  No checks here, nothing can be done with a failure anyway */
    if((fd = open(g_logfile, O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
        (void)write(fd,logbuffer,strlen(logbuffer));
        (void)write(fd,"\n",1);
        (void)close(fd);
    }

    /* No checks here, nothing can be done with a failure anyway */
    if(type == ERROR || type == NOTFOUND || type == FORBIDDEN){
        exit(3);
    }
}


/*
 *  web()
 *  -----
 *  This is a child web server process, so we can exit on errors
 *
 */
void web(int fd, int hit){

    int j, file_fd, buflen;
    long i, ret, len;
    char * fstr;
    static char buffer[BUFSIZE+1]; /* static so zero filled */

    /* read Web request in one go */
    ret =read(fd,buffer,BUFSIZE);

    /* read failure stop now */
    if(ret == 0 || ret == -1) {
        logger(FORBIDDEN,"failed to read browser request","",fd);
    }

    /* return code is valid chars */
    if(ret > 0 && ret < BUFSIZE){
        /* terminate the buffer */
        buffer[ret]=0;
    } else {
        buffer[0]=0;
    }

    /* remove CF and LF characters */
    for(i=0;i<ret;i++){
        if(buffer[i] == '\r' || buffer[i] == '\n'){
            buffer[i]='*';
        }
    }

    /* Log request */
    logger(LOG,"request",buffer,hit);

    if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
        logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd);
    }

    /*  null terminate after the second space to ignore extra stuff */
    for(i=4;i<BUFSIZE;i++) {
        /* string is "GET URL " +lots of other stuff */
        if(buffer[i] == ' ') {
            buffer[i] = 0;
            break;
        }
    }

    /* check for illegal parent directory use .. */
    for(j=0;j<i-1;j++){
        if(buffer[j] == '.' && buffer[j+1] == '.') {
            logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,fd);
        }
    }

    /* convert no filename to index file */
    if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ){
        (void)strcpy(buffer,"GET /index.html");
    }

    /* work out the file type and check we support it */
    buflen=strlen(buffer);
    fstr = (char *)0;
    for(i=0;extensions[i].ext != 0;i++) {
        len = strlen(extensions[i].ext);
        if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
            fstr =extensions[i].filetype;
            break;
        }
    }
    if(fstr == 0) {
        /* Log the unsupported file type */
        logger(FORBIDDEN,"file extension type not supported",buffer,fd);
    }

    /* open the file for reading */
    if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {
        logger(NOTFOUND, "failed to open file",&buffer[5],fd);
    }

    logger(LOG,"SEND",&buffer[5],hit);

    /* lseek to the file end to find the length */
    len = (long)lseek(file_fd, (off_t)0, SEEK_END);
    /* lseek back to the file start ready for reading */
    (void)lseek(file_fd, (off_t)0, SEEK_SET);

    /* Header + a blank line */
    (void)sprintf(buffer,    "HTTP/1.1 200 OK\n"
                            "\tServer: beechfuzz/nweb v%.1f\n"
                            "\tContent-Length: %ld\n"
                            "\tConnection: close\n"
                            "\tContent-Type: %s\n\n", VERSION, len, fstr);
    logger(LOG,"Header",buffer,hit);
    (void)write(fd,buffer,strlen(buffer));

    /* send file in 8KB block - last block may be smaller */
    while ( (ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
        (void)write(fd,buffer,ret);
    }

    /* allow socket to drain before signalling the socket is closed */
    sleep(1);
    close(fd);

    exit(1);
}


/***************************************************************
***************************************************************
**
**  MAIN
**  ----
*/

int main(int argc, char **argv){

    int opt, pid, listenfd, socketfd, hit;
    socklen_t length;
    static struct sockaddr_in cli_addr;  /* static = initialised to zeros */
    static struct sockaddr_in serv_addr; /* static = initialised to zeros */
    char cwd[PATH_MAX];

    /* Set argument defaults */
    int daemonize = 0;
    int port = 80;
    char *logfile = "/var/log/nweb/nweb.log";
    char *rootdir = getcwd(cwd,sizeof(cwd));

    /* Parse arguments */
    while((opt = getopt(argc, argv, ":dhl:p:r:")) != -1){
        switch(opt){
            case 'd':
                daemonize = 1;
                break;
            case 'h':
                help();
                break;
            case 'l':
                logfile = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                if ( port < 0 || port > 60000 ){
                    printf("\nERROR: Invalid port number (try 1->60000)\n");
                    exit(3);
                }
                break;
            case 'r':
                rootdir = optarg;
                if(chdir(rootdir) == -1){
                    printf("ERROR: Can't open specified root directory '%s'\n",rootdir);
                    exit(4);
                }
                break;
            case ':':
                printf("option needs a value\n");
                break;
            case '?':
                printf("unknown option: %c\n", optopt);
                usage();
                break;
        }
    }
    for (;optind < argc ; optind++){
        printf("Too many extra arguments!\n");
        usage();
    }

    /* Assign g_logfile global variable */
    g_logfile = (char *) malloc(strlen(logfile) + 1);
    strcpy(g_logfile,logfile);

    /* Run as daemon if specified */
    if (daemonize == 1){
        runAsDaemon();
    }

	/* First log entry */
    logger(LOG, "nweb starting", (char *)&port, getpid());

    /* Setup the network socket */
    if ((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0){
        logger(ERROR, "system call","socket",0);
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0){
        logger(ERROR,"system call","bind",0);
    }
    if( listen(listenfd,64) <0){
        logger(ERROR,"system call","listen",0);
    }
    for(hit=1; ;hit++) {
        length = sizeof(cli_addr);
        if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0){
            logger(ERROR,"system call","accept",0);
        }
        if((pid = fork()) < 0) {
            logger(ERROR,"system call","fork",0);
        } else {
            /* child */
            if(pid == 0) {
                (void)close(listenfd);
                /* never returns */
                web(socketfd,hit);
            /* parent */
            } else {
                (void)close(socketfd);
            }
        }
    }

    /* Free up our resources */
    free((void *) g_logfile);
}
