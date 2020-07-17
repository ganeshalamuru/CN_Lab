#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

void error(char *msg)
{
    perror(msg);
    exit(0);
}
int mul(int x, int y, int mod)
{
    return (long long)x * y % mod;
}
int ipow(int x, int y, int p)
{
    int ret = 1, piv = x % p;
    while (y)
    {
        if (y & 1)
            ret = mul(ret, piv, p);
        piv = mul(piv, piv, p);
        y >>= 1;
    }
    return ret;
}
int encrpytmsg(int m, int e, int n)
{
    return ipow(m, e, n);
}
int main(int argc, char *argv[])
{

    int sockfd, portno, nn;

    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[25600];
    if (argc < 3)
    {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    int flag = 0;
    int n, e;
    while (1)
    {
        if (flag == 0)
        {
            bzero(buffer, 25600);
            nn = read(sockfd, buffer, 25500);
            buffer[strcspn(buffer, "\n")] = 0;
            sscanf(buffer, "%d %d", &n, &e);
            printf("n : %d ,e : %d\n", n, e);
            nn = read(sockfd, buffer, sizeof(buffer));
        }
        flag = 1;
        printf("Please enter message: ");
        bzero(buffer, 25600);
        fgets(buffer, 25500, stdin);
        int num_bits = 0;
        int tmp = n;
        while (tmp > 0)
        {
            num_bits++;
            tmp >>= 1;
        }
        if (num_bits % 8 != 0)
            num_bits += (8 - num_bits % 8);
        int len = strlen(buffer);
        char e_msg[25600];
        int block_sz = num_bits / 4;
        FILE *fp = fmemopen(e_msg, len * num_bits + 2, "w+");
        for (int i = 0; i < len; i++)
        {
            char ch = buffer[i];
            int m = (int)ch;
            int y = encrpytmsg(m, e, n);
            fprintf(fp, "%0*x", block_sz, y);
        }
        fclose(fp);
        printf("encrpted_msg :: %s \n", e_msg);
        nn = write(sockfd, e_msg, strlen(e_msg));
        if (nn < 0)
            error("ERROR writing to socket");
        buffer[strcspn(buffer, "\n")] = 0;
        if (strcmp("quit", buffer) == 0)
            break;
        bzero(buffer, 25600);
        nn = read(sockfd, buffer, 25600);
        if (nn < 0)
            error("ERROR reading from socket");
        printf("message recieved from server :: %s\n", buffer);
    }
    close(sockfd);
    return 0;
}