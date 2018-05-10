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
#include "switch.h"

int open_connection(){
	int ret;
	struct hostent *host;
	struct sockaddr_in addr;
	
	if((ret = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		printf("Error: Could not create socket\n");
		return -1;
	}

	return ret;	
}

int main(int argc, char **argv){
	int sock, port, recvlen, i, id, count, verbose;
	int fail = -1;
	char* host;
	FILE* logfile;
	time_t start = time(NULL);
	struct sw_message msg;
	struct sw_message msg_recv;
	struct sockaddr_in controller;
	struct sockaddr_in recv_temp;
	struct sockaddr_in target;
	struct hostent* hp;
	struct hostent* hp_target;
	struct neighbor nb[20];
	struct rr_update up;

	char *part = ".log";
	char filename[10];

	sprintf(filename, "%s%s", argv[1], part);
	//printf("%s\n", filename);

	logfile = fopen(filename, "w");

	if(argc != 5 && argc != 7){
		printf("Usage: switch [id] [controller port] [controller hostname] [verbose] {-f [fail id]}");
		return 1;
	}

	port = atoi(argv[2]);
	id = atoi(argv[1]);
	host = argv[3];
	verbose = atoi(argv[4]);
	if(argc == 7){
		fail = atoi(argv[6]);
	}	

	sock = open_connection();

	//printf("Connection opened...\n");

	bzero((char*)&controller, sizeof(controller));
	controller.sin_family = AF_INET;
	controller.sin_port = htons(port);

	/*if ((hp = gethostbyname(host)) == NULL){
		printf("Error: Could not resolve hostname");
		return 1;
	}*/

	//bcopy((char *)hp->h_addr, (char *)&controller.sin_addr.s_addr, hp->h_length);

	controller.sin_addr.s_addr = htonl(INADDR_ANY);

	if(sock < 0){
		return 1;
	}

	bzero((char*)&msg, sizeof(msg));
	
	msg.type = REGISTER_REQUEST;
	msg.source = id;

	bcopy((char*)&id, (char*)msg.buffer, sizeof(id));
	
	printf("%ld: Sending REGISTER_REQUEST...\n", (long int)(time(NULL)-start));
	fprintf(logfile, "%ld: Sending REGISTER_REQUEST...\n", (long int)(time(NULL)-start));

	signal(SIGPIPE, SIG_IGN);
	
	if(sendto(sock, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&controller, sizeof(controller)) < 0){
		printf("Could not send REGISTER_REQUEST packet\n");
		return 1;
	}

	printf("%ld: REGISTER_REQUEST sent\n", (long int)(time(NULL)-start));
	fprintf(logfile, "%ld: REGISTER_REQUEST sent\n", (long int)(time(NULL)-start));

	recvlen = sizeof(recv_temp);

	bzero((char*)&msg_recv, sizeof(msg_recv));

	if(recvfrom(sock, &msg_recv, sizeof(msg_recv), 0, (struct sockaddr*)&recv_temp, &recvlen) < 0){
		printf("Did not receive response packet\n");
		return 1;
	}

	if(msg_recv.type != REGISTER_RESPONSE){
		printf("Did not receive proper REGISTER_RESPONSE packet\n");
		return 1;
	}

	printf("%ld: REGISTER_RESPONSE received\n", (long int)(time(NULL)-start));
	fprintf(logfile, "%ld: REGISTER_RESPONSE received\n", (long int)(time(NULL)-start));

	memcpy((char*)&up, (char*)msg_recv.buffer, sizeof(up));

	count = up.num_switch;

	bzero((char*)&msg, sizeof(msg));

	msg.type = KEEP_ALIVE;
	msg.source = id;

	int maxfd = sock;

	//printf("%d\n", up.num_switch);

	for(i = 0; i < up.num_switch; i++){
		nb[i].id = up.neighbor_list[i].id;
		nb[i].alive = up.neighbor_list[i].alive;
		nb[i].timeout = timerfd_create(CLOCK_REALTIME, 0);
		if(nb[i].timeout > maxfd) maxfd = nb[i].timeout;
		nb[i].addr = up.neighbor_list[i].addr;
		//printf("Neighbor %d: %d\n", i, nb[i].alive);
		if(nb[i].alive && nb[i].id != fail){
			if(sendto(sock, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&(nb[i].addr), sizeof(nb[i].addr)) < 0){
				printf("Could not send KEEP_ALIVE message after REGISTER_RESPONSE\n");
			}
			printf("%ld: Sent KEEP_ALIVE to node %d\n", (long int)(time(NULL)-start), nb[i].id);
			if(verbose) fprintf(logfile, "%ld: Sent KEEP_ALIVE to node %d\n", (long int)(time(NULL)-start), nb[i].id);
		}
	}

	int alivefd = timerfd_create(CLOCK_REALTIME, 0);
	if(alivefd > maxfd) maxfd = alivefd;
	struct itimerspec spec1, spec2;

	spec1.it_interval.tv_sec = 0;
	spec1.it_interval.tv_nsec = 0;
	spec1.it_value.tv_sec = T_K;
	spec1.it_value.tv_nsec = 0;

	timerfd_settime(alivefd, 0, &spec1, NULL);

	for(i = 0; i < count; i++){
		nb[i].spec.it_interval.tv_sec = 0;
		nb[i].spec.it_interval.tv_nsec = 0;
		nb[i].spec.it_value.tv_sec = T_K * T_M;
		nb[i].spec.it_value.tv_nsec = 0;

		timerfd_settime(nb[i].timeout, 0, &(nb[i].spec), NULL);
	}

	fd_set rfds;
	int selval;
	struct topology tp;
	struct routes rt;

	while(1){
		FD_ZERO(&rfds);
		FD_SET(alivefd, &rfds);
		FD_SET(sock, &rfds);
		for(i = 0; i < count; i++){
			if(nb[i].alive){
				FD_SET(nb[i].timeout, &rfds);
			}
		}
		selval = select(maxfd+1, &rfds, NULL, NULL, NULL);

		if(FD_ISSET(sock, &rfds)){
			bzero((char*)&msg_recv, sizeof(msg_recv));
			if(recvfrom(sock, &msg_recv, sizeof(msg_recv), 0, (struct sockaddr*)&recv_temp, &recvlen) < 0){
				printf("Could not open packet\n");
				return 1;
			}
			if(msg_recv.type == KEEP_ALIVE){
				for(i = 0; i < count; i++){
					if(nb[i].id == msg_recv.source && nb[i].id != fail){
						nb[i].spec.it_interval.tv_sec = 0;
						nb[i].spec.it_interval.tv_nsec = 0;
						nb[i].spec.it_value.tv_sec = T_K * T_M;
						nb[i].spec.it_value.tv_nsec = 0;
						timerfd_settime(nb[i].timeout, 0, &(nb[i].spec), NULL);
						if(!nb[i].alive){
							up.neighbor_list[i].alive = 1;
							up.neighbor_list[i].addr = recv_temp;
							nb[i].alive = 1;
							nb[i].addr = recv_temp;
							tp.node = nb[i].id;
							tp.alive = 1;
							bzero((char*)&msg, sizeof(msg));
							memcpy((char*)msg.buffer, (char*)&up, sizeof(up));
							msg.type = TOPOLOGY_UPDATE;
							msg.source = id;
							if(sendto(sock, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&controller, sizeof(controller)) < 0){
								printf("Could not send TOPOLOGY_UPDATE packet\n");
								return 1;
							}
							printf("%ld: Sent TOPOLOGY_UPDATE on reactivated node\n", (long int)(time(NULL)-start));
							fprintf(logfile, "%ld: Sent TOPOLOGY_UPDATE on reactivated node\n", (long int)(time(NULL)-start));
						}
					}
				}
			} else if(msg_recv.type == ROUTE_UPDATE){
				memcpy((char*)&rt, (char*)msg_recv.buffer, sizeof(rt));
				printf("%ld: Received ROUTE_UPDATE\n", (long int)(time(NULL)-start));
				printf("Routing Table\n");
				fprintf(logfile, "%ld: Received ROUTE_UPDATE\n", (long int)(time(NULL)-start));
				fprintf(logfile, "Routing Table\n");
				for(i = 0; i < rt.count; i++){
					printf("%d: %d\n", i, rt.hops[i]);
					fprintf(logfile, "%d: %d\n", i, rt.hops[i]);
				}
				printf("\n");
				fprintf(logfile, "\n");
			}

		} else if(FD_ISSET(alivefd, &rfds)){
			bzero((char*)&msg, sizeof(msg));
			msg.type = KEEP_ALIVE;
			msg.source = id;
			for(i = 0; i < count; i++){
				if(nb[i].alive && nb[i].id != fail){
					if(sendto(sock, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&(nb[i].addr), sizeof(nb[i].addr)) < 0){
						printf("Could not send KEEP_ALIVE message\n");
					}
					printf("%ld: Sent KEEP_ALIVE to node %d\n", (long int)(time(NULL)-start), nb[i].id);
					if(verbose) fprintf(logfile, "%ld: Sent KEEP_ALIVE to node %d\n", (long int)(time(NULL)-start), nb[i].id);
				}
			}
			bzero((char*)&msg, sizeof(msg));
			msg.type = TOPOLOGY_UPDATE;
			msg.source = id;
			if(sendto(sock, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&controller, sizeof(controller)) < 0){
				printf("Could not send TOPOLOGY_UPDATE packet\n");
				return 1;
			}
			printf("%ld: Sent TOPOLOGY_UPDATE\n", (long int)(time(NULL)-start));
			fprintf(logfile, "%ld: Sent TOPOLOGY_UPDATE\n", (long int)(time(NULL)-start));

			spec1.it_interval.tv_sec = 0;
			spec1.it_interval.tv_nsec = 0;
			spec1.it_value.tv_sec = T_K;
			spec1.it_value.tv_nsec = 0;

			timerfd_settime(alivefd, 0, &spec1, NULL);
		}
		for(i = 0; i < count; i++){
			if(FD_ISSET(nb[i].timeout, &rfds)){
				up.neighbor_list[i].alive = 0;
				nb[i].alive = 0;
				tp.node = nb[i].id;
				tp.alive = 0;
				bzero((char*)&msg, sizeof(msg));
				memcpy((char*)msg.buffer, (char*)&up, sizeof(up));
				msg.type = TOPOLOGY_UPDATE;
				msg.source = id;
				printf("ID: %d\n", msg.source);
				if(sendto(sock, (char*)&msg, sizeof(msg), 0, (struct sockaddr*)&controller, sizeof(controller)) < 0){
					printf("Could not send TOPOLOGY_UPDATE packet\n");
					return 1;
				}
				printf("%ld: Sent TOPOLOGY_UPDATE on node failure\n", (long int)(time(NULL)-start));
				fprintf(logfile, "%ld: Sent TOPOLOGY_UPDATE on node failure\n", (long int)(time(NULL)-start));
			}
		}
	}	
}
