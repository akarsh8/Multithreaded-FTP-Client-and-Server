#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX 1024

pthread_t threads;
pthread_t get_threads[MAX];
pthread_t put_threads[MAX];

typedef struct str_indata {
    char msg[MAX];
} indata;

int nport;
int tport;
char* ip;
int getID = 0;
int putID = 0;
char* farr[MAX];
int get_sarr[MAX] = {0};
int put_sarr[MAX] = {0};
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

void get_amp(int sid, char* msg, char* rsp) {
    int temp = getID;
    getIDs[temp] = 1;
    getID++;

    char* nof = "ERROR: No file found!";
    char* fer = "ERROR: Unable to write to file!\n";
    char* sig = "signal";
    char buff[100];
    char fn[100];
    int i;

    bzero(buff, sizeof(buff));
    bzero(fn, sizeof(fn));

    for (i = 5; i < strlen(msg) - 1; i++)
        fn[i - 5] = msg[i];
    
    for (i = 1; i < strlen(msg); i++)
        buff[i - 1] = msg[i];
    
    write(sid, buff, strlen(buff));
    read(sid, rsp, MAX);

    if (strncmp(rsp, nof, 21) != 0) {
        int cid = atoi(rsp);
        get_sarr[temp] = cid;

        printf("UPDATE: Command ID - %s.\n", rsp);
        bzero(rsp, strlen(rsp));

        farr[temp] = fn;
        int out = curr_file(mutexArr, fn);
        while (out == 1)
            out = curr_file(mutexArr, fn);
        mutexArr[temp] = fn;

        FILE* file = fopen(fn, "wb");
        if (file == NULL) {
            printf("%s", fer);
            close(sid);
            getIDs[temp] = 0;
            return;
        } //if

        write(sid, sig, strlen(sig));
        while (read(sid, rsp, 1000) > 0) {
            fwrite(rsp, sizeof(char), strlen(rsp), file);
            bzero(rsp, strlen(rsp));
        } //while

        fclose(file);
        mutexArr[temp] = NULL;
    } //if
    else
        printf("%s - %s", rsp, fn);
    
    close(sid);
    getIDs[temp] = 0;
} //get_amp

void put_amp(int sid, char* msg, char* rsp) {
    int temp = putID;
    putID++;
    putIDs[temp] = 1;

    char buff[MAX];
    char fn[100];
    int i;

    bzero(buff, sizeof(buff));
    bzero(fn, sizeof(fn));

    for (i = 5; i < strlen(msg) - 1; i++)
        fn[i - 5] = msg[i];
    
    farr[temp] = fn;
    int out = curr_file(mutexArr, fn);
    while (out == 1)
        out = curr_file(mutexArr, fn);
    mutexArr[temp] = fn;

    FILE* file = fopen(fn, "rb");
    if (file == NULL) {
        printf("ERROR: No file found - %s!\n", fn);
        putIDs[temp] = 0;
        return;
    } //if

    for (i = 1; i < strlen(msg); i++)
        buff[i - 1] = msg[i];
    
    write(sid, buff, strlen(buff));
    read(sid, rsp, MAX);
    
    int cid = atoi(rsp);
    printf("UPDATE: Command ID: %s\n", rsp);
    put_sarr[temp] = cid;
    bzero(rsp, strlen(rsp));
    bzero(buff, sizeof(buff));

    while (fread(buff, 1, 1000, file) > 0) {
        write(sid, buff, strlen(buff));
        bzero(buff, sizeof(buff));
        sleep(2);
    } //while

    fclose(file);
    mutexArr[temp] = NULL;
    close(sid);
    putIDs[temp] = 0;
} //put_amp

void nport_connect(void* ptr) {
    indata* data;
    data = (indata*) ptr;
    int sockID;
    char rsp[MAX];
    struct sockaddr_in server;
    int n;

    sockID = socket(AF_INET, SOCK_STREAM, 0);
    if (sockID < 0) {
        printf("ERROR: Unable to create socket for nport!\n");
        exit(errno);
    } //if

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(nport);

    if (connect(sockID, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("ERROR: Unable to connect to the server!\n");
        exit(errno);
    } //if

    if (strncmp(data->msg, "&get", 4) == 0)
        n = 1;
    else
        n = 2;

    bzero(rsp, sizeof(rsp));

    switch (n) {
        case 1:
            get_amp(sockID, data->msg, rsp);
            break;
        case 2:
            put_amp(sockID, data->msg, rsp);
            break;
    } //switch

    pthread_exit(NULL);
} //nport_connect

int get_id(int* arr, int cid) {
    int i = 0;

    for (i = 0; i < MAX - 1; i++) {
        if(arr[i] == cid)
            return i;
    } //for

    return -1;
} //get_id

void terminate(char* id) {
    int cid = atoi(id);
    int gid = get_id(get_sarr, cid);
    int pid = get_id(put_sarr, cid);

    if (getIDs[gid] == 1) {
        pthread_cancel(get_threads[gid]);
        mutexArr[gid] = NULL;
        remove(farr[gid]);

        printf("UPDATE: Get command with ID - %d has been terminated!\n", cid);
        printf("UPDATE: %s has been released!\n", farr[gid]);

        getIDs[gid] = 0;
    } //if
    else if (putIDs[pid] == 1) {
        pthread_cancel(put_threads[pid]);

        printf("UPDATE: Put command with ID - %d has been terminated!\n", cid);

        putIDs[pid] = 0;
    } //else if
    else
        printf("ERROR: No process with command ID - %d found in client!\n", cid);
} //terminate

void tport_connect(char* id) {
    int sockID;
    char buff[MAX];
    char rsp[MAX];
    struct sockaddr_in server;

    sockID = socket(AF_INET, SOCK_STREAM, 0);
    if (sockID < 0) {
        printf("ERROR: Unable to create socket for tport!\n");
        exit(errno);
    } //if

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(tport);

    if (connect(sockID, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("ERROR: Unable to connect to the server!\n");
        exit(errno);
    } //if

    printf("UPDATE: Command ID - %s.", id);
    terminate(id);
    write(sockID, id, strlen(id));

    pthread_exit(NULL);
} //tport_connect

void get(int sid, char* req, char* rsp) {
    char* nof = "ERROR: No file found!";
    char* fer = "ERROR: Unable to write to file!\n";
    char* sig = "signal";
    char fn[100];
    int i;

    int temp = getID;
    getIDs[temp] = 1;
    getID++;

    bzero(fn, sizeof(fn));

    for (i = 4; i < strlen(req) - 1; i++)
        fn[i - 4] = req[i];
    
    write(sid, req, strlen(req));
    read(sid, rsp, MAX);

    if (strncmp(rsp, nof, 21) != 0) {
        int cid = atoi(rsp);
        printf("UPDATE: Command ID - %s\n", rsp);
        bzero(rsp, strlen(rsp));
        write(sid, sig, strlen(sig));

        farr[temp] = fn;
        int out = curr_file(mutexArr, fn);
        while (out == 1)
            out = curr_file(mutexArr, fn);
        mutexArr[temp] = fn;

        FILE* file = fopen(fn, "wb");

        if (file == NULL) {
            printf("%s", fer);
            close(sid);
            return;
        } //if

        while (read(sid, rsp, 1000) > 0) {
            fwrite(rsp, sizeof(char), strlen(rsp), file);
            if (strlen(rsp) < 1000)
                printf("UPDATE: ALL bytes of file - %s has been received. (Command ID - %d)\n", fn, cid);
            else
                printf("UPDATE: 1000 bytes of file - %s has been received. (Command ID - %d)\n", fn, cid);
            bzero(rsp, strlen(rsp));
        } //while

        fclose(file);
        printf("UPDATE: File - %s received from server.\n", fn);
        mutexArr[temp] = NULL;
    } //if
    else
        printf("%s: %s", rsp, fn);
    
    close(sid);
    getIDs[temp] = 0;
} //get

void put(int sid, char* req, char* rsp) {
    int temp = putID;
    putID++;
    putIDs[temp] = 1;

    char buff[MAX];
    char fn[100];
    int i;

    bzero(buff, sizeof(buff));
    bzero(fn, sizeof(fn));

    for (i = 4; i < strlen(req) - 1; i++)
        fn[i - 4] = req[i];
    
    farr[temp] = fn;
    int out = curr_file(mutexArr, fn);
    while (out == 1)
        out = curr_file(mutexArr, fn);
    mutexArr[temp] = fn;

    FILE* file = fopen(fn, "rb");
    if (file == NULL) {
        printf("ERROR: No file found - %s!\n", fn);
    } //if

    write(sid, req, strlen(req));
    read(sid, rsp, MAX);
    
    int cid = atoi(rsp);
    printf("UPDATE: Command ID - %s.\n", rsp);

    bzero(rsp, strlen(rsp));

    while (fread(buff, 1, 1000, file) > 0) {
        write(sid, buff, strlen(buff));
        if (strlen(buff) < 1000)
            printf("UPDATE: ALL bytes of file - %s has been transferred. (Command ID - %d)\n", fn, cid);
        else
            printf("UPDATE: 1000 bytes of file - %s has been transferred. (Command ID - %d)\n", fn, cid);
        bzero(buff, sizeof(buff));
        sleep(2);
    } //while

    fclose(file);
    mutexArr[temp] = NULL;
    close(sid);
    putIDs[temp] = 0;
} //put

void quit(int sid, char* req) {
    write(sid, req, strlen(req));
    printf("UPDATE: The session has successfully ended.\n");

    exit(1);
} //quit

void cmd(int sid, char* req, char* rsp) {
    write(sid, req, strlen(req));
    read(sid, rsp, MAX);
    printf("%s", rsp);

    close(sid);
} //cmd

void sock_connect() {
    int sockID;
    char rsp[MAX];
    char buff[MAX];
    char id_str[10];
    struct sockaddr_in server;
    indata data;
    int n;

    while (1) {
        bzero(buff, sizeof(buff));
        bzero(id_str, sizeof(id_str));
        bzero(data.msg, sizeof(data.msg));

        printf("\nmyftp> ");
        //Here to get user input
        fgets(buff, sizeof(buff), stdin);

        strcpy(data.msg, buff);
        bzero(rsp, sizeof(rsp));

        if (strncmp(buff, "terminate", 9) == 0) {
            int i;
            for (i = 0; i <= (strlen(buff) - 9); i++)
                id_str[i] = buff[i + 10];
            
            pthread_t term_threads;
            pthread_create(&term_threads, NULL, (void*)&tport_connect, &id_str);
            sleep(1);
        } //if
        else if (strncmp(buff, "&get", 4) == 0) {
            pthread_create(&get_threads[getID], NULL, (void*)&nport_connect, (void*)&data);
            sleep(1);
        } //else if
        else if (strncmp(buff, "&put", 4) == 0) {
            pthread_create(&put_threads[putID], NULL, (void*)&nport_connect, (void*)&data);
            sleep(1);
        } //else if
        else {
            sockID = socket(AF_INET, SOCK_STREAM, 0);
            if (sockID < 0) {
                printf("ERROR: Unable to create socket for nport!\n");
                exit(errno);
            } //if

            bzero(&server, sizeof(server));
            server.sin_family = AF_INET;
            server.sin_addr.s_addr = inet_addr(ip);
            server.sin_port = htons(nport);

            if (connect(sockID, (struct sockaddr*)&server, sizeof(server)) < 0) {
                printf("ERROR: Unable to connect to the server!\n");
                exit(errno);
            } //if

            if (strncmp(buff, "get", 3) == 0)
                n = 1;
            else if (strncmp(buff, "put", 3) == 0)
                n = 2;
            else if (strncmp(buff, "quit", 4) == 0)
                n = 3;
            else
                n = 0;
            
            switch(n) {
                case 1:
                    get(sockID, buff, rsp);
                    break;
                case 2:
                    put(sockID, buff, rsp);
                    break;
                case 3:
                    quit(sockID, buff);
                    break;
                default:
                    cmd(sockID, buff, rsp);
            } //switch
        } //else
    } //while
} //connect

int main(int argc, char** argv) {

    if (argc != 4)
        printf("ERROR: Invalid number of arguments! The server IP and two port numbers are required.\n");
    else {
        ip = argv[1];
        nport = atoi(argv[2]);
        tport = atoi(argv[3]);

        pthread_create(&threads, NULL, (void*)&sock_connect, NULL);
        pthread_join(threads, NULL);
    } //else

    return 0;

} //main