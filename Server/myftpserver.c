#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX 1024

pthread_t threads[MAX];

int thread_id = 0;
int sarr[MAX] = {0};
char* farr[MAX];
int getIDs[MAX] = {0};
int putIDs[MAX] = {0};
char* mutexArr[MAX] = {NULL};

int curr_file(char** marr, char* fn) {
    int i = 0;

    for (i = 0; i < MAX - 1; i++) {
        if ((marr[i] != NULL) && (strcmp(marr[i], fn) == 0))
            return 1;
    } //for

    return 0;
} //curr_file

void get(int sid, char* in) {
    char buff[MAX];
    char fn[100];
    char tid_str[10];
    int i;
    char* ferr = "ERROR: No file found!";

    bzero(buff, sizeof(buff));
    bzero(fn, sizeof(fn));

    int tid = sarr[sid];
    sprintf(tid_str, "%d", tid);
    getIDs[tid] = 1;

    for (i = 4; i < strlen(in) - 1; i++)
        fn[i - 4] = in[i];
        
    FILE* file = fopen(fn, "rb");
    if (file == NULL) {
        printf("ERROR: No file found - %s\n", fn);
        write(sid, ferr, strlen(ferr));
        getIDs[tid] = 0;
        return;
    } //if

    write(sid, tid_str, strlen(tid_str));

    farr[tid] = fn;
    int out = curr_file(mutexArr, fn);
    while (out == 1)
        out = curr_file(mutexArr, fn);
    mutexArr[tid] = fn;
    printf("Server Status: %s is locked\n", farr[tid]);

    read(sid, buff, sizeof(buff));
    printf("Server Status: transferring file - %s\n", buff);
    bzero(buff, sizeof(buff));

    while (fread(buff, 1, 1000, file) > 0) {
        write(sid, buff, strlen(buff));

        if (strlen(buff) < 1000)
            printf("Server Status: transferred ALL bytes of file - %s (Command ID - %d)\n", fn, tid);
        else {
            printf("Server Status: transferred 1000 bytes of file - %s (Command ID - %d)\n", fn, tid);
            printf("Server Status: waiting for any terminate command...\n");
        } //else

        bzero(buff, sizeof(buff));
        sleep(2);
    } //while

    fclose(file);
    mutexArr[tid] = NULL;
    printf("Server Status: file - %s has been released\n", farr[tid]);
    close(sid);
    getIDs[tid] = 0;
} //get

void put(int sid, char* in) {
    char buff[MAX];
    char fn[100];
    char tid_str[10];
    int i;

    int tid = sarr[sid];
    sprintf(tid_str, "%d", tid);
    putIDs[tid] = 1;

    write(sid, tid_str, strlen(tid_str));

    bzero(buff, sizeof(buff));
    bzero(fn, sizeof(fn));

    for (i = 4; i < strlen(in) - 1; i++)
        fn[i - 4] = in[i];
    
    farr[tid] = fn;
    int out = curr_file(mutexArr, fn);
    while (out == 1)
        out = curr_file(mutexArr, fn);

    mutexArr[tid] = fn;
    printf("Server Status: %s is locked\n", farr[tid]);

    FILE* file = fopen(fn, "wb");
    if (file == NULL) {
        printf("ERROR: Unable to write to file - %s!\n", fn);
        putIDs[tid] = 0;
        return;
    } //if

    while (read(sid, buff, 1000) > 0) {
        fwrite(buff, sizeof(char), strlen(buff), file);

        if (strlen(buff) < 1000)
            printf("Server Status: loaded ALL bytes of file - %s (Command ID - %d)\n", fn, tid);
        else {
            printf("Server Status: loaded 1000 bytes of file - %s (Command ID - %d)\n", fn, tid);
            printf("Server Status: waiting for any terminate command...\n");
        } //else

        bzero(buff, sizeof(buff));
        sleep(2);
    } //while

    fclose(file);
    printf("Server Status: get file - %s from client completed successfully\n", fn);

    mutexArr[tid] = NULL;
    printf("Server Status: file - %s has been released\n", farr[tid]);

    putIDs[tid] = 0;
} //put

void delete(int sid, char* in) {
    char buff[100];
    char* success = "UPDATE: The file has been successfully deleted!\n";
    char* failure = "ERROR: Unable to delete the file!\n";
    int i;

    bzero(buff, sizeof(buff));

    for (i = 7; i < strlen(in) - 1; i++)
        buff[i - 7] = in[i];
    
    if (remove(buff) != 0) {
        printf("%s\n", failure);
        write(sid, failure, strlen(failure));
    } //if
    else {
        printf("%s\n", success);
        write(sid, success, strlen(success));
    } //else
} //delete

void ls(int sid, char* in) {
    FILE* fp;
    char buff[MAX];
    char out[MAX] = "";

    fp = popen(in, "r");
    while (fgets(buff, sizeof(buff), fp) != NULL)
        strcat(out, buff);
    
    write(sid, out, sizeof(out));
    pclose(fp);
} //ls

void cd(int sid, char* in) {
    char buff[100];
    char* success = "UPDATE: The directory has been successfully changed!\n";
    char* failure = "ERROR: Unable to change the directory!\n";
    int i;

    bzero(buff, sizeof(buff));
    for (i = 3; i < strlen(in) - 1; i++)
        buff[i - 3] = in[i];

    if (chdir(buff) != 0) {
        printf("%s\n", failure);
        write(sid, failure, strlen(failure));
    } //if
    else {
        printf("%s\n", success);
        write(sid, success, strlen(success));
    } //else
} //cd

void md(int sid, char* in) {
    char buff[100];
    char* success = "UPDATE: The directory has been successfully created!\n";
    char* failure = "ERROR: Unable to create the directory!\n";
    int i;

    bzero(buff, sizeof(buff));
    for (i = 6; i < strlen(in) - 1; i++)
        buff[i - 6] = in[i];
    
    if (mkdir(buff, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH) < 0) {
        printf("%s\n", failure);
        write(sid, failure, strlen(failure));
    } //if
    else {
        printf("%s\n", success);
        write(sid, success, strlen(success));
    } //else
} //md

void pwd(int sid) {
    FILE* fp;
    char buff[MAX];

    system("pwd");
    fp = popen("pwd", "r");
    fgets(buff, sizeof(buff), fp);
    write(sid, buff, sizeof(buff));
    pclose(fp);
} //pwd

void quit() {
    printf("Server Status: client disconnected from server\n");
} //quit

void cmd(int sid, char* in) {
    int n;
    char* err = "ERROR: Invalid command!\n";

    if (strncmp(in, "get", 3) == 0)
        n = 1;
    else if (strncmp(in, "put", 3) == 0)
        n = 2;
    else if (strncmp(in, "delete", 6) == 0)
        n = 3;
    else if (strncmp(in, "ls", 2) == 0)
        n = 4;
    else if (strncmp(in, "cd", 2) == 0)
        n = 5;
    else if (strncmp(in, "mkdir", 5) == 0)
        n = 6;
    else if (strncmp(in, "pwd", 3) == 0)
        n = 7;
    else if (strncmp(in, "quit", 4) == 0)
        n = 8;
    else
        n = 0;

    switch (n) {
        case 1:
            get(sid, in);
            break;
        case 2:
            put(sid, in);
            break;
        case 3:
            delete(sid, in);
            break;
        case 4:
            ls(sid, in);
            break;
        case 5:
            cd(sid, in);
            break;
        case 6:
            md(sid, in);
            break;
        case 7:
            pwd(sid);
            break;
        case 8:
            quit();
            break;
        default:
            printf("ERROR: Invalid command!\n");
            write(sid, err, strlen(err));
    } //switch
} //cmd

void nthread_process(int sock) {
    char buff[MAX];

    bzero(buff, MAX);
    read(sock, buff, MAX);
    cmd(sock, buff);
} //nthread_process

void nport_connect(int port) {
    int sockID;
    int sid;
    char buff[MAX];
    struct sockaddr_in server;

    printf("Server Status: nport number set to %d\n", port);

    sockID = socket(AF_INET, SOCK_STREAM, 0);
    if (sockID < 0) {
        printf("ERROR: Unable to create socket for nport!\n");
        exit(errno);
    } //if

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(sockID, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("ERROR: Unable to bind socket for nport!\n");
        exit(errno);
    } //if

    listen(sockID, MAX);

    while ((sid = accept(sockID, (struct sockaddr*)NULL, NULL)) > 0) {
        sarr[sid] = thread_id;
        pthread_create(&threads[thread_id], NULL, (void*)&nthread_process, (int*)sid);
        thread_id++;
    } //while
} //nport_connect

void terminate(char* in) {
    int tid = atoi(in);

    if (putIDs[tid] == 1) {
        pthread_cancel(threads[tid]);
        mutexArr[tid] = NULL;
        remove(farr[tid]);

        printf("Server Status: put command with ID - %d has been terminated\n", tid);

        putIDs[tid] = 0;
    } //if
    else if (getIDs[tid] == 1) {
        pthread_cancel(threads[tid]);

        printf("Server Status: get command with ID - %d has been terminated\n", tid);

        getIDs[tid] = 0;
    } //else if
    else
        printf("ERROR: No process with command ID - %d found in server!\n", tid);
} //terminate

void tthread_process(int sock) {
    char buff[MAX];

    bzero(buff, MAX);
    read(sock, buff, MAX);
    terminate(buff);
} //tthread_process

void tport_connect(int port) {
    int sockID;
    int sid;
    char buff[MAX];
    struct sockaddr_in server;

    printf("Server Status: tport number set to %d\n", port);

    sockID = socket(AF_INET, SOCK_STREAM, 0);
    if (sockID < 0) {
        printf("ERROR: Unable to create socket for tport!\n");
        exit(errno);
    } //if

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    if (bind(sockID, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("ERROR: Unable to bind socket for tport!\n");
        exit(errno);
    } //if

    listen(sockID, 10);

    while ((sid = accept(sockID, (struct sockaddr*)NULL, NULL)) > 0) {
        pthread_t tthread;
        pthread_create(&threads, NULL, (void*)&tthread_process, (int*)sid);
    }
} //tport_connect

int main(int argc, char** argv) {

    int nport;
    int tport;
    pthread_t npthread;
    pthread_t tpthread;

    if (argc != 3)
        printf("ERROR: Invalid number of arguments! Two port numbers are required.\n");
    else {
        nport = atoi(argv[1]);
        tport = atoi(argv[2]);

        if (nport > 1024 && nport < 65535 && tport > 1024 && tport < 65535) {
            pthread_create(&npthread, NULL, (void*)&nport_connect, (int*)nport);
            pthread_create(&tpthread, NULL, (void*)&tport_connect, (int*)tport);
            pthread_join(npthread, NULL);
            pthread_join(tpthread, NULL);
        } //if
        else
            printf("ERROR: Port number out of range!\n");
    } //else

    return 0;

} //main