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

void getCommand(int sockd);
void getTime(int sockd);
int login(int sockd);
int readProblem(int sockd);
int sendSourceCode(int sockd);
void receiveResults(int sockd);
void getHelp(int sockd);

int main (int argc, char *argv[])
{
    int sd, nr = 0;
    struct sockaddr_in server;
    char buf[DIMBUF] = "Rezultat";

    if(argc != 3)
    {
        printf("[participant] Lipsesc argumentele: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }
    port = atoi (argv[2]);

    if((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
        handle_error("[participant] Eroare la socket().\n");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons (port);

    if(connect(sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
        handle_error("[participant] Eroare la connect().\n");

    getCommand(sd);

    close(sd);
}
void getCommand(int sockd)
{
    int has_logged = 0, has_read = 0, has_sent = 0;
    char command[25], message[5];
    while(1)
    {
        bzero(message, 5);
        printf("Dati o comanda : ");
        cin.getline(command, 25);
        if(strchr(command, ' ') != NULL)
        {
            printf("Comanda nu poate contine spatii.\n");
            fflush(stdout);
        }
        else
        {
            if(strcmp(command, "login") == 0)
            {
                if(has_logged == 0)
                {
                    if(write(sockd, command, 25) < 0)
                        handle_error("[participant] Eroare la transmiterea comenzii catre server.\n");
                    has_logged = login(sockd);
                }
                else
                {
                    printf("Sunteti logat.\n");
                    fflush(stdout);
                }
            }
            else if(strcmp(command, "get_problem") == 0)
            {
                if(has_logged == 0)
                {
                    printf("Este necesar sa va logati. Apelati comanda \'login\'.\n");
                    fflush(stdout);
                }
                else
                {
                    if(write(sockd, command, 25) < 0)
                        handle_error("[participant] Eroare la transmiterea comenzii catre server.\n");
                    has_read = readProblem(sockd);
                }
            }
            else if(strcmp(command, "send_source") == 0)
            {
                if(has_logged == 0)
                {
                    printf("Este necesar sa va logati. Apelati comanda \'login\'.\n");
                    fflush(stdout);
                }
                else if(has_read == 0)
                {
                    printf("Nu puteti trimite rezolvarea fara a citi in prealabil enuntul. Apelati comanda \'get_problem <numar>\'.\n");
                    fflush(stdout);
                }
                else
                {
                    if(write(sockd, command, 25) < 0)
                        handle_error("[participant] Eroare la transmiterea comenzii catre server.\n");
                    has_sent = sendSourceCode(sockd);
                    receiveResults(sockd);
                }
            }
            else if(strcmp(command, "help") == 0)
            {
                if(write(sockd, command, 25) < 0)
                        handle_error("[participant] Eroare la transmiterea comenzii catre server.\n");
                getHelp(sockd);
            }
            else if(strcmp(command, "exit") == 0)
            {
                printf("Ati parasit competitia.\n");
                fflush(stdout); exit(0);
            }
            else if(strcmp(command, "time") == 0)
            {
                if(write(sockd, command, 25) < 0)
                        handle_error("[participant] Eroare la transmiterea comenzii catre server.\n");
                getTime(sockd);
            }
            else
            {
                printf("Comanda inexistenta. Apelatii comanda \'help\' pentru clarificari.\n");
                fflush(stdout);
            }
        }
    }
}
void getTime(int sockd)
{
    char message[16];
    if(read(sockd, message, 16) < 0)
        handle_error("[participant] Eroare la primirea timpului de la server.\n");
    printf("Timp ramas: %s\n", message);
    fflush(stdout);
}
int login(int sockd)
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
    if(write(sockd, buffer, DIMBUF) < 0)
        handle_error("[participant] Eroare la scrierea in socket a username-ului.\n");
    bzero(buffer, DIMBUF);
    if(read(sockd, buffer, DIMBUF) < 0)
        handle_error("[participant] Eroare la citirea din socket a mesajului.\n");
    printf("%s\n", buffer); fflush(stdout);
    if(buffer[0] == 'U')
        return 0;
    return 1;
}
int readProblem(int sockd)
{
    char statement[DIMBUF*2], msg[50];
    if(read(sockd, msg, 50) < 0)
        handle_error("[participant] Eroare la primirea mesajului de inceput.");
    printf("%s", msg); fflush(stdout);
    if(read(sockd, statement, DIMBUF*2) < 0)
        handle_error("[participant] Eroare la primirea enuntului problemei.");
    printf("%s", statement); fflush(stdout);
    return 1;
}
int sendSourceCode(int sockd)
{
    char sourcename[40];
    int fd;
    printf ("Intoduceti numele fisierului sursa (*.c, *.cpp): ");
    fflush (stdout);
    read (0, sourcename, DIMBUF);
    sourcename[strlen(sourcename) - 1] = '\0';
    while((fd = open(sourcename, O_RDONLY)) < 0)
    {
        bzero(sourcename, 20);
        printf("Fisierul dat nu exista.\n");
        printf ("Intoduceti numele fisierului sursa (*.c, *.cpp): ");
        fflush (stdout);
        read (0, sourcename, DIMBUF);
        sourcename[strlen(sourcename) - 1] = '\0';
    }
    close(fd);
    if(write (sockd, sourcename, DIMBUF) < 0)
        handle_error("[participant] Eroare la trimiterea numelui fisierului sursa spre server.\n");
    int source_fd, readcode;
    char buffer[DIMBUF];

    if((source_fd = open(sourcename, O_RDWR)) == -1)
        handle_error("[participant] Eroare la deschiderea fisierului sursa.\n");

    while(1)
    {
        bzero(buffer, DIMBUF);
        if((readcode = read(source_fd, buffer, DIMBUF)) < 0)
            handle_error("[participant] Eroare la citirea din fisierul sursa.\n");
        if(write(sockd, buffer, DIMBUF) < 0)
            handle_error("[participant] Eroare la scrierea in socket a fisierului sursa.\n");
        if(readcode <= DIMBUF)
            break;
    }
    close(source_fd);
    return 1;
}
void receiveResults(int sockd)
{
    char buffer[DIMBUF];
    printf("Se asteapta rezultatele.\n");
    if(read(sockd, buffer, DIMBUF) < 0)
        handle_error("[participant] Eroare la primirea rezultatelor.");
    printf("%s", buffer);
    fflush(stdout);
}
void getHelp(int sockd)
{
    int readcode;
    char buffer[DIMBUF];
    if((readcode = read(sockd, buffer, DIMBUF)) < 0)
        handle_error("[participant] Eroare la citirea mesajului help.\n");
    printf("%s\n", buffer);
    fflush(stdout);
}
