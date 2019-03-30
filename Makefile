All: Mycloud_Client.c Mycloud_Server.c csapp.c csapp.h
	gcc -o client Mycloud_Client.c csapp.c -lpthread
	gcc -o server Mycloud_Server.c csapp.c -lpthread

