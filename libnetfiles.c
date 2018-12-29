#include "libnetfiles.h"
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <unistd.h>

extern int h_errno;

#define PORT 32767
#define INVALID_FILE_MODE 50

int fileconnectionmode = -1;
char hostbuffer[256];
char* IPbuff;


int netserverinit(char* hostname, int filemode){
	printf("netserverinit \n");
	memcpy(hostbuffer,hostname,strlen(hostname));
	if(gethostbyname(hostname) == NULL){
		h_errno = HOST_NOT_FOUND;
		printf("%s\n", hstrerror(h_errno));
		return -1;
	}

	else if(filemode < 0 || filemode > 2){
		h_errno = INVALID_FILE_MODE;
		printf("%d %s\n", INVALID_FILE_MODE, "Invalid File Mode");
		return -1;
	}

	// find IP 4 all nets
    	struct hostent *host;
    	host = gethostbyname(hostbuffer);
    	IPbuff = inet_ntoa(*((struct in_addr*) host->h_addr_list[0]));

    	printf("Hostname: %s\n", hostbuffer);
    	printf("IP: %s\n", IPbuff);
	
	fileconnectionmode = filemode; //sets global variable so other net functions can access it
	//printf("Success\n");
	return 0;
}








// ------------------------NETOPEN-----------------------------------

int netopen(const char*pathname, int flags){
	printf("\n netopen \n");
	// printf("%d\n", flags); has to = 0,1,or2
	// if netserverinit has not been run or host not found

	if(fileconnectionmode == -1){
		h_errno = HOST_NOT_FOUND;
		printf("%s \n" ,hstrerror(h_errno));
		return -1;
	} 	


    //concatenate file connection mode, pathname, and flag
    
    char fullstring[sizeof(pathname)+sizeof(char*)*4];
    char* filemode;
    char* flag;     

    if(fileconnectionmode == 1){
    	filemode = "1";
    }
    if(fileconnectionmode == 2){
    	filemode = "2";
    }
    if(fileconnectionmode == 0){
    	filemode = "0";
    }
    if(flags == O_RDONLY){
    	flag = "0";
    }
    if(flags == O_WRONLY){
    	flag = "1";
    }
    if(flags == O_RDWR){
    	flag = "2";
    }

    strcpy(fullstring,"0"); // or we can put a digit for simplicity
    strcat(fullstring,",");
    strcat(fullstring,filemode);
    strcat(fullstring,",");
    strcat(fullstring, pathname);
    strcat(fullstring,",");
    strcat(fullstring, flag);

   // printf("msg to server: %s\n", fullstring);


   //create socket
	int netsocket = socket(AF_INET, SOCK_STREAM, 0);
	if(netsocket < 0){
		printf("Socket error in client \n");
		return -1;
	}
	
	//connect to fileserver
	struct sockaddr_in servaddress;
	memset(&servaddress, '0', sizeof(servaddress));
	
	servaddress.sin_family = AF_INET;
	servaddress.sin_port = htons(PORT);

	if(inet_pton(AF_INET, IPbuff, &servaddress.sin_addr) <= 0){
		printf("inet_pton error\n");
		return -1;
	}

	int connectstat = connect(netsocket, (struct sockaddr*) &servaddress, sizeof(servaddress));

	//if couldn't connect
	if(connectstat == -1){
		printf("Error connecting to server!\n");
	}

	//got a connection
 	
 	//send fileconnectionmode, pathname, flag
	send(netsocket, fullstring, strlen(fullstring), 0);
	//free(fullstring);
 

	//return file description and set errno (if any)
	char sresponse[256];
	recv(netsocket, sresponse, sizeof(sresponse), 0);

	//printf("Server response: %s\n", sresponse);
	
	char ** resp = tokenizer(sresponse,1);
	//printf("0: %d & 1: %d \n", atoi(resp[0]),atoi(resp[1]));
	//printf("res[0] %d \n", resp[1]);
	if(atoi(resp[0]) == -1){
		errno = atoi(resp[1]);
		printf("%s \n", strerror(errno));
		//printf("done with open \n \n");
		freee(resp);
		return -1;
	}
	
	else if(atoi(resp[0]) == 1){
		//printf("done with open \n \n");
		int a = atoi(resp[1]);
		freee(resp);		
		return a;
		
	}
	else{
		freee(resp);
	}
	return -1;
}








// ------------------------NETREAD-----------------------------------

ssize_t netread(int fildes, void*buf, size_t nbyte){
	printf("\n netread \n");
	// if netserverinit has not been run or host not found
	if(fileconnectionmode == -1){
		h_errno = HOST_NOT_FOUND;
		printf("%s \n", hstrerror(h_errno));
		return -1;
	} 	

    //concatenate read, filedescriptor, & # of bytes
    
    char fullstring[sizeof(fildes)+sizeof(nbyte)+3];
    char fildes2[(int)((ceil(log10(5))+1)*sizeof(char))];
    char nbyte2[(int)((ceil(log10(5))+1)*sizeof(char))];
    sprintf(nbyte2, "%d", (int)nbyte);
    sprintf(fildes2, "%d", fildes);

    strcpy(fullstring,"1"); 
    strcat(fullstring,",");
    strcat(fullstring, fildes2);
    strcat(fullstring,",");
    strcat(fullstring, nbyte2);
    
    //printf("msg to server: %s\n", fullstring);


   //create socket
	int netsocket = socket(AF_INET, SOCK_STREAM, 0);
	if(netsocket < 0){
		errno = ETIMEDOUT;			
		printf("%s\n", strerror(errno));
		return -1;
	}
	
	//connect to fileserver
	struct sockaddr_in servaddress;
	memset(&servaddress, '0', sizeof(servaddress));
	
	servaddress.sin_family = AF_INET;
	servaddress.sin_port = htons(PORT);

	if(inet_pton(AF_INET, IPbuff, &servaddress.sin_addr) <= 0){
		printf("inet_pton error\n");
		return -1;
	}

	int connectstat = connect(netsocket, (struct sockaddr*) &servaddress, sizeof(servaddress));

	//if couldn't connect
	if(connectstat == -1){
		errno = ECONNRESET;		
		printf("%s \n", strerror(errno));
	}

	//got a connection
 	
 	//send fileconnectionmode, pathname, flag
	send(netsocket, fullstring, strlen(fullstring), 0);
	
	//return file description and set errno (if any)
	char sresponse [2000];
	recv(netsocket, sresponse, 2000, 0);
	//sresponse[strlen(sresponse)+1] = ' ';
	
	//printf("%d\n", (int)strlen(sresponse));
	//printf("Server response: %s\n", sresponse);
	
	char ** resp = tokenizer1(sresponse);
	//free(sresponse);
	//printf("0: %d & 1: %d \n", atoi(resp[0]),atoi(resp[1]));
	//printf("res[0] %d \n", resp[1]);
	
	if(atoi(resp[0]) == -1){
		char ** resp1 = tokenizer(sresponse,1);		
		errno = atoi(resp[1]);
		//printf("text: %s \n", resp[2]);		
		strcpy(buf,resp[2]);			
		printf("%s \n", strerror(errno));
		freee(resp1);
		freee(resp);
		return nbyte;
	}
	
	
	else if(atoi(resp[0]) == 1){
		char ** resp2 = tokenizer(sresponse,2);
		strcpy(buf,resp[2]);		
		//memcpy(buf,resp[2],nbyte);	
		int b = atoi(resp[1]);		
		freee(resp);
		freee(resp2);	
		return b;
	}
	
	else{
		freee(resp);
	}
	return -1;

}


// ------------------------NETWRITE-----------------------------------

ssize_t netwrite(int fildes,const void *buf, size_t nbyte){
	printf("\n netwrite \n");
	if(fileconnectionmode == -1){
		errno = HOST_NOT_FOUND;
		return -1;
	} 
	// convert void buf to string
	 const char* text;
	 text = (char*)buf; 
	 text = buf;

	 // convert to string
    	char fildes2[(int)((ceil(log10(5))+1)*sizeof(char))];
   	char nbyte2[(int)((ceil(log10(5))+1)*sizeof(char))];
   	sprintf(nbyte2, "%d", (int)nbyte);
   	sprintf(fildes2, "%d", fildes);
	
	char fullstring[sizeof(text)+sizeof(fildes)+sizeof(nbyte)+8];
	
	strcpy(fullstring, "2");
	strcat(fullstring, ",");
	strcat(fullstring,fildes2);
	strcat(fullstring,",");
	strcat(fullstring, nbyte2);
	strcat(fullstring, ",");
	strcat(fullstring, text);
	
	//printf("msg to server: %s \n", fullstring);
   
   //create socket
	int netsocket = socket(AF_INET, SOCK_STREAM, 0);
	if(netsocket < 0){
		printf("Socket error in client \n");
		return -1;
	}
	
	//connect to fileserver
	struct sockaddr_in servaddress;
	memset(&servaddress, '0', sizeof(servaddress));
	
	servaddress.sin_family = AF_INET;
	servaddress.sin_port = htons(PORT);

	if(inet_pton(AF_INET, IPbuff, &servaddress.sin_addr) <= 0){
		printf("inet_pton error\n");
		return -1;
	}

	int connectstat = connect(netsocket, (struct sockaddr*) &servaddress, sizeof(servaddress));

	//if couldn't connect
	if(connectstat == -1){
		printf("Error connecting to server!\n");
	}

	//got a connection
 	
 	//send fileconnectionmode, pathname, flag
	send(netsocket, fullstring, strlen(fullstring), 0);
	
// ----------------- FINISH REST to parse errno from response----------------

	//return file description and set errno (if any)
	char sresponse[256];
	recv(netsocket, sresponse, sizeof(sresponse), 0);

	//printf("Server response: \n %s\n", sresponse);

	char ** resp = tokenizer(sresponse,1);
	//printf("0: %d & 1: %d \n", atoi(resp[0]),atoi(resp[1]));
	//printf("res[0] %d \n", resp[1]);
	
	if(atoi(resp[0]) == -1){
		errno = atoi(resp[1]);	
		printf("%s \n", strerror(errno));
		freee(resp);
		return -1;
	}
	
	else if(atoi(resp[0]) == 1){
		int c = atoi(resp[1]);		
		freee(resp);
		return c;
	}
	else{
		freee(resp);
	}	
	return -1;

	
}










// ------------------------NETCLOSE-----------------------------------

int netclose(int fd){
	printf("\n netclose \n");
	// if netserverinit has not been run or host not found
	if(fileconnectionmode == -1){
		errno = HOST_NOT_FOUND;
		return -1;
	} 	

    // -- store nbyte in buf ***** ---- still need 2 figure this out --------- *******
    //memcpy(buf, &nbyte, nbyte);
    //printf("hello: %d \n", buf);

    //concatenate read, filedescriptor, & # of bytes
    
    char fullstring[sizeof(fd)+6];
    char fd2[(int)((ceil(log10(5))+1)*sizeof(char))];
    sprintf(fd2, "%d", fd);

    strcpy(fullstring,"3"); 
    strcat(fullstring,",");
    strcat(fullstring, fd2);
    
    //printf("msg to server: %s\n", fullstring);


   //create socket
	int netsocket = socket(AF_INET, SOCK_STREAM, 0);
	if(netsocket < 0){
		printf("Socket error in client \n");
		return -1;
	}
	
	//connect to fileserver
	struct sockaddr_in servaddress;
	memset(&servaddress, '0', sizeof(servaddress));
	
	servaddress.sin_family = AF_INET;
	servaddress.sin_port = htons(PORT);

	if(inet_pton(AF_INET, IPbuff, &servaddress.sin_addr) <= 0){
		printf("inet_pton error\n");
		return -1;
	}

	int connectstat = connect(netsocket, (struct sockaddr*) &servaddress, sizeof(servaddress));

	//if couldn't connect
	if(connectstat == -1){
		printf("Error connecting to server!\n");
	}

	//got a connection
 	
 	//send fileconnectionmode, pathname, flag
	send(netsocket, fullstring, strlen(fullstring), 0);
	
// ----------------- FINISH REST to parse errno from response----------------

	//return file description and set errno (if any)
	char sresponse[256];
	recv(netsocket, sresponse, sizeof(sresponse), 0);

	//printf("Server response: \n %s\n", sresponse);
	
	char ** resp = tokenizer(sresponse,1);
	//printf("0: %d & 1: %d \n", atoi(resp[0]),atoi(resp[1]));
	//printf("res[0] %d \n", resp[1]);
	
	if(atoi(resp[0]) == -1){
		errno = atoi(resp[1]);	
		printf("%s \n", strerror(errno));
		freee(resp);
		return -1;
	}
	
	else if(atoi(resp[0]) == 0){		
		freee(resp);
		return 0;
	}
	
	else{
		freee(resp);
	}
	return -1;

}



char** tokenizer1(char* str){
	
	unsigned int size = 4;
	char** vals = malloc(sizeof(char*) * size);
	int i;
	for(i = 0; i < size; i++){
		vals[i] = NULL;
	}
	
	char s[strlen(str)];
	strcpy(s, str);
	
	char* pt = strtok(s, ",");
	char* p = malloc(sizeof(char) * strlen(pt));
	strcpy(p, pt);
	i = 0;
	while(pt != NULL){
		vals[i] = p;
		pt = strtok(NULL, ",");
		
		if(pt == NULL){
			break;
		}
		p = malloc(sizeof(char) * strlen(pt));
		strcpy(p, pt);
		i++;
	}
	
	return vals;
}


char** tokenizer(char* str, int commas){
    
	unsigned int size = 4;
	char** vals = malloc(sizeof(char*) * size);
	
	int j;
	for(j = 0; j < 4; j++){
	  vals[j] = NULL;
	}
	
	int prev = -1;
	int i;
	int idx = 0;
	
	for(i = 0; commas != 0; i++){
	  if(str[i] == ','){
	    commas--;
	    
	    char* p = malloc(i - prev);	    
	    strncpy(p, &str[prev + 1], i - prev - 1);
	    vals[idx++] = p;
	    
	    if(commas == 0){
	      prev = i;
	      p = malloc(strlen(str) - prev);
	      strncpy(p, &str[prev + 1], strlen(str) - prev - 1);
	      vals[idx++] = p;
	      break;
	    }

	    prev = i;
	  }
	}
	
	return vals;
}

// -------------------------

void freee(char ** r){
	int i;	
	for(i = 0; i < 4; i++){
		free(r[i]);
	}
	free(r);
}

//----------------------------

// ------------------------MAIN-----------------------------------

int main(){
	netserverinit("kill.cs.rutgers.edu",0);
	int fd = netopen("readme", O_RDWR); 
	char * buffer = "hello i am arti";
	netwrite(fd,buffer,5);
	//printf("%s \n", buffer);
	netclose(fd);
	return 0;
}
