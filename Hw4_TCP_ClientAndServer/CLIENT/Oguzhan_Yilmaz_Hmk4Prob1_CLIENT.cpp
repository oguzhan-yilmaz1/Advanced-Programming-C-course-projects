/*
Author: Oguzhan Yilmaz 
Class: ECE4122 
Last Date Modified: 11/1/2019
Description: Creates a TCP client that can connect with a server and 
send/receive packages while interacting with user, and constructing the messages.
*/

/*
   http://www.linuxhowtos.org/C_C++/socket.htm
   A simple server in the internet domain using TCP
   The port number is passed as an argument */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

#ifdef _WIN32
   /* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501  /* Windows XP. */
#endif
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")
#else
   /* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
#include <unistd.h> /* Needed for close() */

typedef int SOCKET;
#endif

// structure defining the message transmitted
typedef struct tcpMessage
{
    unsigned char nVersion;  
    unsigned char nType;
    unsigned short nMsgLen;  
    char chMsg[1000];
} tcpMessage;

int sockInit(void)
{
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(1, 1), &wsa_data);
#else
    return 0;
#endif
}

int sockQuit(void)
{
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}

/* Note: For POSIX, typedef SOCKET as an int. */

int sockClose(SOCKET sock)
{

    int status = 0;

#ifdef _WIN32
    status = shutdown(sock, SD_BOTH);
    if (status == 0)
    {
        status = closesocket(sock);
    }
#else
    status = shutdown(sock, SHUT_RDWR);
    if (status == 0)
    {
        status = close(sock);
    }
#endif

    return status;

}

// atomic flag to indicate the closure of client socket
std::atomic<bool> clientquitFlag{false};

void error(const char *msg)
{
    perror(msg);

    exit(0);
}

/*
The thread responsible for constructing and sending the message to the server
@param sockfd the file descriptor for the socket that this client is sitting on
*/
void sendThread(int sockfd) {
    int n; // return value from the send() function call
    // command from user input
    std::string command;
    // instance of tcpMessage to hold the data to send
    tcpMessage *msgStruct = (tcpMessage*)calloc(1,sizeof(tcpMessage));

    // do this while client is not closed (prompted to or server closed)
    while (!clientquitFlag){
        // initialize the message to all zeros
        memset(msgStruct->chMsg,0,1000);
        // get user input
        std::cout << "Please enter command: ";
        std::getline (std::cin,command);
        if (command[0] == 'v') // get version #
        {
            msgStruct->nVersion = command[2];
        } else if (command[0] == 't') // get type # and prepare to send
        {
            msgStruct->nType = command[2];
            // get the message which starts from the index 4 of the command
            memcpy(msgStruct->chMsg, command.c_str() + 4, command.size() - 4);
            msgStruct->nMsgLen = command.length() - 4; // actual message length
            msgStruct->chMsg[msgStruct->nMsgLen] = 0; // end with endline 
            n = send(sockfd, msgStruct, sizeof(tcpMessage), 0); // send to server
            if (n < 0)
                error("ERROR writing to socket");
        } else if (command == "q") // quit the client, time to close the socket
            clientquitFlag = true;
    }
}

/*
The thread responsible for the messages received from the server and displaying them
@param sockfd the file descriptor for the socket that this client is sitting on
*/
void recThread(int sockfd) {
    // instance of tcpMessage to hold the data to receive
    tcpMessage *msgStructCli = (tcpMessage*)calloc(1,sizeof(tcpMessage));
    memset(msgStructCli->chMsg,0,1000);
    int n;
    while (!clientquitFlag){
        n = recv(sockfd, msgStructCli, sizeof(tcpMessage), 0);
        if (n < 0)
            error("ERROR reading from socket");
        if (n == 0) {
            clientquitFlag = true;
        } else {
            std::cout<<"\n"<<"Received Msg Type: "<<msgStructCli->nType<<";"<<" Msg: "<<msgStructCli->chMsg<<std::endl; 
        }
    }
}

/*
Main entry for the program. Starts the sendThread and recThread .
Main thread setting up the connection. After setup, all processing happens inside
the other threads.
@param ipAddress command line argument, IP address of the server to connect to
@param portNumber command line argument specifying the server's port number
*/
int main(int argc, char *argv[])
{
    int sockfd, portno;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    
    int n;
    
    if (argc < 3) {
        fprintf(stderr, "usage %s hostname port\n", argv[0]);
        exit(0);
    }
    
    sockInit();
    // Convert string to int
    portno = atoi(argv[2]);
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    server = gethostbyname(argv[1]);

    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    // Zero out serv_addr variable
    // memset((char *)&serv_addr, sizeof(serv_addr), 0);
    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;

    memmove((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr, server->h_length);

    serv_addr.sin_port = htons(portno);

    // connect with the server
    if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    

    // start the threads
    std::thread tsend(sendThread, sockfd);
    tsend.detach();
    std::thread trecv(recThread, sockfd);
    trecv.detach();

    while (!clientquitFlag) {
        
    }
    clientquitFlag = true; //necessary for graceful exit
    sockClose(sockfd);
    sockQuit();

#ifdef _WIN32
    std::cin.get();
#endif
    return 0;
}