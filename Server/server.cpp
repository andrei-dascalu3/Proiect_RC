#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/file.h>
#include <stdlib.h>
#include <time.h>
#include <chrono>
#include <libxml2/libxml/parser.h>
#include <libxml2/libxml/tree.h>
#include <libxml/xmlmemory.h>

#define PORT 2800
#define DIMBUF 1024
#define handle_error(msg,exitcode)\
        { perror(msg); if(exitcode>=0)exit(exitcode); }

using namespace std;

extern int errno;
static void *treat(void *);

typedef struct {
    pthread_t idThread;
    int thCount;
} Thread;

Thread *threadsPool;

int sd, nthreads, problemCode, problemTime = 60;
pthread_mutex_t mlock=PTHREAD_MUTEX_INITIALIZER;

chrono::high_resolution_clock::time_point startTime;

void configure();
void threadCreate(int i);
void *treat(void * arg);
void login(int clientSock);
int chooseProblem(int nrProblems);
void sendProblem(int clientSock);
void receiveSource(int clientSock, char* sourceName);
void evaluateSource(int clientSock, char* sourceName, char* execName);
void itoa(char number[10], int N)
{
    bzero(number, 0);
    int lg = 0, i;
    char aux;
    if(N == 0)
    {
        number[0] = '0';
        return;
    }
    while(N)
    {
        number[lg++] = '0' + N % 10;
        N /= 10;
    }
    for(i = 0; i < lg/2; ++i)
    {
        aux = number[i];
        number[i] = number[lg-i-1];
        number[lg-i-1] = aux;
    }
}

int main (int argc, char *argv[])
{
    struct sockaddr_in server;

    if(argc<2)
        handle_error("[server] Eroare: Numar invalid de argumente. Primul argument este numarul de fire de executie.", 1);

    nthreads = atoi(argv[1]);

    if(nthreads <= 0)
        handle_error("[server] Eroare: Numar invalid de fire de executie.", 2);

    threadsPool = (Thread*)calloc(nthreads, sizeof(Thread));

    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
        handle_error("[server] Eroare la socket().\n", errno);

    int on=1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    bzero (&server, sizeof (server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl (INADDR_ANY);
    server.sin_port = htons (PORT);

    if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
        handle_error("[server] Eroare la bind().", errno);
    if (listen (sd, 2) == -1)
        handle_error("[server] Eroare la listen().", errno);

    printf("Numarul de participanti asteptati (fire de executie) = %d \n", nthreads); fflush(stdout);
    int i, nrProblems = 1;
    problemCode = chooseProblem(nrProblems);

    configure();
    for(i = 0; i < nthreads; i++)
        threadCreate(i);

    while(1)
    {
        printf ("[server] Asteptam la portul %d...\n",PORT);
        pause();
    }
}

void configure()
{
    char command[10], param[20], value[5], i;
    while(1)
    {
        printf("Dati comanda start sau modify daca doriti sa schimbati configuratia: ");
        fflush(stdout);
        bzero(command, 0);
        cin.getline(command, 10);
        if(strcmp(command, "start") == 0)
            break;
        else if(strcmp(command, "modify") == 0)
        {
            bzero(param, 0);
            printf("Introduceti parametrul pe care doriti sa il modificati:\n-> participanti\n-> timp (min)\n");
            fflush(stdout);
            cin.getline(param, 20);
            if(strcmp(param, "participanti") == 0)
            {
                printf("Valoare : ");
                fflush(stdout);
                bzero(value, 0);
                cin.getline(value, 5);
                for(i = 0; i < strlen(value); ++i)
                    if(value[i] < '0' || value[i] > '9')
                    {
                        printf("Valoare invalida. Dati un nr. natural.\n");
                        fflush(stdout);
                        break;
                    }
                if(i = strlen(value))
                {
                    nthreads = atoi(value);
                    threadsPool = (Thread*)calloc(nthreads, sizeof(Thread));
                }
            }
            else if(strcmp(param, "timp") == 0)
            {
                printf("Valoare : ");
                fflush(stdout);
                bzero(value, 0);
                cin.getline(value, 5);
                for(i = 0; i < strlen(value); ++i)
                    if(value[i] < '0' || value[i] > '9')
                    {
                        printf("Valoare invalida. Dati un nr. natural.\n");
                        fflush(stdout);
                        break;
                    }
                if(i = strlen(value))
                    problemTime = atoi(value);
            }
            else
            {
                printf("Parametru invalid. Dati un parametru corespunzator.\n");
                fflush(stdout);
            }
        }
    }
    startTime = chrono::high_resolution_clock::now();
}
void threadCreate(int i)
{
    pthread_create(&threadsPool[i].idThread, NULL, &treat, &i);
    return;
}

void *treat(void * arg)
{
    int client, tag;
    char sourceName[DIMBUF], execName[DIMBUF];
    struct sockaddr_in from;
    bzero (&from, sizeof (from));
    tag = *((int*) arg);
    printf ("[thread]- %d - pornit...\n", tag);
    fflush(stdout);

    while(1) {
        socklen_t length = sizeof (from);
        pthread_mutex_lock(&mlock);

        if ((client = accept(sd, (struct sockaddr *) &from, &length)) < 0)
            handle_error("[thread] Eroare la accept().\n", -1);
        pthread_mutex_unlock(&mlock);
        threadsPool[tag].thCount++;

        login(client);
        sendProblem(client);
        receiveSource(client, sourceName);
        evaluateSource(client, sourceName, execName);

        close (client);
    }
}
void login(int clientSock)
{
    char buffer[DIMBUF], msg[DIMBUF] = "V-ati logat cu username-ul: ";
    if(read(clientSock, buffer, DIMBUF) <= 0)
        handle_error("[server] Eroare la citirea username-ului din socket.\n", errno);
    printf("Utilizator %s logat.\n", buffer);
    strcat(msg, buffer); strcat(msg, "\n");
    if(write(clientSock, msg, strlen(msg)) <= 0)
        handle_error("[server] Eroare la scrierea in socket.\n", errno);
}
int chooseProblem(int nrProblems)
{
    srand(time(NULL));
    return rand() % nrProblems;
}
void sendProblem(int clientSock)
{
    printf("[server] Se trimite enuntul problemei.\n");
    char statement[DIMBUF*2], msg[50] = "Competitia a inceput. Timp : ", time[5];
    itoa(time, problemTime);
    strcat(msg, time); strcat(msg, "\n");
    if(write(clientSock, msg, strlen(msg)) <= 0)
        handle_error("[service] Eroare la trimiterea mesajului de inceput.\n", errno);
    int pbfd;
    bzero(statement, DIMBUF*2);
    if((pbfd = open("Probleme/Pb1.txt", O_RDONLY)) == -1)
        handle_error("[server] Eroare la deschiderea fisierului cu enuntul problemei.\n", errno);
    if(read(pbfd, statement, DIMBUF*2) <= 0)
        handle_error("[service] Eroare la citirea enuntului problemei.\n", errno);
    if(write(clientSock, statement, DIMBUF*2) <= 0)
        handle_error("[service] Eroare la trimiterea enuntului problemei.\n", errno);
}
void receiveSource(int clientSock, char* sourceName)
{
    printf("[server] Se accepta surse cu rezolvarea problemei.\n");
    int source_fd, readcode;
    char buffer[DIMBUF], *dot;
    sourceName[0] = '_';
    if(read(clientSock, sourceName + 1, DIMBUF) < 0)
        handle_error("[server] Eroare la citirea numelui fisierului sursa trimis.\n", errno);

    if((source_fd = open(sourceName, O_RDWR | O_CREAT, 0644)) == -1)
        handle_error("[server] Eroare la crearea fisierului sursa de catre server.\n", errno);

    while(1)
    {
        bzero(buffer, DIMBUF);
        if((readcode = read(clientSock, buffer, DIMBUF)) < 0)
            handle_error("[server] Eroare la citirea din socket.\n", errno);
        if(write(source_fd, buffer, DIMBUF) < 0)
            handle_error("[server] Eroare la scrierea in fisierul sursa.\n", errno);
        if(readcode <= DIMBUF)
            break;
    }
    /*if(write(clientSock, "Am primit fisierul sursa.\n", DIMBUF) < 0)
        handle_error("[server] Eroare la scrierea in socket.\n", errno);
    */
    close(source_fd);
}
void evaluateSource(int clientSock, char* sourceName, char* execName)
{
    pid_t child;
    char *extension;

    if(-1 == (child = fork()))
        handle_error("[server] Eroare la compilarea sursei primite.\n", errno);
///Compilarea sursei participantului
    if(child == 0)
    {
        if((extension = strstr(sourceName, ".cpp")) != NULL)
        {
            *extension = '\0';
            strcpy(execName, sourceName);
            *extension = '.';
            printf("[server] Fisier C++.\n");
            if(-1 == execlp("g++", "g++", sourceName, "-o", execName, NULL))
                handle_error("[server] Eroare la compilarea sursei primite.\n", errno);
        }
        else if((extension = strstr(sourceName, ".c")) != NULL)
        {
            *extension = '\0';
            strcpy(execName, sourceName);
            *extension = '.';
            printf("[server] Fisier C.\n");
            if(-1 == execlp("gcc", "gcc", sourceName, "-o", execName, NULL))
                handle_error("[server] Eroare la compilarea sursei primite.\n", errno);
        }
    }
    printf("[server] Se evalueaza sursele primite si transmitem rezultatele.\n");
    if(write(clientSock, "Test 1 : 25 puncte\nTest 2 : 25 puncte\nTest 3 : 25 puncte\nTest 4 : 25 puncte\n\nTotal: 100 puncte\n", DIMBUF) < 0)
        handle_error("[server] Eroare la trimiterea rezultatelor.", errno);
    printf("[server] Am transmis rezultatele.\n");
    fflush(stdout);
}