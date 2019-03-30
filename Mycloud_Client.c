#include "csapp.h"
#include <stdio.h>
#include <unistd.h>
#define MAXARGS 128
#define MAXSIZE (1024*100) //Max buffer size 100k bytes


int fd;  
rio_t rio;
int seqNum = 1;

int parseline(char *buf, char **argv);  //tokenize user input
void processBasicResponse();  //most server response have a common form, designed to handle this response
void eval(char *cmdline);  //given user input, sends off to parseline then evaluates proper action
void hello(unsigned int key);  //handles initial handshake
void evalCP (char **argv);  //cluster of all copy subactions
void evalRM (char **argv);// cluster of all remove subactions
void evalList (char **argv);  //processes list command

struct message_hello {
    int seqno;
    unsigned int type;
    unsigned int key;
} s_hello;

struct message_basic_response {
    int seqno;
    int status;
} s_basicResponse;

struct message_remove {
    int seqno;
    unsigned int type;
    char filename[40];  //name of server file to remove
} s_remove;

struct message_copy {
    int seqno;
    unsigned int type;
    char filename1[40];  //name of server file to copy
    char filename2[40];  //name of new server file to copy to
} s_copy;

struct message_store {
    int seqno;
    unsigned int type;
    char filename[40];  //name place on new file being sent to server
    unsigned int filesize;  //size of the file to be read
    char buffer[MAXSIZE]; //the binary data of the file
} s_store, s_retrieve;  //retrive command uses this structure for message but only first 48 bytes



int main(int argc, char **argv) 
{
    char *host, *port, cmdline[MAXLINE];
    if (argc != 4) {   //ensure run arguements are valid size
	fprintf(stderr, "usage: %s <host> <port> <key>\n", argv[0]);
	exit(0);
    }
    host = argv[1];
    port = argv[2];

    fd = Open_clientfd(host, port);
    Rio_readinitb(&rio, fd);

    hello(atoi(argv[3]));

    while (1) {
        printf("> ");                   
        Fgets(cmdline, MAXLINE, stdin); 
        if (feof(stdin))
            exit(0);

        eval(cmdline);  //tokenize user input
    }

    Close(fd); //line:netp:echoclient:close
    exit(0);
}
/* $end echoclientmain */

void processBasicResponse(){
    Rio_readn(fd, &s_basicResponse, 8);  //capture first 8 bytes of message
    if(s_basicResponse.status == -1)  //if status value is -1, it failed
        printf("Command Fail.\n"); //chose to not make std error to match style of success
    else{ //if not -1, success, technically should be 0, but since server code is in control, these extra states shouldnt occur
        printf("Command Success.\n");
    }
    seqNum = s_basicResponse.seqno + 1;  //new seqNum is 1 more than seqNum that was returned
}

void hello(unsigned int key){  //only give it the secret key as fd is global
    s_hello.seqno = seqNum;
    s_hello.type = 1;  //hello message id
    s_hello.key = key;
    Rio_writen(fd, &s_hello, 12);  //send 12 bytes
    Rio_readn(fd, &s_basicResponse, 8);  //read back 8 byte response
    if(s_basicResponse.status == -1){  //if command failed, code key was wrong
        printf("Secret code incorrect, connection rejected.\n");
        exit(0);
    }
    printf("Connection accepted.\n");  //since if terminates no need for logic to control else
    seqNum = s_basicResponse.seqno + 1;
}

void eval(char *cmdline) 
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int argc;

    strcpy(buf, cmdline);
    argc = parseline(buf, argv);   //parse user input and get number of arguments into argc
    if (argv[0] == NULL)    
        return;   /* Ignore empty lines */
    else if (!strcmp(argv[0], "quit")){ /* quit command */
        Close(fd);
        exit(0);  
    }
    else if (!strcmp(argv[0], "cp")){  //if command recognized as cp
        if(argc != 3){  //check to make sure there are exactly 3 args
            fprintf(stderr, "Number of args should be 3, found %i.\n",argc);
            return;
        }
        evalCP(argv); //run helper               
    }
    else if (!strcmp(argv[0], "rm")){ //if command recognized as remove
        if(argc != 2){  //ensure exactly 2 args
            fprintf(stderr, "Number of args should be 2, found %i.\n",argc);
            return;
        }
        evalRM(argv);   //run helper      
    }
    else if (!strcmp(argv[0], "list")){ //if command is list
        if(argc != 1) //complain about extra args but just ignore them
            fprintf(stderr, "Ignoring extra arguements.\n");
        evalList(argv); //run helper 
    }
    else{  //if not one of the above commands, print error
        fprintf(stderr, "Command not recognized!\n");  
    }
}

void evalCP (char **argv){
    int cpOption=0;  
    if(argv[1][0] == 'c' && argv[1][1] == ':') //binary mapping of argument type, having c: in first make it option 2 or 3
        cpOption = cpOption + 2;
    if(argv[2][0] == 'c' && argv[2][1] == ':')//binary mapping of arg type, hacing c: in second makes it option 0 or 1
        cpOption = cpOption + 1;
    if(cpOption == 0){ //if no c:, local copy
        FILE *fSource, *fDest;  //define file for both source and dest
        fSource = fopen( argv[1], "rb" );  //open source
        char buffer[MAXSIZE];
        size_t bytes;
        if (fSource!=NULL){ //if file opened successfully
            fDest = fopen(argv[2], "wb");  //open dest
            bytes = fread(buffer, 1, sizeof(buffer), fSource);  //read file binary into buffer and capture size
            fwrite(buffer, 1, bytes, fDest);  //write buffer to dest
            fclose(fSource);//close files
            fclose(fDest);
        }  
        else{
            fprintf(stderr, "File to copy does not exist.\n");  //if source file does not exist
        }       
    }
    else if(cpOption == 1){  //store
        s_store.type = 2;
        FILE *fSource;
        fSource = fopen( argv[1], "rb" );  //attempt to open source
        if (fSource!=NULL){ //if file was able to be openned
            s_store.filesize = fread(s_store.buffer, 1, sizeof(s_store.buffer), fSource);  //set size in message to size of read in file
            s_store.seqno = seqNum; 
            strcpy(s_store.filename,argv[2]+2);  //copy the name of file starting at index 2, ignore c:
            Rio_writen(fd, &s_store, 52+s_store.filesize);  //sent message to server, size is 52 bytes + whatever the size of the file
            fclose(fSource);//close file
            processBasicResponse();  //response is basic
        } 
        else{
            fprintf(stderr, "File to store does not exist.\n");  //if source file does not exist
        }  
    }
    else if(cpOption == 2){ //retrieve
        s_retrieve.type = 3;
        s_retrieve.seqno = seqNum;
        strcpy(s_retrieve.filename,argv[1]+2);  //save to message the file wanting to be retrieved
        Rio_writen(fd, &s_retrieve, 48);  //send message to server
        Rio_readn(fd, &s_basicResponse, 8);   //read first 8 bytes to determine whether op succeeded
        if(s_basicResponse.status == -1)  //if failed, print fail
            printf("Command Fail.\n");
        else{ 
            printf("Command Success.\n");  //success
            char buffer[MAXSIZE];
            size_t bytes;
            FILE *fDest;
            fDest = fopen(argv[2], "wb");  //open blank file
            Rio_readn(fd, &bytes, 4);  //get size of file retrieved
            Rio_readn(fd, &buffer, (int)bytes);  //read number of bytes equal to size of file
            fwrite(buffer, 1, bytes, fDest);  //write this buffer to open dest file
            fclose(fDest);
        }
        seqNum = s_basicResponse.seqno + 1;
    }
    else{  //cloud copy
        s_copy.seqno = seqNum; 
        s_copy.type = 4;
        strcpy(s_copy.filename1,argv[1]+2);  //put into message names of the 2 files
        strcpy(s_copy.filename2,argv[2]+2);
        Rio_writen(fd, &s_copy, 88);
        processBasicResponse();
    }
}

void evalRM (char **argv){
    if(argv[1][0] == 'c' && argv[1][1] == ':'){ //if file to remove is on the cloud
        s_remove.seqno = seqNum; 
        s_remove.type = 6;
        strcpy(s_remove.filename,argv[1]+2);  //save name of file to message
        Rio_writen(fd, &s_remove, 48);  //send message to server
        processBasicResponse();  //response is basic
    }
    else{   //if local remove
        int ret;
        ret = remove(argv[1]);  //remove the file by this name
        if(ret == 0) {  //if remove succeeded
            printf("File deleted successfully\n");
        } 
        else {  //if remove failed
            fprintf(stderr,"Error: that file does not exist.\n");
        }
    }
}

void evalList (char **argv){   
    s_hello.type = 5;
    s_hello.seqno = seqNum; 
    Rio_writen(fd, &s_hello, 8);  //send message to server
    Rio_readn(fd, &s_basicResponse, 8);  //read first 8 bytes of server response
    if(s_basicResponse.status == -1)  //if failed print error and stop
        printf("Command Fail.\n");
    else{
        printf("Command Success.\n\n");  //success
        int elements;
        Rio_readn(fd, &elements, 4);  //read the number of elements in the list
        if(elements == 0)  //to avoid confusion when nothing is shown because list is empty, print message
            printf("Folder is empty!\n");
        for (int i = 0; i < elements; i++){  //for each elements
            char buf[40];  //prepare a buffer for filename
            Rio_readn(fd, &buf, 40);  //read 40 bytes
            printf("%s\n",buf);  //print the buffer
        }
    }
    seqNum = s_basicResponse.seqno + 1;
} 

int parseline(char *buf, char **argv) //function core copied from Project 3
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */


    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        if(buf[0] == '%') //If first character of buf is a %, break loop which will ignore rest of the line
            break;
         
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
                buf++;
    }
    argv[argc] = NULL;
    return argc;
}