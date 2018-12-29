#ifndef _libnetfiles_h_
#define _libnetfiles_h_

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

int netopen(const char* pathname, int flags);
ssize_t netread(int fildes, void * buf, size_t nbyte);
ssize_t netwrite(int fildes, const void * buf, size_t nbyte);
int netclose(int fd);
int netserverinit(char * hostname, int filemode);
int hostname_to_ip(char * hostname , char* ip);
char** tokenizer(char* str, int commas);
char** tokenizer1(char* str);
void freee(char ** r);

#endif
