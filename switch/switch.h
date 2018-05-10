#ifndef MY_STRUCT_H
#define MY_STRUCT_H

#define MAX_BUF 4096
#define T_K 10
#define T_M 3
#define MAX_NODE 20

#include <sys/socket.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <time.h>


typedef enum {REGISTER_REQUEST, REGISTER_RESPONSE, KEEP_ALIVE, ROUTE_UPDATE, TOPOLOGY_UPDATE} msg_type;

struct sw_message{
	msg_type type;
	int source;
	char buffer[MAX_BUF];
};

struct neighbor_send{
	int id;
	int alive;
	struct sockaddr_in addr;
};

struct neighbor{
	int id;
	int alive;
	int timeout;
	struct itimerspec spec;
	struct sockaddr_in addr;
};

struct rr_update{
	int num_switch;
	struct neighbor_send neighbor_list[MAX_NODE];
};

struct routes{
	int count;
	int hops[MAX_NODE];
};

struct topology{
	int node;
	int alive;
};

#endif
