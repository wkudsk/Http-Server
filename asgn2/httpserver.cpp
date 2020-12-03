#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <err.h>
#include <pthread.h>
#include <deque>

using namespace std;

unsigned long int offset;
int logfd;
// Declaration of thread condition variable 
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER; 
  
// declaring mutex 
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;


pthread_mutex_t writeLogLock = PTHREAD_MUTEX_INITIALIZER;

int incrementOffset(const char* logString) {
    pthread_mutex_lock(&writeLogLock);
    int localOffset = offset;
    offset = offset + strlen(logString);
    pthread_mutex_unlock(&writeLogLock);
    pwrite(logfd, logString, strlen(logString), localOffset);
    return 1;
}
int internalServerError(int socket, const char* fileName) {
    char internalMessage[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
    send(socket, internalMessage, strlen(internalMessage), 0);
    
    if (logfd != -1) {
        char logString[73] = "FAIL: PUT ";
        strcat(logString, fileName);
        strcat(logString, " HTTP/1.1 --- response 500\n========\n");
        
        incrementOffset(logString);
    }
    return -1;
}

int logPut(long int newLineLog, char *buffer, long int localOffset) {
    char logString[69] = "";
    if(newLineLog % 20 == 0) {
        if(newLineLog == 0) {
            sprintf(logString, "%08ld ", newLineLog);
        }
    }
    for(int i = 0; i < (int)strlen(buffer); i++) {    
        
        sprintf(logString, "%s%02x ", logString, buffer[i]);

        //cout << newLineLog << endl;

        newLineLog++;
        if(newLineLog % 20 == 0) {
            
            pwrite(logfd, logString, strlen(logString), localOffset);
            localOffset = localOffset + strlen(logString);
            sprintf(logString, "\n%08ld ", newLineLog);
            //cout << newLineLog << endl;
        }
        
        
        //cout << logString << endl;
        
    }

    pwrite(logfd, logString, strlen(logString), localOffset);
    localOffset = localOffset + strlen(logString);
            
    return localOffset;
}

int put(int socket, const char *fileName, int contentLength) {
    // Check if we have a content length defined
    
    if (access(fileName, F_OK) == -1) {
        int fd = creat(fileName, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            //cout << fileName << endl;
            return internalServerError(socket, fileName);
        }
        else {
            char createMessage[] = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n";
            send(socket, createMessage, strlen(createMessage), 0);
            //cout << "Created file" << endl;
        }
        //fd = open(headerParts[1].c_str(), 0);
    }
    else if(access(fileName, W_OK) == -1) {
        char forbiddenMessage[] = "HTTP/1.1 403 Forbidden\r\n\r\n";
            send(socket, forbiddenMessage, strlen(forbiddenMessage), 0);
            
            if (logfd != -1) {
                char logString[73] = "FAIL: PUT ";
                strcat(logString, fileName);
                strcat(logString, " HTTP/1.1 --- response 403\n========\n");
                
                incrementOffset(logString);
            }
            return -1;
    }
    //cout << fd << endl;
    int fd = open(fileName, O_WRONLY | O_TRUNC);
    if (fd == -1) {
        //cout << fileName << endl;
        return internalServerError(socket, fileName);
    }
    char okayMessage[] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n";
    send(socket, okayMessage, strlen(okayMessage), 0);
    //cout << "Sent okay message" << endl;

    //cout << contentLength << endl;
    if (contentLength == -1) {
        
        if (logfd != -1) {
            char logString[41] = "PUT ";
            strcat(logString, fileName);
            sprintf(logString, "%s length 0\n", logString);
            
            incrementOffset(logString);
               
        }

        int closeSocket = 4095;
        //cout << "entering loop for PUT w/o content length" << endl;
        while (closeSocket > 0) {
            char buffer[4095] = "";

            closeSocket = recv(socket, buffer, sizeof(buffer), 0);

            //cout << "writing to file" << endl;
            int q = write(fd, buffer, strlen(buffer));
            if (q < 0) {
                cout << "couldnt write value " << endl;
                return -1;
            }
        }
    }
    else {
        
        unsigned long int localOffset;
        if (logfd != -1) {
            char logString[50] = "PUT ";
            strcat(logString, fileName);
            sprintf(logString, "%s length %d\n", logString, contentLength);
            int lessThanTwenty = 1;
            if(contentLength % 20  == 0) {
                lessThanTwenty = 0;
            }
            //cout << logString << endl;
            pthread_mutex_lock(&writeLogLock);
            localOffset = offset;
            offset = offset + strlen(logString) + (contentLength*3) + (((lessThanTwenty + contentLength/20))*10) + 9;
            //cout << offset << endl;
            pthread_mutex_unlock(&writeLogLock);
            pwrite(logfd, logString, strlen(logString), localOffset);
            localOffset = localOffset + strlen(logString);
            
        }

        unsigned long int newLineLog = 0;
        
        //cout << "Content Length before entering loop: " << contentLength << endl;

        while (contentLength > 0) {
            char buffer[4095] = "";
            
            int p = recv(socket, buffer, sizeof(buffer), 0);
            if (p < 0) {
                cout << "couldn't recieve value" << endl;
                return -1;
            }
            //cout << "Buffer size: " << strlen(buffer) << endl;

            //cout << buffer << endl;
            if (logfd != -1) {
                //cout << "Content Length before logPut: " << contentLength << endl;
                localOffset = logPut(newLineLog, buffer, localOffset);
                newLineLog =  newLineLog + strlen(buffer);
                
                //cout << "Content Length after logPut: " << contentLength << endl;
            }
            
            contentLength = contentLength - (int)strlen(buffer);

            int q = write(fd, buffer, strlen(buffer));
            if (q < 0) {
                cout << "couldnt send value " << endl;
                return -1;
            }
        
        }
        if(newLineLog % 20 != 0) {
            pwrite(logfd, "\n========\n", 10, localOffset);
            
        }
        else {
            pwrite(logfd, "========\n", 9, localOffset);    
        }
            
    }
    //cout << "End of function" << endl;
    close(socket);
    close(fd);
    return 0;
}

int get(int socket, const char *fileName) {
    //cout << "GET Request" << endl;
    
    if(access(fileName, R_OK) == -1) {
        char forbiddenMessage[] = "HTTP/1.1 403 Forbidden\r\n\r\n";
            send(socket, forbiddenMessage, strlen(forbiddenMessage), 0);
            
            if (logfd != -1) {
                char logString[73] = "FAIL: GET ";
                strcat(logString, fileName);
                strcat(logString, " HTTP/1.1 --- response 403\n========\n");
                
                incrementOffset(logString);

            }
            return -1;
    }

    int fd = open(fileName, 0);
    //cout << fd << endl;
    
    
    if (fd == -1) {
        char notFoundMessage[] = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(socket, notFoundMessage, strlen(notFoundMessage), 0);
        
        if (logfd != -1) {
            char logString[73] = "FAIL: GET ";
            strcat(logString, fileName);
            strcat(logString, " HTTP/1.1 --- response 404\n========\n");
            
            incrementOffset(logString);
            
        }
        close(socket);
        return -1;
    }

    else {

        if (logfd != -1) {
        char logString[50] = "GET ";
        strcat(logString, fileName);
        strcat(logString, " length 0\n========\n");
        
        incrementOffset(logString);
                
        }

        int eof = 4095;
        while (eof != 0) {
            char buff[4096] = "";
            eof = read(fd, buff, eof);
            //cout << eof << endl;
            unsigned int q = send(socket, buff, strlen(buff), 0);
            if (q < 0) {
                cout << "couldnt send value " << endl;
                exit(-1);
            }
        }
        
        
        close(fd);
        close(socket);
    }
    return 0;
}

static void * handleClient(deque<unsigned int>* clients) {
    while(1) {
        
        //cout << "Client size: " << clients->size() << endl;
        pthread_mutex_lock(&lock);
        while(clients->size() <= 0) {
            
            pthread_cond_wait(&cond1, &lock);
        
        }
            
        
        int newsockfd = (clients->front());
        clients->pop_front();
        pthread_mutex_unlock(&lock);
    
        //cout << "Socket: "<< newsockfd << endl;
        char header[4096] = "";

        unsigned int p = recv(newsockfd, header, sizeof(header), 0);
        if (p < 0) {
            cout << "couldnt recieve value" << endl;
            exit(-1);
        }

        //cout << header << endl;

        stringstream parseHeader(header);
        vector<string> headerParts;
        string word;
        parseHeader >> word;
        headerParts.push_back(word);
        parseHeader >> word;
        headerParts.push_back(word);
        parseHeader >> word;
        headerParts.push_back(word);

        bool hasContentLength = false;
        int contentLength = -1;
        //cout << contentLength << endl;
        char *contentPointer = strstr(header, "Content-Length:");

        //cout << "Value is NULL: " << (contentPointer == NULL) << endl;

        if (contentPointer != NULL) {
            hasContentLength = true;
            while (word.compare("Content-Length:") != 0) {
                parseHeader >> word;
                //cout << word << endl;
                headerParts.push_back(word);
            }

            parseHeader >> contentLength;

            
        }
        else {
            parseHeader >> word;
            headerParts.push_back(word);
        }

        string requestType = headerParts[0];
        const char *fileName = headerParts[1].c_str();
        if(strlen(fileName) != 27){
            char notFoundMessage[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
            send(newsockfd, notFoundMessage, strlen(notFoundMessage), 0);
            
            if (logfd != -1) {
                char logString[73] = "FAIL: ";
                strcat(logString, requestType.c_str());
                strcat(logString, " ");
                strcat(logString, fileName);
                strcat(logString, " HTTP/1.1 --- response 400\n========\n");
                
                incrementOffset(logString);
                
            }
        }
        else {
            // GET function
            if (requestType.compare("GET") == 0) {
                get(newsockfd, fileName);
            }

            // PUT function
            else if (requestType.compare("PUT") == 0) {
                //cout << "Content Length is: " << contentLength << endl;
                put(newsockfd, fileName, contentLength);
            }
            else {
                char internalMessage[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                send(newsockfd, internalMessage, strlen(internalMessage), 0);
                
                if (logfd != -1) {
                    char logString[73] = "FAIL: ";
                    strcat(logString, requestType.c_str());
                    strcat(logString, " ");
                    strcat(logString, fileName);
                    strcat(logString, " HTTP/1.1 --- response 500\n========\n");
                    
                    incrementOffset(logString);
                }
            }

        }
        
        close(newsockfd);
    
    }
    
}

// main function -
// where the execution of program begins
int main(int argc, char *argv[]) {
    //cout << argc << endl;
    int port = 80;
    offset = 0;
    logfd = -1;
    int numThreads = 6;
    bool givenHostname = false;
    struct hostent *server;
    int opt;
    const char* logFile;
    
    while( (opt = getopt(argc, argv, "N:l:")) != -1) {
        switch (opt) {
            case 'N':
            {
                //printf("option: %s\n", (optarg));
                numThreads = atoi(optarg);
                
            } break;
            case 'l':
            {
                //printf("option: %s\n", optarg);
                logFile = optarg;
                logfd = open(logFile, O_WRONLY | O_TRUNC);
                if(logfd == -1){
                    logfd = creat(logFile, S_IRUSR | S_IWUSR);
                }
            } break;
            default: 
                break;
        }
    }
    
    server = gethostbyname(argv[optind]);    
    //cout << argv[optind] << endl;
        
    optind++;    

    if(optind < argc) {
        port = atoi(argv[optind]);
        //cout << port << endl;
    }

    //cout << server << endl;
    //cout << logfd << endl;    

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cout << "Exit because of connection error" << endl;
        exit(-1);
    }
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (givenHostname)
        bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
    else
        server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        cout << "Exit because of bind error" << endl;
        exit(-1);
    }
    if (listen(sockfd, 3) < 0) {
        cout << "listen" << endl;
        exit(-1);
    }

    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    
    

    deque<int> clients;
    vector<pthread_t> threads;


    for(int i = 0; i < numThreads; i++) {
        pthread_t thread;
        pthread_create(&thread, NULL, (void *(*)(void *))handleClient, &clients);
        threads.push_back(thread);
    }
    
    while (1) {

        int newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
        if (newsockfd < 0) {
            cout << "Exit because cant connect to client" << endl;
        }
        
        clients.push_back(newsockfd);
        
        pthread_cond_signal(&cond1);
    }
}