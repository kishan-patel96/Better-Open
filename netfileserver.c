#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>
#define PORT 32767

typedef struct node{
	char* key;
	int fd;
	int accessmode;
	int connectmode;
	struct node* next;
}node;

typedef struct hashtable{
	node** table;
	int size;
	int numkeys;
	double balancefactor;
}hashtable;

typedef struct node2{
	int key;
	char* filepath;
}node2;

typedef struct hashtable2{
	node2** table;
	int size;
	int numkeys;
	double balancefactor;
}hashtable2;

typedef struct ticklock{
	pthread_cond_t c;
	pthread_mutex_t m;
	unsigned long qhead, qtail;
}ticklock;

#define TICK_LOCK_INITIALIZER {PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER}

void* threadhandler(void* clientsocket);
char* servopen(int accessmode, int connectmode, char* path);
char* servread(int fd, size_t nbyte);
char* servwrite(int fd, char* buff, size_t nbyte);
char* servclose(int fd);
char* int2string(int n);
char** tokenizer(char* str, int commas);

hashtable* initialize(hashtable* hashtable, int size);
void cleartable(hashtable* hashtable);
unsigned long hashingfunc(char* key, hashtable* hashtable);
hashtable* insert(char* key, int fd, int accessmode, int connectmode, hashtable* hashtable);
node* getnode(char* key, hashtable* hashtable);
hashtable* delnode(node* n, hashtable* hashtable);
hashtable* rehash(hashtable* oldtable);
void freetable(hashtable* hashtable);

hashtable2* initialize2(hashtable2* hashtable2, int size);
void cleartable2(hashtable2* hashtable2);
unsigned long hashingfunc2(int key, hashtable2* hashtable2);
hashtable2* insert2(int key, char* filepath, hashtable2* hashtable2);
node2* getnode2(int key, hashtable2* hashtable2);
hashtable2* delnode2(int key, hashtable2* hashtable2);
hashtable2* rehash2(hashtable2* oldtable);
void freetable2(hashtable2* hashtable2);

void queue(char* key);
void tlock(ticklock* t);
void tunlock(ticklock* t);

char servmessage[256];
hashtable* modetable;
hashtable2* pathtable;
//static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main(){
	strcpy(servmessage, "Reached Server!");
	
	modetable = initialize(modetable, 100);
	pathtable = initialize2(pathtable, 100);
	
	int servsocket = socket(AF_INET, SOCK_STREAM, 0);
	if(servsocket == 0){
		perror("socket failed");
		return 0;
	}
	int opt = 1;
	int servopt = setsockopt(servsocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

	if(servopt == -1){
		perror("setsockopt");
		return 0;
	}

	struct sockaddr_in servaddress;

	servaddress.sin_family = AF_INET;
	servaddress.sin_port = htons(PORT);
	servaddress.sin_addr.s_addr = INADDR_ANY;

	int servbind = bind(servsocket, (struct sockaddr*) &servaddress, sizeof(servaddress));
	
	if(servbind < 0){
		perror("bind");
		return 0;
	}

	int servlist = listen(servsocket, 5);

	if(servlist < 0){
		perror("listen");
		return 0;
	}
	
	int addrlen = sizeof(servaddress);
	int clientsocket;
	
	while(1){

		clientsocket = accept(servsocket, (struct sockaddr*) &servaddress, (socklen_t*)&addrlen);

		if(clientsocket < 0){
			perror("accept failed");
			continue;
		}
		
		//sleep(1);		
		puts("connection accepted");
		
		pthread_t tid;
		int* newsock = malloc(sizeof(newsock));
		*newsock = clientsocket;
		
		if(pthread_create(&tid, NULL, threadhandler, (void*)newsock)){
			perror("failed to create thread");
			return 0;
		}
		
		if(pthread_detach(tid) != 0){
			printf("Thread detach error!\n");
		}
		//pthread_join(tid, NULL);
		//free(newsock);
	}
	
	freetable(modetable);
	freetable2(pathtable);
	
	return 0;
}

void* threadhandler(void* clientsocket){
	
	int sock = *(int*)clientsocket;
	puts("handler assigned");
	char buff[5000000];
	recv(sock, buff, sizeof(buff), 0);
	printf("%s\n", buff);
	
	int netfunc = buff[0] - '0';
	int commas;
	
	if(netfunc == 0 || netfunc == 2){
	  commas = 3;
	}
	else if(netfunc == 1){
	  commas = 2;
	}
	else{
	  commas = 1;
	}
	
	char** clientinfo = tokenizer(buff, commas);
	char* servresp = malloc(5000000);
	
	if(atoi(clientinfo[0]) == 0){
		servresp = servopen(atoi(clientinfo[3]), atoi(clientinfo[1]), clientinfo[2]);
	}
	else if(atoi(clientinfo[0]) == 1){
		servresp = servread(atoi(clientinfo[1]), atoi(clientinfo[2]));
	}
	else if(atoi(clientinfo[0]) == 2){
		servresp = servwrite(atoi(clientinfo[1]), clientinfo[3], atoi(clientinfo[2]));
	}
	else if(atoi(clientinfo[0]) == 3){
		servresp = servclose(atoi(clientinfo[1]));
	}
	else{
		printf("Invalid Netfunc!\n");
	}
	
	servresp[strlen(servresp)] = '\0';
	printf("%d\n", (int)strlen(servresp));
	printf("%s\n", servresp);
	
	int i;
	for(i = 0; i < 4; i++){
		free(clientinfo[i]);
	}
	free(clientinfo);

	memset(buff, 0, 5000000);
	
	if(send(sock, servresp, strlen(servresp), 0) == -1){
		perror("send failed");
	}
	
	printf("Server message sent\n");
	free(clientsocket);
	close(sock);
	free(servresp);
	return NULL;
}

char* servopen(int accessmode, int connectmode, char* path){
	
	int flag;
	char* response = malloc(10);

	if(accessmode == 0){
		flag = O_RDONLY;
	}
	else if(accessmode == 1){
		flag = O_WRONLY;
	}
	else if(accessmode == 2){
		flag = O_RDWR;
	}
	else{
		char* err = int2string(EINVAL);
		strcpy(response, "-1");
		strcat(response, ",");
		strcat(response, err);
		free(err);
		return response;
	}
	
	int fd;
	node* n = getnode(path, modetable);

	DIR* directory = opendir(path);
	
	if(directory != NULL && flag != 0){
		closedir(directory);
		char* err = int2string(EISDIR);
		strcpy(response, "-1");
		strcat(response, ",");
		strcat(response, err);
		free(err);
	}
	else if((fd = open(path, flag)) == -1){
		char* err = int2string(errno);
		strcpy(response, "-1");
		strcat(response, ",");
		strcat(response, err);
		free(err);
	}
	else{
		
		if(n != NULL){
			while(n != NULL){
				if((n->connectmode == 1 && (n->accessmode == 1 || n->accessmode == 2) && connectmode == 1 && (accessmode == 1 || accessmode == 2)) || n->connectmode == 2 || connectmode == 2){
					/*char* err = int2string(EACCES);
					strcpy(response, "-1");
					strcat(response, ",");
					strcat(response, err);
					free(err);
					return response;
					*/
					printf("Adding to connection queue...\n");
					queue(n->key);
					break;					
				}
				
				n = n->next;
			}
		}
		modetable = insert(path, fd, accessmode, connectmode, modetable);
		pathtable = insert2(fd, path, pathtable);
		char* sfd = int2string(fd*-1);
		strcpy(response, "1");
		strcat(response, ",");
		strcat(response, sfd);
		free(sfd);
	}
	
	return response;
}

char* servread(int fd, size_t nbyte){
	
	if(fd < 0){
		fd = fd*-1;
	}
	if(getnode2(fd, pathtable) == NULL){
		char* response = malloc(10);
		char* err = int2string(EBADF);
		strcpy(response, "-1");
		strcat(response, ",");
		strcat(response, err);
		free(err);
		return response;
	}
	
	node* n = getnode(getnode2(fd, pathtable)->filepath, modetable);
	while(n != NULL){
		if(n->fd == fd && !(n->accessmode == 0 || n->accessmode == 2)){
			char* response = malloc(10);
			char* err = int2string(EBADF);
			strcpy(response, "-1");
			strcat(response, ",");
			strcat(response, err);
			free(err);
			return response;
		}
		n = n->next;
	}
	
	char* respbuff = malloc(nbyte + 20);
	char* buff = malloc(nbyte+1);
	buff[nbyte] = '\0';
	int numread;
	
	if((numread = read(fd, buff, nbyte)) == -1){
		char* err = int2string(errno);
		strcpy(respbuff, "-1");
		strcat(respbuff, ",");
		strcat(respbuff, err);
		respbuff[strlen(respbuff)] = '\0'; //add this line
		free(buff);
		free(err);
		return respbuff;
	}
	else{
		char* nread = int2string(numread);
		strcpy(respbuff, "1");
		strcat(respbuff, ",");
		strcat(respbuff, nread);
		strcat(respbuff, ",");
		strcat(respbuff, buff);
		respbuff[strlen(respbuff)] = '\0'; //add this line
		printf("%s\n", respbuff);
		free(buff);
		free(nread);
		return respbuff;
	}
}

char* servwrite(int fd, char* buff, size_t nbyte){
  
	if(fd < 0){
		fd = fd*-1;
	}
	
	if(getnode2(fd, pathtable) == NULL){
		char* response = malloc(10);
		char* err = int2string(EBADF);
		strcpy(response, "-1");
		strcat(response, ",");
		strcat(response, err);
		free(err);
		return response;
	}
	
	node* n = getnode(getnode2(fd, pathtable)->filepath, modetable);
	while(n != NULL){
		if(n->fd == fd && !(n->accessmode == 1 || n->accessmode == 2)){
			char* response = malloc(10);
			char* err = int2string(EBADF);
			strcpy(response, "-1");
			strcat(response, ",");
			strcat(response, err);
			free(err);
			return response;
		}
		n = n->next; //add this line
	}
	
	char* respbuff = malloc(20);
	int numwrite;
	
	if((numwrite = write(fd, buff, nbyte)) == -1){
		char* err = int2string(errno);
		strcpy(respbuff, "-1");
		strcat(respbuff, ",");
		strcat(respbuff, err);
		free(err);
		return respbuff;
	}
	else{
		char* nwrite = int2string(numwrite);
		strcpy(respbuff, "1");
		strcat(respbuff, ",");
		strcat(respbuff, nwrite);
		free(nwrite);
		return respbuff;
	}
}

char* servclose(int fd){
	
	if(fd < 0){
		fd = fd*-1;
	}
	
	if(getnode2(fd, pathtable) == NULL){
		char* response = malloc(10);
		char* err = int2string(EBADF);
		strcpy(response, "-1");
		strcat(response, ",");
		strcat(response, err);
		free(err);
		return response;
	}
	
	node* curr = getnode(getnode2(fd, pathtable)->filepath, modetable);
	node* prev = NULL;
	char key[strlen(curr->key)];
	strcpy(key, curr->key);
	
	while(curr != NULL){
		if(curr->fd == fd){
			if(prev == NULL){
				
				int idx = hashingfunc(key, modetable);
				modetable = delnode(curr, modetable);
				modetable->table[idx] = NULL;
				break;
			}
			
			prev->next = curr->next;
			modetable = delnode(curr, modetable);
			break;
		}
		prev = curr;
		curr = curr->next;
	}
	
	pathtable = delnode2(fd, pathtable);
	
	char* respbuff = malloc(10);
	int numclose;
	
	if((numclose = close(fd)) == -1){
		char* err = int2string(errno);
		strcpy(respbuff, "-1");
		strcat(respbuff, ",");
		strcat(respbuff, err);
		free(err);
		return respbuff;
	}
	else{
		char* nclose = int2string(numclose);
		strcpy(respbuff, "1");
		strcat(respbuff, ",");
		strcat(respbuff, nclose);
		free(nclose);
		return respbuff;
	}
}

char* int2string(int n){
	
	char* buff = malloc(sizeof(char) * sizeof(int) * 4 + 1);
	
	if(buff){
		sprintf(buff, "%d", n);
	}
	
	return buff;
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

hashtable* initialize(hashtable* hashtable, int size) {

	hashtable = malloc(sizeof(hashtable));
	hashtable->table = malloc(sizeof(node*) * size);
	hashtable->size = size;
	cleartable(hashtable);
	hashtable->numkeys = 0;
	hashtable->balancefactor = 0.75;

	return hashtable;
}

void cleartable(hashtable* hashtable) {

	int i;
	for (i = 0; i < hashtable->size; i++) {
		hashtable->table[i] = NULL;
	}
}

unsigned long hashingfunc(char* key, hashtable* hashtable) {

	unsigned long hash = 5381; //Arbitrary prime
	unsigned int c;
	char* k = (char*)malloc(strlen(key)+1);
	strcpy(k, key);

	while ((c = (*k)++)) { //Finds hashing value
		hash = hash * 33 + c;
	}

	unsigned int idx = hash % hashtable->size; //Finds index from hashing value

	while (hashtable->table[idx] != NULL && strcmp(hashtable->table[idx]->key, key) != 0) { //Linear probing in case of collisions
		idx = (idx + 1) % hashtable->size;
	}

	free(k);
	return idx;
}

hashtable* insert(char* key, int fd, int accessmode, int connectmode, hashtable* hashtable) {

	unsigned int idx = hashingfunc(key, hashtable);

	node* n = malloc(sizeof(node));
	
	n->key = malloc(strlen(key));
	strcpy(n->key, key);
	n->fd = fd;
	n->accessmode = accessmode;
	n->connectmode = connectmode;
	
	if(hashtable->table[idx] == NULL){
		hashtable->numkeys++;
	}

	n->next = hashtable->table[idx];
	hashtable->table[idx] = n;

	if ((double)hashtable->numkeys / (double)hashtable->size >= hashtable->balancefactor) {
		return rehash(hashtable);
	}

	return hashtable;
}

node* getnode(char* key, hashtable* hashtable){
	
	return hashtable->table[hashingfunc(key, hashtable)];
}

hashtable* delnode(node* n, hashtable* hashtable){
	
	free(n->key);
	free(n);
	return hashtable;
}

hashtable* rehash(hashtable* oldtable) {

	hashtable* newtable = malloc(sizeof(hashtable));
	newtable->size = oldtable->size * 2;
	newtable->table = malloc(sizeof(node*) * newtable->size);
	cleartable(newtable);
	newtable->numkeys = oldtable->numkeys;
	newtable->balancefactor = oldtable->balancefactor;

	int i;
	for (i = 0; i < oldtable->size; i++) {
		if (oldtable->table[i] != NULL) {
			newtable->table[hashingfunc(oldtable->table[i]->key, newtable)] = oldtable->table[i];
		}
	}

	free(oldtable->table);
	free(oldtable);

	return newtable;
}

void freetable(hashtable* hashtable) {

	int i;
	for (i = 0; i < hashtable->size; i++) {
		
		node* n = hashtable->table[i];
		while(n != NULL){
			node* c = n;
			n = n->next;
			free(c->key);
			free(c);
		}
	}

	free(hashtable->table);
	free(hashtable);
}

hashtable2* initialize2(hashtable2* hashtable2, int size) {

	hashtable2 = malloc(sizeof(hashtable2));
	hashtable2->table = malloc(sizeof(node2*) * size);
	hashtable2->size = size;
	cleartable2(hashtable2);
	hashtable2->numkeys = 0;
	hashtable2->balancefactor = 0.75;

	return hashtable2;
}

void cleartable2(hashtable2* hashtable2) {

	int i;
	for (i = 0; i < hashtable2->size; i++) {
		hashtable2->table[i] = NULL;
	}
}

unsigned long hashingfunc2(int key, hashtable2* hashtable2) {

	unsigned long hash = 5381; //Arbitrary prime

	unsigned int idx = (hash * 33 + key) % hashtable2->size; //Finds index from hashing value

	while (hashtable2->table[idx] != NULL && hashtable2->table[idx]->key != key) { //Linear probing in case of collisions
		idx = (idx + 1) % hashtable2->size;
	}

	return idx;
}

hashtable2* insert2(int key, char* filepath, hashtable2* hashtable2) {

	unsigned int idx = hashingfunc2(key, hashtable2);

	node2* n = malloc(sizeof(node2));
	
	n->key = key;
	n->filepath = malloc(strlen(filepath));
	strcpy(n->filepath, filepath);
	
	hashtable2->table[idx] = n;

	if ((double)hashtable2->numkeys / (double)hashtable2->size >= hashtable2->balancefactor) {
		return rehash2(hashtable2);
	}

	return hashtable2;
}

node2* getnode2(int key, hashtable2* hashtable2){
	
	return hashtable2->table[hashingfunc2(key, hashtable2)];
}

hashtable2* delnode2(int key, hashtable2* hashtable2){
	
	free(hashtable2->table[hashingfunc2(key, hashtable2)]->filepath);
	free(hashtable2->table[hashingfunc2(key, hashtable2)]);
	hashtable2->table[hashingfunc2(key, hashtable2)] = NULL;
	return hashtable2;
}

hashtable2* rehash2(hashtable2* oldtable) {

	hashtable2* newtable = malloc(sizeof(hashtable2));
	newtable->size = oldtable->size * 2;
	newtable->table = malloc(sizeof(char*) * newtable->size);
	cleartable2(newtable);
	newtable->numkeys = oldtable->numkeys;
	newtable->balancefactor = oldtable->balancefactor;

	int i;
	for (i = 0; i < oldtable->size; i++) {
		if (oldtable->table[i] != NULL) {
			newtable->table[hashingfunc2(oldtable->table[i]->key, newtable)] = oldtable->table[i];
		}
	}

	free(oldtable->table);
	free(oldtable);

	return newtable;
}

void freetable2(hashtable2* hashtable2) {

	int i;
	for (i = 0; i < hashtable2->size; i++) {
		free(hashtable2->table[i]->filepath);
		free(hashtable2->table[i]);
	}

	free(hashtable2->table);
	free(hashtable2);
}

void queue(char* key){
	
	//pthread_mutex_lock(&mutex);
	ticklock* t = malloc(sizeof(ticklock));
	tlock(t);
	
	while(1){
		sleep(3);
		printf("Periodic Check...\n");
		if(modetable->table[hashingfunc(key, modetable)] == NULL){
			//pthread_mutex_unlock(&mutex);
			tunlock(t);
			free(t);
			return;
		}
	}
	
	return;
}

void tlock(ticklock* t){
	
    unsigned long qme;

    pthread_mutex_lock(&t->m);
    qme = t->qtail++;
	
    while (qme != t->qhead){
		
        pthread_cond_wait(&t->c, &t->m);
    }
	
    pthread_mutex_unlock(&t->m);
}

void tunlock(ticklock* t){
	
    pthread_mutex_lock(&t->m);
    t->qhead++;
    pthread_cond_broadcast(&t->c);
    pthread_mutex_unlock(&t->m);	
}