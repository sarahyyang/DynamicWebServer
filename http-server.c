//
// http-server.c
// Dynamic web server
// 
// by Sarah Yang on 05/02/2022
//

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
#include <netdb.h>

static void die(const char *s) 
{ 
    perror(s);
    exit(1);
}

// sends error code to browser
// returns 1 on send fail, 0 on success
int sendError(char *respStat, int clntsock)
{
    char buf[1000];
    snprintf(buf, sizeof(buf), 
            "HTTP/1.0 %s\r\n"
            "\r\n"
            "<html><body><h1>%s</h1></body></html>",
            respStat, respStat);

    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf)) {
        close(clntsock);
        return 1;
    }
    return 0;
}

// sends 200 OK to browser
// returns 1 on send fail, 0 on success
int sendOK(int clntsock)
{
    char buf[1000];
    snprintf(buf, sizeof(buf),
            "HTTP/1.0 200 OK\r\n"
            "\r\n");
    
    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf)) {
        close(clntsock);
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, 
                "usage: %s <server_port> <web_root> <mdb-lookup-host> <mdb-lookup-port>\n",
                argv[0]);
        exit(1);
    }

    unsigned short port = atoi(argv[1]);
    char *webroot = argv[2];
    char *hostName = argv[3];
    unsigned short lookupPort = atoi(argv[4]);

    struct hostent *he;
    if ((he = gethostbyname(hostName)) == NULL)
        die("gethostbyname failed");
    char *ip = inet_ntoa(*(struct in_addr *)he->h_addr);
    
    const char *form = "<h1>mdb-lookup</h1>\n"
                       "<p>\n" 
                       "<form method=GET action=/mdb-lookup>\n" 
                       "lookup: <input type=text name=key>\n" 
                       "<input type=submit>\n" 
                       "</form>\n" 
                       "<p>\n";
  
    /*
     * client socket to mdb-lookup server
     */

    int sock; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");

    struct sockaddr_in servaddr1;
    memset(&servaddr1, 0, sizeof(servaddr1)); 
    servaddr1.sin_family      = AF_INET;
    servaddr1.sin_addr.s_addr = inet_addr(ip);
    servaddr1.sin_port        = htons(lookupPort);

    // Establish a TCP connection to the server
    if (connect(sock, (struct sockaddr *) &servaddr1, sizeof(servaddr1)) < 0)
        die("connect failed");

    FILE *lookupInput = fdopen(sock, "r");
        if (lookupInput == NULL)
            die("fdopen failed");
         
    /*
     * server socket to browser
     */

    // Create listening socket
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
        // Accept an browser connection
        clntlen = sizeof(clntaddr);
        if ((clntsock = accept(servsock,
                        (struct sockaddr *) &clntaddr, &clntlen)) < 0)
            continue;

        char *clntIP = inet_ntoa(clntaddr.sin_addr);
        char buf[4096];
        char *status;

        FILE *input = fdopen(clntsock, "r");
        if (input == NULL) {
            close(clntsock);
            continue;
        }
        
        char *requestLine;
        if ((requestLine = fgets(buf, sizeof(buf), input)) == NULL) {
            fprintf(stderr, "fgets received NULL\n");
            fclose(input);
            continue;
        }
       
        // get copy of request line without newline 
        char rq[strlen(requestLine)];
        strncpy(rq, requestLine, sizeof(rq));
        strtok(rq, "\r\n");

        char *token_separators = "\t \r\n"; 
        char *method = strtok(requestLine, token_separators);   // GET  
        char *requestURI = strtok(NULL, token_separators);  
        char *httpVer = strtok(NULL, token_separators);         // HTTP/1.0 or HTTP/1.1
        
        // get file absolute path
        char b[100];
        snprintf(b, sizeof(b), "%s", webroot);
        char *filepath = strcat(b, requestURI);
        if (requestURI[strlen(requestURI)-1] == '/')
            filepath = strcat(filepath, "index.html");

        // check if file is directory
        int isDir = 0;
        struct stat statBuf;
        if (stat(filepath, &statBuf) == 0) {
            if (S_ISDIR(statBuf.st_mode) != 0)
                isDir = 1;
        }
        
        char *URIsuffix = requestURI + strlen(requestURI) - 3;  // last 3 chars of URI
        


        if (strcmp(method, "GET") != 0 ||
            (strcmp(httpVer, "HTTP/1.0") != 0 && strcmp(httpVer, "HTTP/1.1") != 0)) {
            status = "501 Not Implemented";
            if (sendError(status, clntsock))
                continue;

        } else if ((requestURI[0] != '/') ||
                   (strstr(requestURI, "/../") != NULL) ||
                   (strcmp(URIsuffix, "/..") == 0)) {
            status = "400 Bad Request";
            if (sendError(status, clntsock))
                continue;

        } else if (isDir && 
                   (requestURI[strlen(requestURI)-1] != '/')) {
            status = "403 Forbidden";
            if (sendError(status, clntsock))
                continue;

        } else if (strcmp(requestURI, "/mdb-lookup") == 0) {
            status = "200 OK";
            if (sendOK(clntsock))
                continue;
            
            snprintf(buf, sizeof(buf),
                    "<html><body>\n"
                    "%s\n"
                    "</body></html>\n", form);
            if (send(clntsock, buf, strlen(buf), 0) != strlen(buf)) {
                close(clntsock);
                continue;
            }
        } else if (strncmp(requestURI, "/mdb-lookup?key=", strlen("/mdb-lookup?key=")) == 0) {
            char *keyword = requestURI + strlen("/mdb-lookup?key=");
           
            fprintf(stdout, "looking up [%s]: ", keyword);

            strcat(keyword, "\n");
            status = "200 OK";
            if (sendOK(clntsock))
                continue;

            // send submit form
            if (send(clntsock, form, strlen(form), 0) != strlen(form)) {
                close(clntsock);
                continue;
            }

            // send keyword to mdb-lookup server
            if (send(sock, keyword, strlen(keyword), 0) != strlen(keyword)) 
                die("send to server failed");

            char recBuf[500]; 
            int c = 0;

            snprintf(buf, sizeof(buf), "<p><table border>\n");
            if (send(clntsock, buf, strlen(buf), 0) != strlen(buf)) {
                close(clntsock);
                continue;
            }

            // get records from mdb-lookup server
            while (strcmp(fgets(recBuf, sizeof(recBuf), lookupInput), "\n") != 0)
            {
                c++;
                if (c % 2 == 0)
                    snprintf(buf, sizeof(buf), "<tr><td bgcolor=#EBF5FB> %s\n", recBuf);
                else 
                    snprintf(buf, sizeof(buf), "<tr><td bgcolor=#FEF9E7> %s\n", recBuf);
                
                if (send(clntsock, buf, strlen(buf), 0) != strlen(buf)) {
                    break;
                    close(clntsock);
                    continue;
                }
            }

            snprintf(buf, sizeof(buf), "</table>\n</body></html>\n");
            if (send(clntsock, buf, strlen(buf), 0) != strlen(buf)) {
                close(clntsock);
                continue;
            }

        } else {
            FILE *fp = fopen(filepath, "r");
            if (fp == NULL) {
                status = "404 Not Found";
                if (sendError(status, clntsock))
                    continue;
            } else {
                status = "200 OK";
                if (sendOK(clntsock))
                    continue;
                int n;
                while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
                    if (send(clntsock, buf, n, 0) != n) {
                        fclose(fp);
                        close(clntsock);
                        break;
                        continue;
                    }
                }
                fclose(fp);
            }
        }       

        fprintf(stdout, "%s \"%s\" %s\n", clntIP, rq, status);
        fclose(input);
    }

    fclose(lookupInput);
    return 0;
}
