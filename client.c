#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFSIZE	4096

char *key = "key";
int connectsock(char *host, char *service, char *protocol);
unsigned char stat[256];

void ksa(unsigned char state[], unsigned char key[], int len)
{
   int i,j=0,t;

   for (i=0; i < 256; ++i)
      state[i] = i;
   for (i=0; i < 256; ++i) {
      j = (j + state[i] + key[i % len]) % 256;
      t = state[i];
      state[i] = state[j];
      state[j] = t;
   }
}

// Pseudo-Random Generator Algorithm
// Input: state - the state used to generate the keystream
//        out - Must be of at least "len" length
//        len - number of bytes to generate
void prga(unsigned char state[], unsigned char out[], int len)
{
   int i=0,j=0,x,t;
   unsigned char key;

   for (x=0; x < len; ++x)  {
      i = (i + 1) % 256;
      j = (j + state[i]) % 256;
      t = state[i];
      state[i] = state[j];
      state[j] = t;
      out[x] = state[(state[i] + state[j]) % 256];
   }
}

void *read_msg(void *client) {
    char buf[BUFSIZE], c;
	int *csock = (int *) client;
	int cc, i=0, num=0;
	for (;;) {
		// Read the echo and print it out to the screen
		if ((cc = read(*csock, buf, BUFSIZE)) <= 0) {

			printf("The server has gone.\n");
			close(*csock);
			exit(-1);
		} else {
            char *msg, *comm, *tag;
            comm = strtok(buf, " ");
            msg = strtok(NULL, "\n");

            if(strcmp(comm, "MSGE")==0){

            if(msg!=NULL){
                if(msg[0]=='#'){
                    tag = strtok(msg, " ");
                    msg = strtok(NULL, "");
                }
                if(msg!=NULL){
                num = atoi(strtok(msg, "/"));
                msg = strtok(NULL, "\n");

                char out[strlen(msg)], state[256];
                ksa(state, key, strlen(key));
                prga(state, out, num);
                int i=0;
                for(i=0;i<strlen(msg);i++){
                    out[i] = out[i]^msg[i];
                }
                if(tag!=NULL){
                sprintf(buf, "%s %s",buf, tag);
                }
                sprintf(buf, "%s %d/%s\n",buf, num, out);

                tag=NULL;
                }
                else{
                if(tag!=NULL){
                sprintf(buf, "%s %s\n",buf, tag);
                }
                }
            }


		}
		else{

            if(msg!=NULL){
                sprintf(buf,"%s %s\n", buf,msg);
            }
		}
			printf("%s\n", buf);
		}
	}
	pthread_exit(NULL);
}

void *write_msg(void *client) {
    char buf[BUFSIZE];
	int *csock = (int *) client;
    int num=0;
	unsigned char out[BUFSIZE];
	while (fgets(buf, BUFSIZE, stdin) != NULL) {
        char *tag, *msg, *comm;
        buf[strlen(buf)]=='\0';
		if (buf[0] == 'q' || buf[0] == 'Q') {
			printf("Shutting down the client.\n");
			close(*csock);
			exit(-1);
		}
		comm = strtok(buf, " ");
		msg = strtok(NULL, "\n");


		if(strcmp(comm, "MSGE")==0){
            if(msg!=NULL){
                if(msg[0]=='#'){
                    tag = strtok(msg, " ");
                    msg = strtok(NULL, "");
                }
                if(msg!=NULL){
                num = atoi(strtok(msg, "/"));
                msg = strtok(NULL, "\n");

                char out[strlen(msg)], state[256];
                ksa(state, key, strlen(key));
                prga(state, out, num);
                int i=0;
                for(i=0;i<strlen(msg);i++){
                    out[i] = out[i]^msg[i];
                }
                if(tag!=NULL){
                sprintf(buf, "%s %s",buf, tag);
                }
                sprintf(buf, "%s %d/%s\n",buf, num, out);

                tag=NULL;
                }
                else{
                if(tag!=NULL){
                sprintf(buf, "%s %s\n",buf, tag);
                }
                }
            }


		}
		else{

            if(msg!=NULL){
                sprintf(buf,"%s %s\n", buf,msg);
            }
		}

		// Send to the server
		if (write(*csock, buf, strlen(buf)) < 0) {
			fprintf(stderr, "client write: %s\n", strerror(errno));
			exit(-1);
		}
	}
	pthread_exit(NULL);
}



/*
**	Client
*/
int main(int argc, char *argv[]) {
	char *service;
	char *host = "localhost";
	int csock;
	int status;

	switch(argc) {
		case 2:
			service = argv[1];
			break;
		case 3:
			host = argv[1];
			service = argv[2];
			break;
		default:
			fprintf(stderr, "usage: chat [host] port\n");
			exit(-1);
	}

	/*	Create the socket to the controller  */
	if ((csock = connectsock(host, service, "tcp")) == 0) {
		fprintf(stderr, "Cannot connect to server.\n");
		exit(-1);
	}

	printf("The server is ready, please start sending to the server.\n");
	printf("Type q or Q to quit.\n");
	fflush(stdout);

	pthread_t read_thread, write_thread;

	status = pthread_create(&read_thread, NULL, read_msg, (void *)&csock);
	if (status != 0) {
		printf("Error creating thread -  %d.\n", status);
		exit(-1);
	}
	status = pthread_create(&write_thread, NULL, write_msg, (void *)&csock);
	if (status != 0) {
		printf("Error creating thread -  %d.\n", status);
		exit(-1);
	}
	pthread_join(read_thread, NULL);
	pthread_join(write_thread, NULL);

	close(csock);

	return 0;
}
