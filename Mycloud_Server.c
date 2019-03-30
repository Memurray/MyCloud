#include "csapp.h"
#include <stdio.h>
#include <dirent.h>
#define MAXSIZE (1024*100)  //max file size 100k bytes

int secretkey;

void hello(int fd);
void command_Delete(int fd);
void command_List(int fd);
void command_Copy(int fd);
void command_Store(int fd);
void command_Retrieve(int fd);

struct message_hello {
    int seqno;
    unsigned int type;
    unsigned int key;
} s1;

struct message_basic_response {
    int seqno;
    int status;
} s_basicResponse;

struct message_list {
    int seqno;
    int status;
    int numFiles;
    char dirList [40][40];  //preallocate list, room for 40 elements of 40 character filenames
} s_List;

struct message_retrieve {
    int seqno;
    int status;
    unsigned int filesize; //size of file retrieved
    char buffer[MAXSIZE];  //buffer of up to 100k bytes
} s_retrieve;



int main(int argc, char **argv) 
{
    int listenfd, fd;
    rio_t rio;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */  //line:netp:echoserveri:sockaddrstorage
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 3) {
	fprintf(stderr, "usage: %s <port>  <secretkey>\n", argv[0]);
	exit(0);
    }
    secretkey = atoi(argv[2]);
    listenfd = Open_listenfd(argv[1]);  //listen to port
    while (1) {
    	clientlen = sizeof(struct sockaddr_storage); 
    	fd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  //default accept connection
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE,client_port, MAXLINE, 0);
        Rio_readinitb(&rio, fd);
        hello(fd); //run hello op to determine whether connection will continue
    }
    exit(0);
}


void hello(int fd){
    Rio_readn(fd, &s1, 12);  //read 12 bytes from client
    printf("Seq Number = %i\n", s1.seqno);
    printf("Secret Key = %i\n", s1.key);
    printf("Request Type = Hello\n");
    if(s1.key != secretkey){  //check if key sent matches, if fails
        printf("Operation Status = Error\n\n");  
        s_basicResponse.seqno = s1.seqno;  //respond to client with fail info
        s_basicResponse.status = -1;
        Rio_writen(fd, &s_basicResponse, 8);
        Close(fd);  //close connection
        return; //return to previous loop, to await a new valid connection
    }
    else{
        printf("Operation Status = Success\n\n");
        s_basicResponse.seqno = s1.seqno;  
        s_basicResponse.status = 0;
        Rio_writen(fd, &s_basicResponse, 8);  //inform client of success
    }
    while(Rio_readn(fd, &s1, 8)!=0){ //enter main connection loop, always reads the first 8 bytes from client
        printf("Seq Number = %i\n", s1.seqno);  //print the sequence number

        printf("Request Type = ");  //prep server output, second part of this line handled based on request type
        if(s1.type == 2){ //if store request
            printf("STORE\n");  
            command_Store(fd);  //run store function
        }
        else if (s1.type == 3){ //if retrieve request
            printf("RETRIEVE\n");
            command_Retrieve(fd); //run helper
        }
        else if (s1.type == 4){ //if copy request
            printf("COPY\n");
            command_Copy(fd);      //run helper
        }
        else if (s1.type == 5){  //if list request
            printf("LIST\n");
            command_List(fd);//run helper
        }
        else if (s1.type == 6){  //if delete request
            printf("DELETE\n");
            command_Delete(fd);  //run helper
        }
        else{  //this really shouldnt run, and is hopefully just for sanity
            printf("BAD\n");

        }
    }
}

void command_Store(int fd){
    char name[40];
    size_t bytes;
    char buffer[MAXSIZE];
    FILE *fDest;
    Rio_readn(fd, &name, 40);  //after first 8 byte read, next 40 is filename
    Rio_readn(fd, &bytes, 4); //next 4 bytes from user is filesize
    Rio_readn(fd, &buffer, (int)bytes);  //read from user size equal to filesize

    printf("Filename = %s\n",name);
    fDest = fopen(name, "wb");
    fwrite(buffer, 1, bytes, fDest);  //write file binary to open file
    fclose(fDest);
    printf("Operation Status = Success\n\n");
    s_basicResponse.status = 0; 
           
    s_basicResponse.seqno = s1.seqno;            
    Rio_writen(fd, &s_basicResponse, 8);  //replies to client with success message, as far as I know there isnt a reason this should fail at this step
}

void command_Retrieve(int fd){
    char name[40];
    Rio_readn(fd, &name, 40);
    FILE *fSource;
    fSource = fopen( name, "rb" );//open source file specifed by user
    printf("Filename = %s\n",name);
    if (fSource!=NULL){ //if able to open the file
        s_retrieve.filesize = fread(s_retrieve.buffer, 1, sizeof(s_retrieve.buffer), fSource); //save filesize and binary to buffer
        fclose(fSource);
        printf("Operation Status = Success\n\n");
        s_retrieve.status = 0;
        s_retrieve.seqno = s1.seqno; 
        Rio_writen(fd, &s_retrieve, 12+(int)s_retrieve.filesize); //send message back to user containing file
    }  
    else { //if unable to open file
        printf("Operation Status = Error\n\n");
        s_basicResponse.status = -1;    
        s_basicResponse.seqno = s1.seqno;         
        Rio_writen(fd, &s_basicResponse, 8);  //send back fail message
    }            
}

void command_Copy(int fd){  
    char name1[40],name2[40];
    Rio_readn(fd, &name1, 40);  //get name of source file from client
    Rio_readn(fd, &name2, 40); // get name of dest file from client
    FILE *fSource, *fDest;
    fSource = fopen( name1, "rb" );
    char buffer[MAXSIZE];
    size_t bytes;
    printf("Filenames = %s, %s\n",name1,name2);
    if (fSource!=NULL){  //if able to open source file
        fDest = fopen(name2, "wb");//open dest file
        bytes = fread(buffer, 1, sizeof(buffer), fSource);  //write file to buffer and gather filesize
        fwrite(buffer, 1, bytes, fDest);  //write buffer to new file
        fclose(fSource);
        fclose(fDest);
        printf("Operation Status = Success\n\n");
        s_basicResponse.status = 0; //success
    }  
    else { //if unable to open file
        printf("Operation Status = Error\n\n");
        s_basicResponse.status = -1; //fail
    }            
    s_basicResponse.seqno = s1.seqno;            
    Rio_writen(fd, &s_basicResponse, 8);  //send response success/fail
}

void command_Delete(int fd){
    char name[40];
    Rio_readn(fd, &name, 40);  //get name of file to delete from client
    printf("Filename = %s\n",name);
    int ret;
    ret = remove(name); //attempt to delete the file
    if(ret == 0) { //if delete success
        printf("Operation Status = Success\n\n");
        s_basicResponse.status = 0;  //success
    } 
    else { //if file not found
        printf("Operation Status = Error\n\n");
        s_basicResponse.status = -1;  //fail
    }            
    s_basicResponse.seqno = s1.seqno;            
    Rio_writen(fd, &s_basicResponse, 8); //send response
}

void command_List(int fd){ 
    s_List.numFiles = 0;
    DIR *dir;
    struct dirent *curr;
    if ((dir = opendir ("./")) != NULL) {  //directory of inspection is dir where application was run
        while ((curr = readdir (dir)) != NULL) {  //while there are still files in the current directory stream
            if((!strcmp(curr->d_name, ".")) || (!strcmp(curr->d_name, "..")))  //if file name is "." or ".."
                continue;  //ignore
            if(s_List.numFiles>39)
                break;
            strcpy(s_List.dirList[s_List.numFiles],curr->d_name);  //add current file name to the list of files to be be sent back to the client
            s_List.numFiles++; //increment filecount
        }
        closedir (dir);  //close directory stream
        printf("Operation Status = Success\n\n");
        s_List.status = 0;
        s_List.seqno = s1.seqno;            
        Rio_writen(fd, &s_List, 12+40*s_List.numFiles);  //message to client is 12 bytes + 40 per element in list
    } 
    else {  //if directory failed to open
        printf("Operation Status = Error\n\n");
        s_basicResponse.status = -1;
        s_basicResponse.seqno = s1.seqno;            
        Rio_writen(fd, &s_basicResponse, 8);  //response is just basic error
    }    
}