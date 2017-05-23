#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <pthread.h>
#include <semaphore.h>

#define	QLEN			5
#define	BUFSIZE			2048

int passivesock( char *service, char *protocol, int qlen, int *rport );

/*
**	The server ...
*/


typedef struct _hashtag{
    char* name;
    int size;
    struct _hashtag *next;
    fd_set set;
}hashtag;

typedef struct _msgInfo{
    char *msg;
    int socket, size;
}msgInfo;

hashtag *first;
int			nfds;
sem_t *socks_sem;
pthread_t *writeThread;




void *msgSend(void *arg){
    msgInfo *data = (msgInfo*)arg;
    sem_wait(&socks_sem[data->socket]);

    write(data->socket, data->msg, data->size);

    sem_post(&socks_sem[data->socket]);
    free(data->msg);
    free(arg);
    pthread_exit(NULL);

}

void add(int fd, char* tag, hashtag *curr ){
    if(curr->name==NULL){   //If we came to an end...
        curr->name = (char*) malloc(sizeof(char)*(strlen(tag)+1));
        strcpy(curr->name, tag);
        FD_ZERO(&curr->set);
        FD_SET(fd, &curr->set);
        curr->size=1;
        curr->next=NULL;
    }
    else if(strcmp(curr->name, tag)==0){    //If found...
        if(FD_ISSET(fd, &curr->set)==0){
            curr->size+=1;
            FD_SET(fd, &curr->set);
            if(FD_ISSET(fd, &curr->set)!=0){

            }
        }
    }
    else{
        if(curr->next==NULL){
            curr->next = (hashtag*) malloc(sizeof(hashtag));
            curr->next->name = NULL;
        }
        add(fd, tag, curr->next);
    }
}

void removeSockFromAll(int fd, hashtag *start){
    hashtag *curr=start, *prev=NULL;
    while(curr!=NULL){
        if(FD_ISSET(fd, &curr->set)!=0){
            FD_CLR(fd, &curr->set);
            curr->size-=1;
            if(curr->size==0){//Delete tag
                prev->next=curr->next;
                free(curr->name);
                free(curr);
                curr=prev;
            }
        }
        prev=curr;
        curr=curr->next;
    }
}

void removeByTag(int fd, char* tag, hashtag *start){
    hashtag* curr=start, *prev=NULL;
    while(curr!=NULL){
        if(strcmp(curr->name, tag)==0){//Found
            if(FD_ISSET(fd, &curr->set)!=0){//Delete
                FD_CLR(fd, &curr->set);
                curr->size-=1;
                if(curr->size==0){//Delete tag
                    prev->next=curr->next;
                    free(curr->name);
                    free(curr);
                }
            }
            break;
        }
        else{
            prev=curr;
            curr=curr->next;
        }
    }
}

void writeMsg(char *tag, char* message, hashtag *start, int size){
    hashtag *curr=start;
    while(curr!=NULL){
        if(strcmp(curr->name, tag)==0){
            for(int i=0; i<nfds; i++){
                if(FD_ISSET(i, &curr->set)!=0){

                        msgInfo *newInfo = malloc(sizeof(msgInfo));
                        newInfo->msg = malloc(sizeof(char)*size);
                        for(int j=0; j<size; j++){
                            newInfo->msg[j] = message[j];
                        }
                        newInfo->socket=i;
                        newInfo->size = size;
                        pthread_create(&writeThread, NULL, msgSend, newInfo);

                }
            }
            break;
        }
        else{
            curr=curr->next;
        }
    }
}

void printTags(hashtag *curr){
    if(curr!=NULL){
        printf("%s Size:%d\n", curr->name, curr->size);
        printTags(curr->next);
    }
}



int
main( int argc, char *argv[] )
{
	char			buf[BUFSIZE+1];
	char			*service, *command, *message, *tag;
	struct sockaddr_in	fsin;
	int			msock;
	int			ssock;
	fd_set			rfds;
	fd_set			afds;
	int			alen;
	int			fd;

	int			rport = 0;
	int			cc;

	switch (argc)
	{
		case	1:
			// No args? let the OS choose a port and tell the user
			rport = 1;
			break;
		case	2:
			// User provides a port? then use it
			service = argv[1];
			break;
		default:
			fprintf( stderr, "usage: server [port]\n" );
			exit(-1);
	}

	msock = passivesock( service, "tcp", QLEN, &rport );
	if (rport)
	{
		//	Tell the user the selected port
		printf( "server: port %d\n", rport );
		fflush( stdout );
	}


    first = (hashtag*)malloc(sizeof(hashtag));
    first->name = (char*)malloc(sizeof(char)*3);
    sprintf(first->name, "ALL");
    first->name="ALL";
    first->size=1;
    FD_ZERO(&first->set);
    first->next=NULL;


	nfds = getdtablesize();

	FD_ZERO(&afds);
	FD_SET( msock, &afds );

    socks_sem = (sem_t*)malloc(sizeof(sem_t) * nfds);
    for(int i=0; i<nfds; i++){
        sem_init(&socks_sem[i], 0, 1);
    }

	for (;;)
	{
		memcpy((char *)&rfds, (char *)&afds, sizeof(rfds));

		if (select(nfds, &rfds, (fd_set *)0, (fd_set *)0,
				(struct timeval *)0) < 0)
		{
			fprintf( stderr, "server select: %s\n", strerror(errno) );
			exit(-1);
		}

		/*	Handle the main socket - a new guy has checked in  */
		if (FD_ISSET( msock, &rfds))
		{
			int	ssock;

			alen = sizeof(fsin);
			ssock = accept( msock, (struct sockaddr *)&fsin, &alen );
			if (ssock < 0)
			{
				fprintf( stderr, "accept: %s\n", strerror(errno) );
				exit(-1);
			}

			/* start listening to this guy */
			FD_SET( ssock, &afds );
		}

		/*	Handle the participants requests  */
		for ( fd = 0; fd < nfds; fd++ )
		{
			if (fd != msock && FD_ISSET(fd, &rfds))
			{
				if ( (cc = read( fd, buf, BUFSIZE )) <= 0 )
				{

					printf( "The client has gone.\n" );
					(void) close(fd);
					FD_CLR( fd, &afds );
                    removeSockFromAll(fd, first);
                    //printTags(first);
				}
				else
				{
				    //Copying the message and saving original one for later
                    buf[cc]='\0';
                    char* copy = (char*)malloc(sizeof(char)*cc);
                    strcpy(copy, buf);


                    //Comparing the copy to the possible commands
                    if(strcmp(copy, "REGISTERALL\r\n")==0){
                        removeSockFromAll(fd, first);
                        add(fd, "ALL", first);

                    }else if(strcmp(copy, "DEREGISTERALL\r\n")==0){
                        removeSockFromAll(fd, first);

                    }//If it is not a single word command, we split it
                    else{


                        if(strncmp(buf, "REGISTER", 8)==0){

                            command = strtok(copy, " ");
                            tag = strtok(NULL, "\r\n");

                            if(FD_ISSET(fd, &first->set)==0){//If it is not registered for ALL...
                            add(fd, tag, first);
                            }
                        }
                        else if(strncmp(buf, "DEREGISTER", 10)==0){

                            command = strtok(copy, " ");
                            tag = strtok(NULL, "\r\n");


                                removeByTag(fd, tag, first);

                        }
                        else if(strncmp(buf, "MSG", 3)==0 && buf[3]!='E') {


                        command = strtok(copy, " ");
                        tag = strtok(NULL, "\r\n");

                            if(tag[0]=='#'){//"we have a tag"
                                tag = strtok(tag, "#");
                                tag = strtok(tag, " ");


                                writeMsg("ALL", buf, first, cc);
                                writeMsg(tag, buf,first->next, cc);
                            }
                            else{
                                writeMsg("ALL",buf, first, cc);
                            }
                        }
                        else if(strncmp(buf, "MSGE", 4)==0){
                            command = strtok(copy, " ");
                        tag = strtok(NULL, "");

                            if(tag[0]=='#'){//"we have a tag"
                                tag = strtok(tag, "#");
                                tag = strtok(tag, " ");


                                writeMsg("ALL", buf, first, cc);
                                writeMsg(tag, buf,first->next, cc);
                            }
                            else{
                                writeMsg("ALL",buf, first, cc);
                            }

                        }
                        else if(strncmp(buf, "IMAGE", 5)==0){
                                    tag=NULL;
                                    strtok(copy, " ");
                                    command = strtok(NULL, "/");
                                    if(command[0]=='#'){
                                        tag = strtok(command, "#");
                                        tag = strtok(tag, " ");
                                        command = strtok(NULL, "");

                                    }
                                    int size = atoi(command);

                                    int i=0, off=strlen(buf);

                                    char *msg = malloc(sizeof(char) * size);
                                    sprintf(msg, "%s", buf);
                                    writeMsg("ALL", buf, first,cc);
                                    if(tag!=NULL){
                                        writeMsg(tag, buf,first,cc);
                                    }
                                    int count=0, j;
                                    while(i<size){
                                        char c[1];
                                        j = read(fd, c, 1);
                                        count+=j;
                                        msg[i] = c[0];
                                        i++;

                                    }

                                    writeMsg("ALL", msg, first, size);
                                    if(tag!=NULL){
                                        writeMsg(tag, msg, first, size);
                                    }


                        }


                    }






				}
				  printTags(first);
			}

		}
	}
}

