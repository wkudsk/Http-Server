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

unsigned int MAX_CACHE_SIZE = 4;
unsigned int offset;
int logfd;
bool cacheBit;
deque<string> cache;
deque<string> fileNames;
// Declaration of Cache portions and log portions 

int writeLog(const char* logString) {
    //cout << offset << endl;
    int localOffset = offset;
    offset = offset + strlen(logString);
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
        
        writeLog(logString);
    }
    return -1;
}

int logPut(long int newLineLog, char *buffer) {
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
            
            writeLog(logString);
            sprintf(logString, "\n%08ld ", newLineLog);
            //cout << newLineLog << endl;
        }
        //cout << logString << endl;      
    }

    writeLog(logString);      
    return 1;
}

int writeToFile(const char *fileName, string buffer) {
    int fd = open(fileName, O_WRONLY | O_TRUNC);

    int q = write(fd, buffer.c_str(), strlen(buffer.c_str()));
    if (q < 0) {
        cout << "couldnt write value " << endl;
        return -1;
    }
    close(fd);
    return 1;
}

int putWithCache(int socket, string fileName, int contentLength) {
    if (access(fileName.c_str(), F_OK) == -1) {
        int fd = creat(fileName.c_str(), S_IRUSR | S_IWUSR);
        if (fd == -1) {
            //cout << fileName << endl;
            return internalServerError(socket, fileName.c_str());
        }
        else {
            char createMessage[] = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n";
            send(socket, createMessage, strlen(createMessage), 0);
            //cout << "Created file" << endl;
        }
        //fd = open(headerParts[1].c_str(), 0);
    }
    else if(access(fileName.c_str(), W_OK) == -1) {
        char forbiddenMessage[] = "HTTP/1.1 403 Forbidden\r\n\r\n";
        send(socket, forbiddenMessage, strlen(forbiddenMessage), 0);
            
            if (logfd != -1) {
                char logString[73] = "FAIL: PUT ";
                strcat(logString, fileName.c_str());
                strcat(logString, " HTTP/1.1 --- response 403\n========\n");
                
                writeLog(logString);
            }
            return -1;
    }
    //cout << fd << endl;
    int fd = open(fileName.c_str(), O_WRONLY);
    if (fd == -1) {
        //cout << fileName << endl;
        return internalServerError(socket, fileName.c_str());
    }
    char okayMessage[] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n";
    send(socket, okayMessage, strlen(okayMessage), 0);
    //cout << "Sent okay message" << endl;

    close(fd);

    bool isInCache = false;
    int positionInCache = -1;
    for(unsigned int i = 0; i < fileNames.size(); i ++) {
        if(0 == fileName.compare(fileNames[i])) {
            isInCache = true;
            positionInCache = i;
        }
    }

    if(isInCache) {
    }
    else {
        if(fileNames.size() == MAX_CACHE_SIZE) {
            string oldFile = fileNames.front();
            fileNames.pop_front();
            string buffer = cache.front();
            cache.pop_front();
            writeToFile(oldFile.c_str(), buffer);    
        }
        fileNames.push_back(fileName);
        cache.push_back("");
        positionInCache = fileNames.size() - 1;
    }

    string goesToCache = "";
    
    if (logfd != -1) {
            char logString[50] = "PUT ";
            strcat(logString, fileName.c_str());
            sprintf(logString, "%s length %d\n", logString, contentLength);
            
            writeLog(logString);
        }
    unsigned long int newLineLog = 0;
        
    while(contentLength > 0) {
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
            logPut(newLineLog, buffer);
            newLineLog =  newLineLog + strlen(buffer);
            
            //cout << "Content Length after logPut: " << contentLength << endl;
        }
        
        contentLength = contentLength - strlen(buffer);
        string temp (buffer);
        goesToCache += temp;
    }
    if(newLineLog % 20 != 0) {
        pwrite(logfd, "\n========\n", 10, offset);
        offset = offset + 10;
        
    }
    else {
        pwrite(logfd, "========\n", 9, offset);
        offset = offset + 9;    
    }
    cache[positionInCache] = goesToCache;

    close(socket);
    return 1;
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
                
                writeLog(logString);
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
            
            writeLog(logString);
               
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
        
        if (logfd != -1) {
            char logString[50] = "PUT ";
            strcat(logString, fileName);
            sprintf(logString, "%s length %d\n", logString, contentLength);
            writeLog(logString);
            //cout << logString << endl;
            
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
                logPut(newLineLog, buffer);
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
            pwrite(logfd, "\n========\n", 10, offset);
            offset = offset + 10;
            
        }
        else {
            pwrite(logfd, "========\n", 9, offset);
            offset = offset + 9;    
        }
            
    }
    //cout << "End of function" << endl;
    close(socket);
    close(fd);
    return 0;
}

int getWithCache(int socket, string fileName) {
    if (access(fileName.c_str(), F_OK) == -1)  {
        char notFoundMessage[] = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(socket, notFoundMessage, strlen(notFoundMessage), 0);
        
        if (logfd != -1) {
            char logString[73] = "FAIL: GET ";
            strcat(logString, fileName.c_str());
            strcat(logString, " HTTP/1.1 --- response 404\n========\n");
            
            writeLog(logString);
            
        }
        close(socket);
        return -1;
    }
    else if(access(fileName.c_str(), R_OK) == -1) {
        char forbiddenMessage[] = "HTTP/1.1 403 Forbidden\r\n\r\n";
            send(socket, forbiddenMessage, strlen(forbiddenMessage), 0);
            
            if (logfd != -1) {
                char logString[73] = "FAIL: GET ";
                strcat(logString, fileName.c_str());
                strcat(logString, " HTTP/1.1 --- response 403\n========\n");
                
                writeLog(logString);

            }
            return -1;
    }

    int fd = open(fileName.c_str(), 0);
    //cout << fd << endl;
    
    bool isInCache = false;
    int positionInCache = -1;
    for(unsigned int i = 0; i < fileNames.size(); i ++) {
        if(0 == fileName.compare(fileNames[i])) {
            isInCache = true;
            positionInCache = i;
        }
    }

    if(isInCache) {
        if (logfd != -1) {
            char logString[50] = "GET ";
            strcat(logString, fileName.c_str());
            strcat(logString, " length 0\n========\n");
            //cout << logString << endl;
            writeLog(logString);
                
        }
        unsigned int q = send(socket, cache[positionInCache].c_str(), cache[positionInCache].size(), 0);
        if (q < 0) {
            cout << "couldnt send value " << endl;
            exit(-1);
        }
    }
    else {
        if(fileNames.size() == MAX_CACHE_SIZE) {
            string oldFile = fileNames.front();
            fileNames.pop_front();
            string buffer = cache.front();
            cache.pop_front();
            writeToFile(oldFile.c_str(), buffer);    
        }
        fileNames.push_back(fileName);
        cache.push_back("");
        positionInCache = fileNames.size() - 1;

        //cout << fileName << endl;
        if (logfd != -1) {
            char logString[50] = "GET ";
            strcat(logString, fileName.c_str());
            strcat(logString, " length 0\n========\n");
            //cout << logString << endl;
            writeLog(logString);
                
        }

        int eof = 4095;
        string goesToCache = "";
        while (eof != 0) {
            char buff[4096] = "";
            eof = read(fd, buff, eof);
            //cout << eof << endl;
            unsigned int q = send(socket, buff, strlen(buff), 0);
            if (q < 0) {
                cout << "couldnt send value " << endl;
                exit(-1);
            }

            string temp (buff);
            goesToCache += temp;

            
        }
        cache[positionInCache] = goesToCache;
        
        close(fd);
        close(socket);        
    }

    close(socket);
    return 1;
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
                
                writeLog(logString);

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
            
            writeLog(logString);
            
        }
        close(socket);
        return -1;
    }

    else {

        if (logfd != -1) {
            char logString[50] = "GET ";
            strcat(logString, fileName);
            strcat(logString, " length 0\n========\n");
            
            writeLog(logString);
                
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

void handleClient(int newsockfd) {
    //cout << "thread" << pthread_self() << " handling socket: "<< newsockfd << endl;
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
    string fileName = headerParts[1];
    if(fileName.length() != 27){
        char notFoundMessage[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(newsockfd, notFoundMessage, strlen(notFoundMessage), 0);
        
        if (logfd != -1) {
            char logString[73] = "FAIL: ";
            strcat(logString, requestType.c_str());
            strcat(logString, " ");
            strcat(logString, fileName.c_str());
            strcat(logString, " HTTP/1.1 --- response 400\n========\n");
            
            writeLog(logString);
            
        }
    }
    else {
        // GET function
        if (requestType.compare("GET") == 0) {
            
            if(cacheBit) {
                getWithCache(newsockfd, fileName);
            }
            else {
                get(newsockfd, fileName.c_str());   
            }
        }

        // PUT function
        else if (requestType.compare("PUT") == 0) {
            //cout << "Content Length is: " << contentLength << endl;
            if(cacheBit) {
                putWithCache(newsockfd, fileName, contentLength);
            }
            else {
                put(newsockfd, fileName.c_str(), contentLength);   
            }
        }
        else {
            char internalMessage[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
            send(newsockfd, internalMessage, strlen(internalMessage), 0);
            
            if (logfd != -1) {
                char logString[73] = "FAIL: ";
                strcat(logString, requestType.c_str());
                strcat(logString, " ");
                strcat(logString, fileName.c_str());
                strcat(logString, " HTTP/1.1 --- response 500\n========\n");
                
                writeLog(logString);
            }
        }

    }
    
    close(newsockfd);

}

// main function -
// where the execution of program begins
int main(int argc, char *argv[]) {
    //cout << argc << endl;
    int port = 80;
    logfd = -1;
    cacheBit = false;
    bool givenHostname = false;
    struct hostent *server;
    int opt;
    const char* logFile;
    
    while( (opt = getopt(argc, argv, "cl:")) != -1) {
        switch (opt) {
            case 'c':
            {
                //printf("option: %s\n", (optarg));
                cacheBit = true;
                
            } break;
            case 'l':
            {
                //printf("option: %s\n", optarg);
                logFile = optarg;
                offset = 0;
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
    

    
    while (1) {

        int newsockfd = accept(sockfd, (struct sockaddr *)&client_addr, &len);
        if (newsockfd < 0) {
            cout << "Exit because cant connect to client" << endl;
        }
        
        handleClient(newsockfd);
    }
    return 1;
}