#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

       #include <sys/types.h>
       #include <sys/stat.h>
       #include <fcntl.h>


#define SERV_TCP_PORT 7788 /*8545*/ /* server's port */

// sleep_time = size * 1000 / 2 / 22050
// 20 = size * 1000 / 2 / 22050
// size = 20 * 2 * 22050 / 1000 = 882

// 16000
// 20ms send 640


static void send_data_loop(int socket) {
    //	16000 * 16 * 2 * 1 / 8
    //char buf[882] = { 0 };
    char buf[640] = { 0 };
    int fd = open("provider.wav", O_RDONLY);
    
    if (fd < 0) {
        printf("can not open the file....\n");
        return;
    }
    
    while (1) {
        ssize_t ret = read(fd, buf, sizeof(buf));
        
        if ( ret <= 0 ){
            lseek(fd, 0, SEEK_SET);
            printf("reset to file header ...\n");
        }
        
        ssize_t result = write(socket, buf, ret);
        if (result != ret) {
           printf("not full write ...\n");
        }
        usleep(20 * 1000);
    }
    
    close(fd);
}
int main(int argc, char *argv[])
{
  int sockfd;
  struct sockaddr_in serv_addr;
  char *serv_host = "192.168.101.106";
  struct hostent *host_ptr;
  int port;
  int buff_size = 0;

  /* command line: client [host [port]]*/
  if(argc >= 2) 
     serv_host = argv[1]; /* read the host if provided */
  if(argc == 3)
     sscanf(argv[2], "%d", &port); /* read the port if provided */
  else 
     port = SERV_TCP_PORT;

  /* get the address of the host */
  if((host_ptr = gethostbyname(serv_host)) == NULL) {
     perror("gethostbyname error");
     exit(1);
  }

  if(host_ptr->h_addrtype !=  AF_INET) {
     perror("unknown address type");
     exit(1);
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = 
     ((struct in_addr *)host_ptr->h_addr_list[0])->s_addr;
  serv_addr.sin_port = htons(port);


  /* open a TCP socket */
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
     perror("can't open stream socket");
     exit(1);
  }

  /* connect to the server */    
  if(connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
     perror("can't connect to server");
     exit(1);
  }

  /* write a message to the server */
  //write(sockfd, "hello world", sizeof("hello world"));
send_data_loop(sockfd);
  close(sockfd);
  
  return 0;
}
