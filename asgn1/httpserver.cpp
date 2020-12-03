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

using namespace std; 
  
int put(int socket, const char * fileName, int contentLength)
{
    // Check if we have a content length defined
    int fd = open(fileName, O_WRONLY | O_TRUNC);

    //cout << fd << endl;

    if(fd == -1)
    {
        fd = creat(fileName, S_IRUSR | S_IWUSR);
        if(fd == -1)
        {
            //cout << fileName << endl;
            char internalMessage[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n"; 
            send(socket, internalMessage, strlen(internalMessage), 0);
            return -1;
        }
        else
        {
            char createMessage[] = "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n";
            send(socket, createMessage, strlen(createMessage), 0);
            //cout << "Created file" << endl;
        }
        //fd = open(headerParts[1].c_str(), 0);
    }
    else
    {
        char okayMessage[] = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n";
        send(socket, okayMessage, strlen(okayMessage), 0);
        //cout << "Sent okay message" << endl;
    }

    //cout << contentLength << endl;
    if(contentLength == -1)
    {
        int closeSocket = 4095;
        //cout << "entering loop for PUT w/o content length" << endl; 
        while(closeSocket > 0)
        {
            char buffer[4095] = "";

            closeSocket = recv(socket, buffer, sizeof(buffer), 0);
            
            //cout << "writing to file" << endl;
            int q = write(fd, buffer, strlen(buffer));
            if(q < 0)
            {
                cout << "couldnt write value " << endl;
                return -1;
            }
            
        }
    }
    else
    {
        while(contentLength > 0)
        {
            char buffer[4095] = "";
            
            int p = recv(socket, buffer, sizeof(buffer), 0);
            if(p < 0)
            {
                cout << "couldn't recieve value" << endl;
                return -1;
            }
            //else cout << "Hi I got here" << endl;
            
            //cout << buffer << endl;
            
            int q = write(fd, buffer, strlen(buffer));
            if(q < 0)
            {
                cout << "couldnt send value " << endl;
                return -1;
            }
            contentLength = contentLength - strlen(buffer);
        }
    }
    //cout << "End of function" << endl;
    close(fd);
    close(socket);
    return 0;
}

int get(int socket, const char * fileName)
{
    //cout << "GET Request" << endl;
    int fd = open(fileName, 0);    
    //cout << fd << endl;
    if(fd == -1)
    {
        char notFoundMessage[] = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(socket, notFoundMessage, strlen(notFoundMessage), 0);
        return -1;
    }
    
    else
    {
        int eof = 4095;
        while(eof != 0)
        {
            char buff[4096] = "";
            eof = read(fd, buff, eof);
            //cout << eof << endl;
            unsigned int q = send(socket, buff, strlen(buff), 0);
            if(q < 0)
            {
                cout << "couldnt send value " << endl;
                exit(-1);
            }
            
        }
        close(fd);
        close(socket);
    }
    return 0;
}

// main function - 
// where the execution of program begins 
int main(int argc, char* argv[]) 
{
    //cout << argc << endl;
    int port = 80;
    bool givenHostname = false;
    struct hostent *server;
    
    if(argc == 3)
    {
        stringstream translateArgs;
        
        translateArgs << argv[2];
        translateArgs >> port;
        //cout << port << endl;
     
        givenHostname = true;
        server = gethostbyname(argv[1]);
        
        if (server == NULL)
        {
            cout << "Exiting because it cant find server" << endl;
            exit(-1);      
        }
    

    }
    else if(argc == 2)
    {
        port = 80;
        
        givenHostname = true;
        server = gethostbyname(argv[1]);
        if (server == NULL)
        {
            cout << "Exiting because it cant find server" << endl;
            exit(-1);      
        }
    
        
    }
    else
    {
        cout << "Need a first argument in the form of a hostname or IP address" << endl;
        exit(-1);
    }

    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    { 
        cout << "Exit because of connection error" << endl;
        exit(-1);
    }
    struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if(givenHostname) bcopy((char *)server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
    else server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
    {
        cout << "Exit because of bind error" << endl;
        exit(-1);
    }
    if(listen(sockfd,3) < 0)
    {
        cout << "listen" << endl;
        exit(-1);
    }
    
    
    struct sockaddr_in client_addr;
    socklen_t len = sizeof(client_addr);
    while(1)
    {


        int newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &len);
        if (newsockfd < 0)
        {
            cout << "Exit because cant connect to client" << endl;
        }


        char header[4096] = "";
        unsigned int p = recv(newsockfd, header, sizeof(header), 0);
        if(p < 0)
        {
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
        char * contentPointer = strstr(header, "Content-Length:");
        
        //cout << "Value is NULL: " << (contentPointer == NULL) << endl;

        if(contentPointer != NULL)
        {
            hasContentLength = true;
            while(word.compare("Content-Length:") != 0)
            {
                parseHeader >> word;
                //cout << word << endl;
                headerParts.push_back(word);
            }

            parseHeader >> contentLength;
            
            //cout << "Content Length is: " << contentLength << endl;        
        }
        else
        {
            parseHeader >> word;
            headerParts.push_back(word);
        }

        string requestType = headerParts[0];
        const char * fileName = headerParts[1].c_str();

        // GET function
        if(requestType.compare("GET") == 0)
        {
            get(newsockfd, fileName);
        }

        // PUT function
        else if(requestType.compare("PUT") == 0)
        {
            put(newsockfd, fileName, contentLength);
        }
        else
        {
            char internalMessage[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n"; 
            send(newsockfd, internalMessage, strlen(internalMessage), 0);
        }
        close(newsockfd);
    }
} 