#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int port = 6789;
int main(int argc, char** argv){
  int socket_descriptor;
  int iter = 0;
  char buf[80];
  struct sockaddr_in address;
  
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr("127.0.0.1");
  address.sin_port = htons(port);
  
  socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
  
  for(iter = 0; iter<20; iter++){
    sleep(1);
    
    sprintf(buf, "data packet with ID %d\n", iter);
    sendto(socket_descriptor, buf, sizeof(buf), 0, (struct sockaddr *)&address, sizeof(address));
  }
  
  sprintf(buf, "stop\n");
  sendto(socket_descriptor, buf, sizeof(buf), 0, (struct sockaddr *) &address, sizeof(address));
  close(socket_descriptor);
  printf("Messages Sent, terminating\n");
  
  return (EXIT_SUCCESS);
}
