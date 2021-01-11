#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/file.h>

#define DIMBUF 1024
#define handle_error(msg)\
        { perror(msg); exit(errno); }

using namespace std;

extern int errno;
int port;

void getCommand(const char* compared);
void login(int sockd);
void readProblem(int sockd);
void sendSourceCode(int sockd);
void receiveResults(int sockd);

int main (int argc, char *argv[])
{
    int sd, nr = 0;
    struct sockaddr_in server;
    char buf[DIMBUF] = "Rezultat";

    if(argc != 3)
    {
        printf("Lipsesc argumentele: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }
    port = atoi (argv[2]);

    if((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror ("Eroare la socket().\n");
        return errno;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons (port);

    if(connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
        perror ("[client] Eroare la connect().\n");
        return errno;
    }
    getCommand("login");
    login(sd);
    readProblem(sd);
    getCommand("sendsource");
    sendSourceCode(sd);
    receiveResults(sd);
    getCommand("exit");
    close(sd);
}
void getCommand(const char* compared)
{
    printf("Dati o comanda : ");
    char command[25];
    while(1)
    {
        cin.getline(command, 25);
        if(strchr(command, ' ') != NULL)
        {
            printf("Comanda nu poate contine spatii.\n");
            fflush(stdout);
        }
        else
        {
            if(strcmp(command, compared) == 0)
                break;
            if(strcmp(command, "exit") == 0)
            {
                printf("Program incheiat cu succes.\n");
                fflush(stdout); exit(0);
            }
            printf("Comanda necesara : %s\nDati o comanda : ", compared);
            fflush(stdout);
        }
    }
}
void login(int sockd)
{
    printf("Introduceti username : ");
    char buffer[DIMBUF];
    while(1)
    {
        cin.getline(buffer, DIMBUF);
        if(strchr(buffer, ' ') == NULL)
            break;
        printf("Introduceti username fara spatii : ");
    }
    if(write(sockd, buffer, DIMBUF) <= 0)
        handle_error("[client] Eroare la scrierea in socket a username-ului.\n");
    bzero(buffer, DIMBUF);
    if(read(sockd, buffer, DIMBUF) <= 0)
        handle_error("[client] Eroare la citirea din socket a mesajului.\n");
    printf("%s\n", buffer); fflush(stdout);
}
void readProblem(int sockd)
{
    char statement[DIMBUF*2], msg[50];
    if(read(sockd, msg, 50) <= 0)
        handle_error("[client] Eroare la primirea mesajului de inceput.");
    printf("%s", msg); fflush(stdout);
    if(read(sockd, statement, DIMBUF*2) <= 0)
        handle_error("[client] Eroare la primirea enuntului problemei.");
    printf("%s", statement); fflush(stdout);
}
void sendSourceCode(int sockd)
{
    char sourcename[20];
    int fd;
    printf ("[client] Intoduceti numele fisierului sursa (*.c, *.cpp): ");
    fflush (stdout);
    read (0, sourcename, DIMBUF);
    sourcename[strlen(sourcename) - 1] = '\0';
    while((fd = open(sourcename, O_RDONLY)) <= 0)
    {
        printf("Fisierul dat nu exista.\n");
        printf ("[client] Intoduceti numele fisierului sursa (*.c, *.cpp): ");
        fflush (stdout);
        read (0, sourcename, DIMBUF);
        sourcename[strlen(sourcename) - 1] = '\0';
    }
    close(fd);
    if(write (sockd, sourcename, DIMBUF) <= 0)
        handle_error("[client] Eroare la trimiterea numelui fisierului sursa spre server.\n");
    int source_fd, readcode;
    char buffer[DIMBUF];

    if((source_fd = open(sourcename, O_RDWR)) == -1)
        handle_error("[client]Eroare la deschiderea fisierului sursa.\n");

    while(1)
    {
        bzero(buffer, DIMBUF);
        if((readcode = read(source_fd, buffer, DIMBUF)) < 0)
            handle_error("[client] Eroare la citirea din fisierul sursa.\n");
        if(write(sockd, buffer, DIMBUF) < 0)
            handle_error("[client] Eroare la scrierea in socket a fisierului sursa.\n");
        printf("%s", buffer);
        if(readcode <= DIMBUF)
            break;
    }
    close(source_fd);
}
void receiveResults(int sockd)
{
    char buffer[DIMBUF];
    printf("[client] Se asteapta rezultatele.\n");
    if(read(sockd, buffer, DIMBUF) <= 0)
        handle_error("[client] Eroare la primirea rezultatelor.");
    printf("%s", buffer);
    fflush(stdout);
}
