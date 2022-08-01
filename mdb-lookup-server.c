#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "mdb.h"
#include "mylist.h"

static void die(const char *s) 
{
    perror(s);
    exit(1);
}

int main(int argc, char **argv)
{
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)  
        die("signal() failed"); 

    if (argc != 3) {
        fprintf(stderr, "usage: %s <database-file> <server-port>\n", argv[0]);
        exit(1);
    }

    unsigned short port = atoi(argv[2]);

    // Create a listening socket
    int servsock;
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");

    // Construct local address structure
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    // Bind to the local address
    if (bind(servsock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        die("bind failed");

    // Start listening for incoming connections
    if (listen(servsock, 5) < 0)
        die("listen failed");

    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;

    while (1) {

        // Accept an incoming connection
        clntlen = sizeof(clntaddr);

        if ((clntsock = accept(servsock,
                        (struct sockaddr *) &clntaddr, &clntlen)) < 0)
            die("accept failed");

        fprintf(stderr, "\nconnection started from: %s\n", 
                inet_ntoa(clntaddr.sin_addr));

        //
        // lookup in databse
        //
        FILE *fp = fopen(argv[1], "rb");
        if (fp == NULL)
            die("fopen failed");

        // read records into memory
        struct Node *prevNode = NULL;
        struct List list;
        initList(&list);

        char nameBuf[16];
        char msgBuf[24];
        while (fread(&nameBuf, 16, 1, fp) >= 1)
        {
            fread(&msgBuf, 24, 1, fp);

            struct MdbRec *new = malloc(sizeof(struct MdbRec));
            strcpy(new->name, nameBuf);
            strcpy(new->msg, msgBuf);

            prevNode = addAfter(&list, prevNode, new);
            if (prevNode == NULL)
                die("addAfter() failed");
        }

        fclose(fp);

        // lookup 
        FILE *input = fdopen(clntsock, "r");
        char line[1000];
        char key[6];

        while (fgets(line, sizeof(line), input) != NULL)
        {
            strncpy(key, line, sizeof(key)-1);
            key[sizeof(key)-1] = '\0';

            size_t last = strlen(key) - 1;
            if (key[last] == '\n')
                key[last] = '\0';

            int lineno = 1;
            struct Node *curr = list.head;
            while (curr)
            {
                struct MdbRec *rec = curr->data;
                if (strstr(rec->name, key) || strstr(rec->msg, key))
                {
                    char tmp[1000];
                    int written = snprintf(tmp, sizeof(tmp), "%4d: {%s} said {%s}\n", 
                            lineno, rec->name, rec->msg);
                    if (written < 0  || written > sizeof(tmp))
                        fprintf(stderr, "ERROR: snprintf failed\n");

                    if (send(clntsock, tmp, strlen(tmp), 0) != strlen(tmp))
                        fprintf(stderr, "ERROR: send failed\n");
                }

                lineno++;
                curr = curr->next;
            }

            char nl[2];
            snprintf(nl, sizeof(nl), "\n");
            send(clntsock, nl, 2, 0);
        }
        
        if (ferror(input))
            die("fgets failed");
 
        fclose(input);
        fprintf(stderr, "connection terminated from %s\n", inet_ntoa(clntaddr.sin_addr));

        // clean up
        struct Node *p = list.head;
        while (p != NULL)
        {
            struct Node *tmp = p->next;
            free(p->data);
            free(p);
            p = tmp;
        }

    }

    return 0;
}
