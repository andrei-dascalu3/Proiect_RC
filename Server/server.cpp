#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
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

typedef struct
{
    pthread_t idThread;
    int thCount;
} Thread;

Thread *threadsPool;

int sd, nthreads, problemCode = 1, problemTime = 20*60;
pthread_mutex_t mlock=PTHREAD_MUTEX_INITIALIZER;

chrono::high_resolution_clock::time_point startTime;

void findUser(xmlNode* root, char* searched, bool& found);
void buildProblem(char* statement, xmlNode* root);
void getIO(char* file, char type);
void modifyInput(char* correct, char* inputFile, int idProblem);

void configure();
void threadCreate(int i);
void *treat(void * arg);
void getCommand(int clientSock, char* sourceName, char* execName, char* dirName);
void sendTime(int clientSock);
void sendHelp(int clientSock);
void login(int clientSock);
void sendProblem(int clientSock);
void receiveSource(int clientSock, char* sourceName, char* execName, char* dirName);
void evaluateSource(int clientSock, char* sourceName, char* execName, char* dirName);
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

    printf("Numarul de participanti asteptati (fire de executie) = %d \n", nthreads);
    fflush(stdout);
    int i;

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
            printf("Introduceti parametrul pe care doriti sa il modificati:\n-> participanti\n-> id_problema\n");
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
            else if(strcmp(param, "id_problema") == 0)
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
                    problemCode = atoi(value);
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
    char sourceName[100], execName[100], dirName[200];
    struct sockaddr_in from;
    bzero (&from, sizeof (from));
    tag = *((int*) arg);
    printf ("[thread]- %d - pornit...\n", tag);
    fflush(stdout);

    while(1)
    {
        socklen_t length = sizeof (from);
        pthread_mutex_lock(&mlock);

        if ((client = accept(sd, (struct sockaddr *) &from, &length)) < 0)
            handle_error("[thread] Eroare la accept().\n", -1);
        pthread_mutex_unlock(&mlock);
        threadsPool[tag].thCount++;

        getCommand(client, sourceName, execName, dirName);
    }
}
void getCommand(int clientSock, char* sourceName, char* execName, char* dirName)
{
    char command[25];
    int readcode, help_fd;
    while(1)
    {
        bzero(command, 25);
        if((readcode = read(clientSock, command, 25)) < 0)
            handle_error("[server] Eroare la citirea comenzii de la participant.\n", errno);
        if(strcmp(command, "login") == 0)
            login(clientSock);
        else if(strcmp(command, "get_problem") == 0)
            sendProblem(clientSock);
        else if(strcmp(command, "send_source") == 0)
        {
            receiveSource(clientSock, sourceName, execName, dirName);
            evaluateSource(clientSock, sourceName, execName, dirName);
        }
        else if(strcmp(command, "help") == 0)
            sendHelp(clientSock);
        else if(strcmp(command, "time") == 0)
            sendTime(clientSock);
        else if(strcmp(command, "exit") == 0)
        {
            close(clientSock);
            return;
        }
    }

}
void sendTime(int clientSock)
{
    char message[16], minutes[3], seconds[3];
    auto timeNow = std::chrono::high_resolution_clock::now();
    chrono::duration<int> elapsed = chrono::duration_cast<chrono::seconds>(timeNow - startTime);
    int remained = problemTime - int(elapsed.count());
    itoa(minutes, remained/60);
    cout<<remained/60<<' '<<remained%60<<endl;
    itoa(seconds, remained%60);
    strcpy(message, minutes);
    strcat(message, " : ");
    strcat(message, seconds);
    strcat(message, " s");
    if(write(clientSock, message, 16) <= 0)
        handle_error("[server] Eroare la trimiterea timpului catre participant.\n", errno);
}
void sendHelp(int clientSock)
{
    int help_fd;
    char buffer[DIMBUF];
    if((help_fd = open("Help.txt", O_RDONLY)) <= 0)
        handle_error("[server] Eroare deschiderea fisierului Help.txt.\n", errno);
    if(read(help_fd, buffer, DIMBUF) <= 0)
        handle_error("[server] Eroare la citirea din fisierul Help.txt.\n", errno);
    if(write(clientSock, buffer, DIMBUF) <= 0)
        handle_error("[server] Eroare la trimiterea textului din Help.txt catre participant.\n", errno);
    printf("[server] Am transmis informatii de ajutor ramas catre participant.\n");
    fflush(stdout);
}
void login(int clientSock)
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    bool found = false;
    char buffer[DIMBUF], msg[DIMBUF] = "V-ati logat cu username-ul: ";

    doc = xmlReadFile("Participants.xml", NULL, 0);
    if (doc == NULL)
        handle_error("[server] Eroare la parsarea fisierului \'Participants.xml\'.\n", 1);
    root_element = xmlDocGetRootElement(doc);
    if(read(clientSock, buffer, DIMBUF) <= 0)
        handle_error("[server] Eroare la citirea username-ului din socket.\n", errno);
    findUser(root_element, buffer, found);
    if(found)
    {

        printf("Utilizator %s logat.\n", buffer);
        strcat(msg, buffer);
        strcat(msg, "\n");
        if(write(clientSock, msg, strlen(msg)) <= 0)
            handle_error("[server] Eroare la scrierea in socket.\n", errno);
    }
    else
    {
        printf("Utilizatorul %s nu exista.\n", buffer);
        sprintf(msg, "Utilizatorul %s nu exista.\n", buffer);
        if(write(clientSock, msg, strlen(msg)) <= 0)
            handle_error("[server] Eroare la scrierea in socket.\n", errno);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
}
void sendProblem(int clientSock)
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    int pbfd;
    char statement[DIMBUF*2], msg[50] = "Competitia a inceput.\n";

    printf("[server] Se trimite enuntul problemei.\n");
    if(write(clientSock, msg, strlen(msg)) <= 0)
        handle_error("[service] Eroare la trimiterea mesajului de inceput.\n", errno);
    bzero(statement, DIMBUF*2);

    doc = xmlReadFile("Problems.xml", NULL, 0);
    if (doc == NULL)
        handle_error("[server] Eroare la parsarea fisierului \'Problems.xml\'.\n", 1);
    root_element = xmlDocGetRootElement(doc);
    buildProblem(statement, root_element);
    if(write(clientSock, statement, DIMBUF*2) <= 0)
        handle_error("[service] Eroare la trimiterea enuntului problemei.\n", errno);
    xmlFreeDoc(doc);
    xmlCleanupParser();
}
void receiveSource(int clientSock, char* sourceName, char* execName, char* dirName)
{
    struct stat dirStat;
    printf("[server] Se accepta surse cu rezolvarea problemei.\n");
    int source_fd, readcode;
    char buffer[DIMBUF], *extension, inputFile[100], outputFile[100];
    if(read(clientSock, buffer, DIMBUF) < 0)
        handle_error("[server] Eroare la citirea numelui fisierului sursa trimis.\n", errno);

///Construirea numelor directorului specific concurentului + fisiere
    ///Director
    extension = strchr(buffer, '.');
    *extension = '\0';
    strcpy(dirName, "_"); strcat(dirName, buffer);
    *extension = '.';
    if(-1 == stat(dirName, &dirStat))
        mkdir(dirName, 0700);

    chdir(dirName);
    ///Sursa
    strcpy(sourceName, buffer);
    if((source_fd = open(sourceName, O_RDWR | O_CREAT, 0644)) == -1)
        handle_error("[server] Eroare la crearea fisierului sursa de catre server.\n", errno);

    ///Executabil
    *extension = '\0';
    strcpy(execName, buffer);
    *extension = '.';

    ///Fisiere I/O
    getIO(inputFile, 'I');
    getIO(outputFile, 'O');
    if(-1 == stat(inputFile, &dirStat))
        open(inputFile, O_CREAT, 0655);
    if(-1 == stat(outputFile, &dirStat))
        open(outputFile, O_CREAT, 0655);

    while(1)
    {
        bzero(buffer, DIMBUF);
        if((readcode = read(clientSock, buffer, DIMBUF)) < 0)
            handle_error("[server] Eroare la citirea din socket.\n", errno);
        if(write(source_fd, buffer, readcode) < 0)
            handle_error("[server] Eroare la scrierea in fisierul sursa.\n", errno);
        if(readcode <= DIMBUF)
            break;
    }
    /*if(write(clientSock, "Am primit fisierul sursa.\n", DIMBUF) < 0)
        handle_error("[server] Eroare la scrierea in socket.\n", errno);
    */
    chdir("..");
    close(source_fd);
}
void evaluateSource(int clientSock, char* sourceName, char* execName, char* dirName)
{
    struct stat st;
    pid_t child;
    int ii, outFD, totalScore = 0;
    char buffer[DIMBUF], results[DIMBUF] = "Rezultate:\n", *extension, correct[DIMBUF], inputFile[100], outputFile[100], testStr[4];

    chdir(dirName);
    extension = strchr(sourceName, '.');
///Compilarea sursei participantului
    getIO(inputFile, 'I');
    getIO(outputFile, 'O');
    if(-1 == (child = fork()))
        handle_error("[server] Eroare la compilarea sursei primite.\n", errno);
    if(child == 0)
    {
        if(strcmp(extension, ".cpp") == 0)
        {
            printf("[server] Fisier C++.\n");
            if(-1 == execlp("g++", "g++", "-W", sourceName, "-o", execName, NULL))
            {
                if(write(clientSock, "Compilarea sursei a esuat.\n", DIMBUF) < 0)
                    handle_error("[server] Eroare la trimiterea rezultatelor.", errno);
                handle_error("[server] Eroare la compilarea sursei primite.\n", errno);
            }
        }
        else if(strcmp(extension, ".c") == 0)
        {
            printf("[server] Fisier C.\n");
            if(-1 == execlp("gcc", "gcc", "-W", sourceName, "-o", execName, NULL))
            {
                if(write(clientSock, "Compilarea sursei a esuat.\n", DIMBUF) < 0)
                    handle_error("[server] Eroare la trimiterea rezultatelor.", errno);
                handle_error("[server] Eroare la compilarea sursei primite.\n", errno);
            }
        }
    }
    else
    {
        waitpid(child, NULL, 0);
    }

///Evaluarea sursei participantului
    printf("[server] Se evalueaza sursele primite si transmitem rezultatele.\n");
    child = 0;
    for(ii = 1; ii <= 5; ++ii)
    {
        bzero(correct, DIMBUF);
        bzero(buffer, DIMBUF);
        bzero(testStr, 4);
        modifyInput(correct, inputFile, ii);
        if(-1 == (child = fork()))
            handle_error("[server] Eroare la evaluarea sursei primite.\n", errno);
        if(child == 0)
        {
            if(-1 == execl(execName, execName, NULL))
                handle_error("[server] Eroare la evaluarea sursei primite.\n", errno);
        }
        else
        {
            waitpid(child, NULL, 0);
            outFD = open(outputFile, O_RDONLY);
            read(outFD, buffer, DIMBUF);
            close(outFD);
            buffer[strlen(correct)] = '\0';
            if(strcmp(buffer, correct) == 0)
            {
                totalScore += 20;
                strcat(results, "Testul "); itoa(testStr, ii); strcat(results, testStr); strcat(results, ": 20 pct\n");
            }
            else
            {
                strcat(results, "Testul "); itoa(testStr, ii); strcat(results, testStr); strcat(results, ": 0 pct\n");
            }
        }
    }
    strcat(results, "Total: "); itoa(testStr, totalScore); strcat(results, testStr); strcat(results, "\n");
    if(write(clientSock, results, DIMBUF) < 0)
        handle_error("[server] Eroare la trimiterea rezultatelor.", errno);
    printf("[server] Am transmis rezultatele.\n");
    fflush(stdout);
    chdir("..");
}

void findUser(xmlNode* root, char* searched, bool& found)
{
    xmlNode* cur_node = NULL;
    char compared[DIMBUF];
    for (cur_node = root; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type == XML_ELEMENT_NODE)
        {
            if(strcmp((const char*)cur_node->name, "USERNAME") == 0)
            {
                bzero(compared, DIMBUF);
                strcpy(compared, (const char*)cur_node->children->content);
                if(strcmp(searched, compared) == 0)
                    found = true;
            }
        }
        findUser(cur_node->children, searched, found);
    }
}
void buildProblem(char* statement, xmlNode* root)
{
    xmlNode* cur_node = NULL, *section = NULL;
    int number;
    char id[2];
    sprintf(id, "%d", problemCode);
    strcpy(statement, "______________________________\n");
    for(cur_node = root->children; cur_node; cur_node = cur_node->next)
    {
        if(cur_node->type == XML_ELEMENT_NODE && strcmp((const char*)cur_node->name, "PROBLEM") == 0)
        {
            number = atoi((const char*)xmlGetProp(cur_node, (const xmlChar*)"id"));
            if(number == problemCode)
            {
                strcat(statement, "[");
                strcat(statement, id);
                strcat(statement, "]\n");
                for(section = cur_node->children; strcmp((const char*)section->name, "TESTS") != 0 && section; section = section->next)
                {
                    if(section->type == XML_ELEMENT_NODE)
                    {
                        if(strcmp((const char*)section->name, "TITLE") == 0)
                        {
                            strcat(statement, "~");
                            strcat(statement, (const char*)section->children->content);
                            strcat(statement, "~");
                            strcat(statement, "\n\n");
                        }
                        else if(strcmp((const char*)section->name, "IN_NAME") == 0)
                        {
                            strcat(statement, "INPUT: ");
                            strcat(statement, (const char*)section->children->content);
                            strcat(statement, "\n");
                        }
                        else if(strcmp((const char*)section->name, "OUT_NAME") == 0)
                        {
                            strcat(statement, "OUTPUT: ");
                            strcat(statement, (const char*)section->children->content);
                            strcat(statement, "\n");
                        }
                        else if(strcmp((const char*)section->name, "TIME") == 0)
                        {
                            strcat(statement, "TIMP (min):\n");
                            strcat(statement, (const char*)section->children->content);
                            strcat(statement, "\n\n");
                        }
                        else
                        {
                            strcat(statement, (const char*)section->children->content);
                            strcat(statement, "\n\n");
                        }
                    }
                }
            }
        }
    }
    strcat(statement, "______________________________\n");
}
void getIO(char* file, char type)
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    xmlNode* cur_node = NULL, *section = NULL;
    int number;
    char id[2];
    doc = xmlReadFile("../Problems.xml", NULL, 0);
    root_element = xmlDocGetRootElement(doc);
    if (doc == NULL)
        handle_error("[server] Eroare la parsarea fisierului \'Problems.xml\'.\n", 1);
    sprintf(id, "%d", problemCode);
    for(cur_node = root_element->children; cur_node; cur_node = cur_node->next)
    {
        if(cur_node->type == XML_ELEMENT_NODE && strcmp((const char*)cur_node->name, "PROBLEM") == 0)
        {
            number = atoi((const char*)xmlGetProp(cur_node, (const xmlChar*)"id"));
            if(number == problemCode)
            {
                for(section = cur_node->children; strcmp((const char*)section->name, "TESTS") != 0 && section; section = section->next)
                {
                    if(section->type == XML_ELEMENT_NODE)
                    {
                        if(strcmp((const char*)section->name, "IN_NAME") == 0 && type == 'I')
                        {
                            strcpy(file, (const char*)section->children->content);
                            printf("%s\n", file); fflush(stdout);
                            return;
                        }
                        else if(strcmp((const char*)section->name, "OUT_NAME") == 0 && type == 'O')
                        {
                            strcpy(file, (const char*)section->children->content);
                            printf("%s\n", file); fflush(stdout);
                            xmlFreeDoc(doc);
                            xmlCleanupParser();
                            return;
                        }
                    }
                }
            }
        }
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
}
void modifyInput(char* correct, char* inputFile, int idTest)
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    xmlNode* cur_node = NULL, *section = NULL, *testNode, *indata, *outdata;
    int number, inputFD, test;
    char id[2];
    ofstream fout(inputFile);

    doc = xmlReadFile("../Problems.xml", NULL, 0);
    root_element = xmlDocGetRootElement(doc);
    if (doc == NULL)
        handle_error("[server] Eroare la parsarea fisierului \'Problems.xml\'.\n", 1);

    //inputFD = open(inputFile, O_RDWR);
    sprintf(id, "%d", problemCode);
    for(cur_node = root_element->children; cur_node; cur_node = cur_node->next)
    {
        if(cur_node->type == XML_ELEMENT_NODE && strcmp((const char*)cur_node->name, "PROBLEM") == 0)
        {
            number = atoi((const char*)xmlGetProp(cur_node, (const xmlChar*)"id"));
            if(number == problemCode)
            {
                for(section = cur_node->children; strcmp((const char*)section->name, "TESTS") != 0 && section; section = section->next);
                for(testNode = section->children; testNode; testNode = testNode->next)
                {
                    if(testNode->type == XML_ELEMENT_NODE && strcmp((const char*)testNode->name, "TEST") == 0)
                    {
                        test = atoi((const char*)xmlGetProp(testNode, (const xmlChar*)"id"));
                        if(test == idTest)
                        {

                            indata = testNode->children;
                            while(indata->type != XML_ELEMENT_NODE)
                                indata = indata->next;
                            fout << (const char*)indata->children->content;
                            outdata = indata->next;
                            while(outdata->type != XML_ELEMENT_NODE)
                                outdata = outdata->next;
                            strcpy(correct, (const char*)outdata->children->content);
                            xmlFreeDoc(doc);
                            xmlCleanupParser();
                            //close(inputFD);
                            return;
                        }
                    }
                }
            }
        }
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    //close(inputFD);
}
