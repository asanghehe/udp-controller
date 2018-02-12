#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int port = 6789;

int main(int argc, char ** argv){
  int sin_len;
  char message[256];
  
  int socket_descriptor;
  struct sockaddr_in sin;
  printf("Waiting for data from sender \n");
  
  bzero(&sin, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(port);
  sin_len = sizeof(sin);
  
  socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
  bind(socket_descriptor, (struct sockaddr *)&sin, sizeof(sin));
  
  int msg_len = 0;
  
  //阻塞监听
  while((msg_len = recvfrom(socket_descriptor, message, sizeof(message), 0, (struct sockaddr *)&sin, &sin_len)) > -1){
    //recvfrom(socket_descriptor, message, sizeof(message), 0, (struct sockaddr *)&sin, &sin_len);
    
    //清空多余的字符
    memset(message, msg_len, sizeof(message));
    
    printf("Response from server: %s\n", message);
    if(strncmp(message, "stop", 4) == 0){
      printf("Sender has told me to end the connection\n");
      break;
    }
  }
  
  close(socket_descriptor);
  
  return (EXIT_SUCCESS);
}
