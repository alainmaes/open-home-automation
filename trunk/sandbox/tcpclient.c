#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


int main(int argc, char **argv)

{
        int sock, bytes_recieved;
        char send_data[1024],recv_data[102400];
        struct hostent *host;
        struct sockaddr_in server_addr;

        if (argc != 4) {
            fprintf(stderr,"usage: %s <hostname> <port> <data>\n", argv[0]);
            exit(0);
        }

        host = gethostbyname(argv[1]);

        sock = socket(AF_INET, SOCK_STREAM,0);

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(atoi(argv[2]));
        server_addr.sin_addr = *((struct in_addr *)host->h_addr);
        bzero(&(server_addr.sin_zero),8);

        connect(sock, (struct sockaddr *)&server_addr,sizeof(struct sockaddr));

        snprintf(send_data, sizeof(send_data), "%s%c", argv[3], '\004');
        send(sock,send_data,strlen(send_data), 0);
        bytes_recieved=recv(sock,recv_data,sizeof(recv_data),0);
        recv_data[bytes_recieved-1] = '\0';

        printf("%s" , recv_data);
return 0;
} 
