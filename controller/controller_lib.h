#ifndef CONTROLLER_LIB_H_
#define CONTROLLER_LIB_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>


#define CONFIG_FILE "config.txt"
#define PORT 22000
#define MAXBUF 4096
#define K 10
#define M 3
#define TIME_UNIT 4
#define MAXLOAD 1000
#define MAXNB 20

typedef enum {REGISTER_REQUEST, REGISTER_RESPONSE, KEEP_ALIVE, ROUTE_UPDATE, TOPOLOGY_UPDATE} msg_type;

typedef struct message_protocol{
  msg_type type;
  int id;
  char buffer[MAXBUF];
}sw_message;

typedef struct edge_info{
  int bandwith;
  int delay;
  int active;
}edge;

typedef struct path_info{
  int load;
  int pre;
}path;

typedef struct neighbor_info{
  int id;
  int alive;
  struct sockaddr_in addr;
}neighbors;

typedef struct rr_info{
  int num_switch;
  neighbors all_neighbor[MAXNB];
}rr_update;

typedef struct topology_up_info{
  int nb_id;
  int active;
}tplg_update;

typedef struct rout_info{
  int num_switch;
  int array[MAXNB];
}nxt_hop;


//Global socket info
int controller_fd;
struct sockaddr_in *switchaddrs;
struct sockaddr_in controlleraddr;

//Global TOPOLOGY info
edge **graph;
int num_switch;
int *active_switch;
int *registered_switch;


//Global max load path info
path **max_load;

//Read config file and store graph info
int config_controller();

//Init socket for controller
int init_controller();

//Main running function
void running();

//Send REGISTER_RESPONSE
void send_RR(int id);

//Update topology
//end:-1 indicate to all dest
void update_topology(int start,int end,int active); 

//Send updated widest path
void send_path();

//Compute widest path
void compute_path();

//Free malloc for global pointers
void free_mem();

#endif
