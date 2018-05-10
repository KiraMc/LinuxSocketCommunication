#include "controller_lib.h"

#define LISTENQ 10

//Config the graph
int config_controller(){
  FILE *fp;
  int i,j;
  char row[255];
  int id1,id2,bandwith,delay;

  printf("-----------------Controller start config!-------------------------\n");
  fp = fopen(CONFIG_FILE,"r");
  if(fp == NULL){
    perror("Controller main: Fail to open config file");
    return -1;
  }
  //Read number of switches
  if(fgets(row,sizeof(row),fp)!=NULL){
    num_switch = atoi(row);
  }
  printf("Number of switches: %d\n",num_switch);

  //Allocate memory for active array, registered array,max_load and switch address
  active_switch = (int *)malloc(num_switch * sizeof(int));
  registered_switch = (int *)malloc(num_switch * sizeof(int));
  max_load = (path **)malloc(num_switch * (sizeof(path *)));
  for(i=0;i<num_switch;i++){
    *(max_load+i) = (path *)malloc(num_switch * (sizeof(path)));
  }
  switchaddrs = (struct sockaddr_in *)malloc(num_switch * (sizeof(struct sockaddr_in)));

  //Init them as all inactive and not registered
  for(i = 0;i<num_switch;i++){
    *(active_switch+i) = 0;
    *(registered_switch+i) = 0;
  }
  printf("Graph config\n");
  //Allocate memory for graph
  graph = (edge **)malloc(num_switch * sizeof(edge *));
  if(graph == NULL){
    perror("Controller config: Fail to malloc for graph");
    fclose(fp);
    return -1;
  }
  for(i = 0;i<num_switch;i++){
    *(graph+i) = (edge *)malloc(num_switch * sizeof(edge));
    if(*(graph+i) == NULL){
      perror("Controller config: Fail to malloc for graph*");
      fclose(fp);
      return -1;
    }
  }

  printf("Reset graph\n");
  //Reset graph bandwith to 0, meaning no connection between
  for(i=0;i<num_switch;i++){
    for(j=0;j<num_switch;j++){
      if(i != j){
	graph[i][j].bandwith = 0;
	graph[i][j].active = 0;
      }
      else{
	graph[i][j].bandwith = MAXLOAD;
	graph[i][j].active = 1;
      }
    }
  }

  printf("Map graph\n");
  //Map edge info
  while(fgets(row,sizeof(row),fp) != NULL){
    //printf("line: %s\n",row);
    sscanf(strtok(row," "),"%d",&id1);
    sscanf(strtok(NULL," "),"%d",&id2);
    sscanf(strtok(NULL," "),"%d",&bandwith);
    sscanf(strtok(NULL," "),"%d",&delay);

    //printf("Test split: %d, %d, %d, %d\n",id1,id2,bandwith,delay);
    graph[id1][id2].bandwith = bandwith;
    graph[id1][id2].delay = delay;
    graph[id2][id1].bandwith = bandwith;
    graph[id2][id1].delay = delay;
  }

  fclose(fp);

  return 0;
}

//Set socket
int init_controller(){
  int optval=1;
  printf("------------------------Controller start to init--------------------\n");
  //create socket descriptor
  if((controller_fd = socket(AF_INET,SOCK_DGRAM,0)) < 0){
    perror("Controller init: Socket create fail\n");
    return -1;
  }

  //Eliminates address already in use;
  if(setsockopt(controller_fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) <0){
    perror("Controller init: arrdess already in use\n");
    return -1;
  }

  memset((char *)&controlleraddr, 0, sizeof(controlleraddr));
  controlleraddr.sin_family = AF_INET;
  controlleraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  controlleraddr.sin_port = htons((unsigned short)PORT);

  if(bind(controller_fd, (struct sockaddr *)&controlleraddr, sizeof(controlleraddr)) <0){
    perror("Controller init: bind failed\n");
    return -1;
  }
  
  return 0;
}

//Send back REGISTER_RESPONSE based on id info
void send_RR(int id){
  sw_message send_message;
  int i,j,num_nb = 0;
  rr_update response_info;
  
  send_message.type = REGISTER_RESPONSE;
  send_message.id = -1;
  printf("Send Register_Response back to Switch: %d\n",id);


  
  //Allocate memory for neighbor info and put data in
  
  j=0;
  for(i=0;i<num_switch;i++){
    if(i != id && graph[id][i].bandwith > 0){
      num_nb += 1;
      response_info.all_neighbor[j].id = i;
      response_info.all_neighbor[j].alive = active_switch[i];
      if(active_switch[i] == 1) response_info.all_neighbor[j].addr = switchaddrs[i];
      j++;
    }
  }
  response_info.num_switch = num_nb;
  //Copy neighbor info into buf
  memcpy(send_message.buffer, &response_info, sizeof(response_info));
  printf("Number of neighbor: %d\n",num_nb);
  sendto(controller_fd,&send_message,sizeof(send_message),0,(struct sockaddr *)&switchaddrs[id],sizeof(switchaddrs[id]));
  
}

//Update the topology
void update_topology(int start,int end,int active){
  int i,j;
  if(end == -1){
    printf("Update topology based on switch %d dead\n",start);
    active_switch[start] = 0;
    for(i=0;i<num_switch;i++){
      for(j=0;j<num_switch;j++){
	if(i==start || j==start)  graph[i][j].active = 0;
      }
    }
  }
  else{
    printf("Update topology based on %d to %d connection fail/active: %d\n",start,end,active);
    graph[start][end].active = active;
    graph[end][start].active = active;
  } 
}

//Send computed path
void send_path(){
  int i,j,k;
  
  printf("Send widest path to all active switches\n");
  for(i=0;i<num_switch;i++){
    sw_message send_message;
    nxt_hop hop_info;
    for(j=0;j<num_switch;j++){
      if(j!=i){
	k = j;
	if(max_load[i][k].load == 0){
	  hop_info.array[j] = -1;
	  continue;
	}
	while(max_load[i][k].pre != -1 && max_load[i][k].pre != i){
	  k = max_load[i][k].pre;
	}
	hop_info.array[j] = k;
      }else{
	hop_info.array[j] = i;
      }
    }
    /*printf("Next hop info for switch %d ",i);
    for(j=0;j<num_switch;j++){
      printf(" (dest: %d, nexthop: %d) ",j,hop_info.array[j]);
    }
    printf("\n");*/
    send_message.id = -1;
    send_message.type = ROUTE_UPDATE;
    hop_info.num_switch = num_switch;
    memcpy(send_message.buffer, &hop_info,sizeof(hop_info));
    sendto(controller_fd,&send_message,sizeof(send_message),0,(struct sockaddr *)&switchaddrs[i],sizeof(switchaddrs[i]));
  }
}

//Compute widest path
void path_start_with(int id){
  int i,j,max,select_s;
  int visited[num_switch];

  for(i=0;i<num_switch;i++){
    visited[i] = 0;
    max_load[id][i].load = 0;
  }

  //Initialize the load info
  for(i=0;i<num_switch;i++){
    max_load[id][i].pre = -1;
    if(i != id && graph[id][i].active == 1){
      max_load[id][i].load = graph[id][i].bandwith;
      if(graph[id][i].bandwith !=0){
	max_load[id][i].pre = id;
      }
    }else if(i == id){
      max_load[id][i].load = MAXLOAD;
    }
  }
  //Compute the relation for start switch
  visited[id] = 1;
  for(i=0;i<num_switch;i++){
    max = 0;
    if(visited[i] == 0){
      select_s = id;
      //Find the max load neighbor switch
      for(j=0;j<num_switch;j++){
	if(max_load[id][j].load > max && visited[j] == 0 && graph[id][j].active == 1){
	  max = max_load[id][j].load;
	  select_s = j;
	}
      }
      //Mark it as visited
      visited[select_s] = 1;

      //Update path info
      for(j=0;j<num_switch;j++){
	if(max_load[id][j].load < max_load[id][select_s].load && max_load[id][j].load < graph[select_s][j].bandwith && visited[j] == 0 && graph[select_s][j].active == 1){
	  max_load[id][j].load = (max_load[id][select_s].load > graph[select_s][j].bandwith)?graph[select_s][j].bandwith:max_load[id][select_s].load;
	  max_load[id][j].pre = select_s;
	}
      }
    }
  }
}
void compute_path(){
  int i,j;
  printf("Compute the widest path\n");
  
  for(i=0;i<num_switch;i++){
    path_start_with(i);
  }/*
  for(i=0;i<num_switch;i++){
    printf("Max_load for id %d ",i);
    for(j=0;j<num_switch;j++){
      printf(" (dest:%d, load:%d) ",j,max_load[i][j]);
    }
    printf("\n");
    }*/
  send_path();
}

void running(){
  int i;
  //Variables to receive message
  int switchlen,recvlen,rc_select;
  sw_message received_message;
  struct sockaddr_in received_addr;

  //Variables for select
  fd_set read_fd;
  struct timeval timeout;

  //Variables for timers
  time_t *timers = (time_t *)malloc(num_switch * sizeof(time_t));
  
  printf("---------------------Controller start to listen---------------------\n");

  //Initialize the select variable
  timeout.tv_sec = TIME_UNIT;
  FD_ZERO(&read_fd);
  FD_SET(controller_fd,&read_fd);
  
  //Initialize timers
  for(i = 0;i<num_switch;i++){
    *(timers+i) = time(0);
  }

  while(1){
    int recompute_flag = 0;
    rc_select = select(controller_fd+1,&read_fd,NULL,NULL,&timeout);
    
    if(rc_select > 0){
      int start_compute_flag = 1,i,j;
      rr_update received_tp_update;
      switchlen = sizeof(received_addr);
      recvlen = recvfrom(controller_fd,&received_message,sizeof(sw_message),0,(struct sockaddr *)&received_addr, &switchlen);

      switch(received_message.type){
      case REGISTER_REQUEST:
	printf("REGISTER_REQUEST from switch: %d\n",received_message.id);
	//Update active info
        active_switch[received_message.id] = 1;
	registered_switch[received_message.id] = 1;
	switchaddrs[received_message.id] = received_addr;

	//Send Register Response
	send_RR(received_message.id);

	//Update edge active info
	for(i=0;i<num_switch;i++){
	  if(registered_switch[i] == 0) start_compute_flag = 0;
	}
	if(start_compute_flag ==1){
	  for(i=0;i<num_switch;i++){
	    for(j=0;j<num_switch;j++){
	      if(active_switch[i]==1 && active_switch[j]==1)  graph[i][j].active = 1;
	    }
	  }
	  compute_path();
	}
	//Update timer for id
	timers[received_message.id] = time(0);

	break;

      case TOPOLOGY_UPDATE:
	printf("TOPOLOGY_UPDATE from switch: %d\n",received_message.id);
	//Update timer
	timers[received_message.id] = time(0);

	//update topology and recompute path
	memcpy(&received_tp_update, received_message.buffer,sizeof(received_tp_update));
	
	for(i = 0;i<num_switch;i++){
	  if(registered_switch[i] == 0) start_compute_flag = 0;
	}

	if(start_compute_flag){
	  for(i=0;i<received_tp_update.num_switch;i++){
	    if(received_tp_update.all_neighbor[i].alive != active_switch[received_tp_update.all_neighbor[i].id]){
	      recompute_flag = 1;
	      update_topology(received_message.id, received_tp_update.all_neighbor[i].id, received_tp_update.all_neighbor[i].alive);
	    }
	  }
	}
	
	if(recompute_flag == 1) compute_path();
      }	
    }else if(rc_select == 0){
      printf("Time Check\n");
      for(i = 0;i<num_switch;i++){
	double difft = difftime(time(0),timers[i]);
        if(active_switch[i] == 1 && difft >= M*K){
	  recompute_flag = 1;
	  printf("Switch %d is dead\n",i);
	  update_topology(i,-1,0);
	}
      }
      if(recompute_flag) compute_path();
      FD_SET(controller_fd,&read_fd);
      timeout.tv_sec = TIME_UNIT;
    }else{
      perror("Select Error\n");
    }
  }

  if(timers != NULL){
    free(timers);
    timers = NULL;
  }
  close(controller_fd);
}

//Free memory
void free_mem(){
  int i;
  if(graph != NULL){
    for(i = 0;i<num_switch;i++){
      if(*(graph+i) != NULL){
	free(*(graph+i));
	*(graph+i) = NULL;
      }
    }
    free(graph);
    graph = NULL;
  }
  
  if(active_switch != NULL){
    free(active_switch);
    active_switch = NULL;
  }

  if(registered_switch != NULL){
    free(registered_switch);
    registered_switch = NULL;
  }

  if(max_load != NULL){
    for(i=0;i<num_switch;i++){
      if(*(max_load + i) != NULL){
	free(*(max_load+i));
	*(max_load+i) = NULL;
      }
    }
    free(max_load);
    max_load = NULL;
  }

  if(switchaddrs != NULL){
    free(switchaddrs);
    switchaddrs = NULL;
  }
}
