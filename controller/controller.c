#include "controller_lib.h"

int main(int argc, char **argv){
  int i,j;
  int switchlen,recvlen;
  char buf[MAXBUF];

  if(config_controller() != 0){
    return -1;
  }

  if((init_controller()) != 0){
    return -1;
  }

  running();

  free_mem();
}
