#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */
#include "encode.c"

int SendStream(char *data, int data_len,char *mac, int num)
{
	char *ss = (char*)malloc(data_len * 8);
	b64_encode_noal(data, ss, data_len);

	int ddsss= strlen(ss);


	int portno =        0;  // PORT
    char *host =        ""; // IP
    char *message_fmt = "POST /pub?id=%s%d HTTP/1.0\r\nContent-Length: %d\r\n\r\n%s";
	char *message= (char*)malloc(ddsss + 512);
	char *response= (char*)malloc(4096);

	//printf("data_len=%d ss=%d\n", data_len,ddsss);

    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd, bytes, sent, received, total;


	int sl = strlen(ss);
    sprintf(message, message_fmt, mac, num, sl, ss); //printf("Request:\n%s\n",message);



    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server = gethostbyname(host);

    memset(&serv_addr,0,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; serv_addr.sin_port = htons(portno);
    memcpy(&serv_addr.sin_addr.s_addr,server->h_addr,server->h_length);

	if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
		free(message);free(response);free(ss);
		return 1;
	}
	total = strlen(message);
	bytes = write(sockfd, message, total);
	//printf("df = %d\n",bytes);
    /*
    total = strlen(message);
    sent = 0;
    do {
        bytes = write(sockfd,message+sent,total-sent);
        if (bytes < 0)
            free(message);free(response);free(ss);return 2;
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);
	*/
	/*
    memset(response,0,sizeof(response));
    bytes = read(sockfd,response,4096);
        printf("%d\n",bytes);
        printf("Response:\n%s\n",response);

    total = sizeof(response)-1;
    received = 0;
    do {
        bytes = read(sockfd,response+received,total-received);
        printf("%d\n",bytes);
        printf("Response:\n%s\n",response);
        if (bytes < 0)
            free(message);free(response);return 3;//free(ss);
        if (bytes == 0)
            break;
        received+=bytes;
    } while (received < total);

	printf("Response:\n%s\n",response);
	*/

    close(sockfd);


	free(message); free(response);free(ss);
    return 0;
}
