#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>


#define TCP_BUFFER_NUMS 500

int  tcp_head;
int  tcp_tail;

int  tcp_audio_head;
int  tcp_audio_tail;

void gn_tcp_init();

ssize_t  gn_tcp_send();

ssize_t  gn_tcp_read();

void gn_tcp_listener();

void gn_tcp_put(char* data);

void gn_tcp_audio_put(char* data);

ssize_t  gn_tcp_audio_send();

int IsSocketClosed();

void gn_tcp_clear();