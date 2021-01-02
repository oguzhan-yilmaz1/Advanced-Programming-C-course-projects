/*
Author: Oguzhan Yilmaz 
Class: ECE4122 
Last Date Modified: 11/1/2019
Description: Creates a TCP server that can connect with multiple clients and 
send/receive packages while responding to user prompts such as displaying
last message received, closing all sockets and the connection, and displaying 
information about client connections.
*/

/* 
   http://www.linuxhowtos.org/C_C++/socket.htm
   A simple server in the internet domain using TCP
   The port number is passed as an argument
*/

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

    #pragma comment (lib, "Ws2_32.lib")
#else
   /* Assume that any non-Windows platform uses POSIX-style sockets instead. */
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
    #include <unistd.h> /* Needed for close() */

    typedef int SOCKET;
#endif
/////////////////////////////////////////////////
// Cross-platform socket initialize
int sockInit(void)
{
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(1, 1), &wsa_data);
#else
    return 0;
#endif
}
/////////////////////////////////////////////////
// Cross-platform socket quit
int sockQuit(void)
{
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}
/////////////////////////////////////////////////
// Cross-platform socket close
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

// structure defining the message transmitted
typedef struct tcpMessage
{
    unsigned char nVersion;  
    unsigned char nType;
    unsigned short nMsgLen;  
    char chMsg[1000];
} tcpMessage;

// struct to hold info about connected sockets
typedef struct socketInfo 
{
    int storedSockfd;
    int portno;
    const char* ipaddress;
} socketInfo;

// flag to indicate that the server is in listening state
std::atomic<bool> listening{false};

// flag to indicate to all threads that user invoked server to close
// server closes and program terminates when true
std::atomic<bool> serverQuitFlag{false};

// a vector of socketInfo for actively connected sockets
std::vector<socketInfo> activeSockets;

// vector of messages received
std::vector<std::string> messages;

// counter to keep track of number of clients connected
int connectionCounter = 0;

/////////////////////////////////////////////////
// Output error message and exit
void error(const char *msg)
{
    perror(msg);
    // Make sure any open sockets are closed before calling exit
    exit(1);
}

/*
The thread for handling the socket operations, parsing and processing the message 
@param connectionSockfd the file descriptor id unique to the socket used to connect
                        the client
*/
void processSocket(int connectionSockfd) {
    int n, spaceCount;
    std::string nVersion; //version # of message, ignore message if != 1
    
    /* Type number of the message. If nType == 0, send data to all clients except the 
    one it has been received from. If nType == 1, reverse the message and send to the 
    client that sent the message */
    std::string nType;

    // instance of tcpMessage to hold the received data
    tcpMessage *msgStructServer = (tcpMessage*)calloc(1,sizeof(tcpMessage));
    
    // define the buffer to be used for reversing the message
    char reverse[1000];
    int dx = 0; // index used for reversing the message
    
    // message character length
    unsigned short nMsgLen;

    if (serverQuitFlag) {
        sockClose(connectionSockfd); // close the connection if prompted
    }

    // message receiving and processing
    do {
        // initalize the char buffers to 0
        memset(msgStructServer->chMsg,0,1000);
        memset(reverse,0,1000);
        // receive the data from client, blocking call
        n = recv(connectionSockfd, msgStructServer, sizeof(tcpMessage), 0);
        if (n < 0) 
            error("ERROR reading from socket");
        if (n == 0) // signifies graceful closure of the client
        { // delete the client id from the activeSockets list
            connectionCounter--;
            for (auto it = begin(activeSockets); it != end(activeSockets); ++it) 
            {
                if (connectionSockfd == it->storedSockfd)
                {
                    activeSockets.erase(it);
                    break;
                }
            }
            break;
        }

        // ignore if nVersion is not 1
        if (msgStructServer->nVersion != '1')
            continue;

        if (msgStructServer->nType == '0')
        {
            for (auto it = begin(activeSockets); it != end(activeSockets); ++it) 
            {
                if (it->storedSockfd != connectionSockfd) 
                {
                    n = send(it->storedSockfd, msgStructServer, sizeof(tcpMessage), 0);
                    if (n < 0)
                        error("ERROR writing to socket");
                }
            }
            messages.push_back(msgStructServer->chMsg); // add message to the list
        } else if (msgStructServer->nType == '1') 
        {
            // store the message as received, but send the reversed version to the client
            std::strcpy(reverse,msgStructServer->chMsg);
            for (int i=msgStructServer->nMsgLen-1;i>=0;i--)
            {
                msgStructServer->chMsg[dx] = reverse[i];
                dx++;
            }
            dx = 0; // reset the dummy index back to 0 for the next iteration
            messages.push_back(reverse); // add the message to the list, notice that reverse
                                         // is actually the copy of original and the message is
                                         // reversed in the struct being sent back.
            n = send(connectionSockfd, msgStructServer, sizeof(tcpMessage), 0);        
        } else 
        {
            messages.push_back(msgStructServer->chMsg); // just add the message
        }

    } while (n > 0); // do until recv returns something valid to work with
    
}

/*
The thread for accepting connections and creating processSocket threads for each
connection made. Stores relevant socket information for display if prompted by user
@param sockfd the file descriptor for the listening socket initialized in the main()
@param cli_addr a pointer to sockaddr_in struct
@param clilen a pointer to socklen_t
*/
void acceptThread(int sockfd, sockaddr_in *cli_addr, socklen_t * clilen) {
    int newsockfd; // file descriptor for the new connection made
    char str[INET_ADDRSTRLEN]; // needed for recollecting the ip address of client
    socketInfo myClientInfo; // store the info to this
    while (listening)
    {
        // make the connection
        newsockfd = accept(sockfd, (struct sockaddr *) cli_addr, clilen);
        if (newsockfd < 0){
            error("ERROR on accept");
            continue;
        }
        // increment counter
        connectionCounter++;
        // start the corresponding thread for the client connection
        std::thread t2(processSocket, newsockfd);
        t2.detach();
        // store relevant info about connection
        myClientInfo.portno = ntohs(cli_addr->sin_port);
        myClientInfo.storedSockfd = newsockfd;
        myClientInfo.ipaddress = inet_ntop(AF_INET,&(cli_addr->sin_addr),str, INET_ADDRSTRLEN);
        activeSockets.push_back(myClientInfo);
    }
}

/*
Main entry point for the program. Starts the acceptThread.
Main thread responsible for handling user commands
@param portNumber command line argument specifying the server's port number
*/
int main(int argc, char *argv[])
{
    int sockfd, portno; // variables for connection information
    socklen_t clilen;
    std::string command;
    /*    struct sockaddr_in {
        short            sin_family;   // e.g. AF_INET
        unsigned short   sin_port;     // e.g. htons(3490)
        struct in_addr   sin_addr;     // see struct in_addr, below
        char             sin_zero[8];  // zero this if you want to
    };*/

    struct sockaddr_in serv_addr, cli_addr;
    if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
    sockInit();
    // Create the socket
    //int socket(int domain, int type, int protocol);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // Make sure the socket was created
    if (sockfd < 0)
        error("ERROR opening socket\n");

    // Zero out the variable serv_addr
    // void * memset(void * ptr, int value, size_t num);
    memset((char *)&serv_addr, 0, sizeof(serv_addr));

    // Convert the port number string to an int
    // int atoi (const char * str);
    portno = atoi(argv[1]);

    // Initialize the serv_addr
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    // Convert port number from host to network
    serv_addr.sin_port = htons(portno);

    // Bind the socket to the port number
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        error("ERROR on binding");
    }
    printf("Listening for connections...\n");
    clilen = sizeof(cli_addr);

    // start listening for connections
    listen(sockfd, 5);
    listening = true;
    // start the thread for accepting and handling the communication
    std::thread t1(acceptThread, sockfd, &cli_addr, &clilen);
    // do this while server is not prompted to close down
    while(!serverQuitFlag) {
        // get user command
        std::cout << "Please enter command: ";
        std::getline (std::cin,command);
        if (command == "q") // quit the server
        {
            serverQuitFlag = true;
        } else if (command == "0") // display the most recent message
        {
            if (messages.empty())
            {
                printf("empty message box\n");
            } else 
            {
                printf("Last message: %s\n",messages.back().c_str());
            }
        } else if (command == "1") // display the currently connected sockets
        {
            printf("Numer of Clients: %d\n",connectionCounter);
            printf("IP Address      Port\n");
            for (auto it = begin(activeSockets); it != end(activeSockets); ++it) 
            {
                printf("%s      %d\n",it->ipaddress,it->portno);
            }
        }
    }
    // set flag to stop listening
    listening = false;
    // close the listening socket
    sockClose(sockfd);
    sockQuit();

#ifdef _WIN32
    std::cin.get();
#endif
    return 0;

}