/* A simple server in the internet domain using TCP
   The port number is passed as an argument 
   This version runs forever, forking off a separate 
   process for each connection
   gcc server2.c -lsocket
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <time.h>

int isPrime[100000 + 1] = {0};
void sieve()
{
    for (int i = 0; i < 100000 + 1; i++)
        isPrime[i] = 1;
    isPrime[0] = isPrime[1] = 0;
    for (int i = 2; i * i <= 100000; i++)
        if (isPrime[i] == 1)
            for (int j = i * i; j <= 100000; j += i)
                isPrime[j] = 0;
}
int gcd(int a, int b)
{
    if (b == 0)
        return a;
    else
        return gcd(b, a % b);
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
int miller_rabin(int x, int a)
{
    if (x % a == 0)
        return 1;
    int d = x - 1;
    while (1)
    {
        int tmp = ipow(a, d, x);
        if (d & 1)
            return (tmp != 1 && tmp != x - 1);
        else if (tmp == x - 1)
            return 0;
        d >>= 1;
    }
}
int isprime(int n)
{
    int primeslt_40[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    for (int i = 0; i < 12; i++)
    {
        if (n == primeslt_40[i])
            return 1;
        if (n > 40 && miller_rabin(n, primeslt_40[i]))
            return 0;
    }
    if (n <= 40)
        return 0;
    return 1;
}
int gcdExtended(int a, int b, int *x, int *y)
{
    if (a == 0)
    {
        *x = 0;
        *y = 1;
        return b;
    }
    int x1, y1;
    int gcd = gcdExtended(b % a, a, &x1, &y1);
    *x = y1 - (b / a) * x1;
    *y = x1;

    return gcd;
}
void dostuff(int); /* function prototype */
void error(char *msg)
{
    perror(msg);
    exit(1);
}
int dencrpytmsg(int y, int d, int n)
{
    return ipow(y, d, n);
}
int main(int argc, char *argv[])
{
    //generate two large prime (0,1e3)
    srand(time(NULL));
    sieve();
    int p, q;
    do
    {
        do
            p = rand() % 1000;
        while (p <= 2 || isprime(p) != 1);
        do
            q = rand() % 1000;
        while (q <= 2 || isprime(q) != 1);
    } while (isPrime[p] != 1 || isPrime[q] != 1);
    // compute n;
    int n = p * q;

    //compute phi(n)
    int phi_n = (p - 1) * (q - 1);

    //select public key e
    int e;
    do
        e = rand() % phi_n;
    while (gcd(e, phi_n) != 1);

    //compute private key d
    int d;
    int a = e, b = phi_n;
    int x, y;
    int g = gcdExtended(a, b, &x, &y);
    while (x < 0)
        x += phi_n;
    d = x;
    int num_bits = 0;
    int tmp = n;
    while (tmp > 0)
    {
        num_bits++;
        tmp >>= 1;
    }
    if (num_bits % 8 != 0)
        num_bits += (8 - num_bits % 8);
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int shmid = shmget(IPC_PRIVATE, sizeof(int), (IPC_CREAT | 0666));
    int *count = (int *)shmat(shmid, NULL, 0);
    count[0] = 0;
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *)&serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *)&serv_addr,
             sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd, 3);
    clilen = sizeof(cli_addr);
    int pids[4];
    while (1)
    {

        newsockfd = accept(sockfd,
                           (struct sockaddr *)&cli_addr, &clilen);
        count[0]++;
        char pub_key[25600];
        bzero(pub_key, sizeof(pub_key));
        sprintf(pub_key, "%d %d\n", n, e);
        if (newsockfd < 0)
            error("ERROR on accept");
        pid = fork();
        if (pid < 0)
            error("ERROR on fork");
        if (pid == 0)
        {
            int *count_child = shmat(shmid, NULL, 0);
            close(sockfd);
            printf("in server2 newsockfd :: %d\n", newsockfd);

            //dostuff(newsockfd);
            int sock = newsockfd;
            write(sock, pub_key, sizeof(pub_key));
            int nn;
            char e_msg[25600];
            while (1)
            {
                bzero(e_msg, 25600);
                nn = read(sock, e_msg, 25590);
                if (nn < 0)
                    error("ERROR reading from socket");
                e_msg[strcspn(e_msg, "\n")] = 0;
                if (strcmp("quit", e_msg) == 0)
                    break;
                printf("Here is the encrpted_msg message: %s\n", e_msg);
                int len2 = strlen(e_msg);
                int block_sz = num_bits / 4;
                char d_msg[25600];
                FILE *fp2 = fmemopen(d_msg, 25600, "w+");
                FILE *fp = fmemopen(e_msg, 25600, "r");
                int dec = 0;
                int cnt = 1;
                int num_b = num_bits / 8;
                while (len2 > 0)
                {
                    int tmp;
                    fscanf(fp, "%02x", &tmp);
                    dec = (dec << 8) + tmp;
                    if (cnt % (num_b) == 0)
                    {
                        int d_y = ipow(dec, d, n);
                        char ch = (char)d_y;
                        fprintf(fp2, "%c", ch);
                        dec = 0;
                    }
                    cnt++;
                    len2 -= 2;
                }
                fclose(fp);
                fclose(fp2);
                printf("decrpyted msg from client :: %s", d_msg);
                d_msg[strcspn(d_msg, "\n")] = 0;
                if (strcmp("quit", d_msg) == 0)
                    break;
                nn = write(sock, d_msg, 25600);
                if (nn < 0)
                    error("ERROR writing to socket");
            }
            count_child[0]--;
            exit(0);
        }
        else
            close(newsockfd);
    }         /* end of while */
    return 0; /* we never get here */
}
