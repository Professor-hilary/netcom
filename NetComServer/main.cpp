#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>  // For memset, strlen

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <process.h>  // For _beginthread
    #pragma comment(lib, "ws2_32.lib")  // Link with Winsock library
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <pthread.h>
#endif

#define PORT 8000
#define BUFFER_SIZE 256
#define MAX_CLIENTS 10

std::vector<int> client_fds;

#if defined(_WIN32) || defined(_WIN64)
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

#if defined(_WIN32) || defined(_WIN64)
void handle_client( void *arg ) {
#else
void *handle_client( void *arg ) {
#endif
    // Client ID gottent from arg
    socket_t client_fd = *( socket_t * )arg;
    srand( client_fd );
    char id_str[6]; // +1 for null bit

    for ( size_t index = 0; index < 5; ++index )
        id_str[index]='a'+( rand()%26 );

    id_str[5]='\0'; // Add null character to the char array

    // Create two buffers for user chats
    char buffer[BUFFER_SIZE] = {0}, tempBuffer[BUFFER_SIZE] = {0};

    while ( true ) {
        // Receive message from client
        int read_size = recv( client_fd, tempBuffer, BUFFER_SIZE, 0 );
        if ( read_size <= 0 ) {
            std::cout << "Client " << id_str << " disconnected" << std::endl;
            break;
        }
        std::cout << "message: " << id_str << " -> " << tempBuffer << std::endl;

        // Broadcast message to all clients
        for ( socket_t fd : client_fds ) {
            if ( fd != client_fd )
                sprintf( buffer, "[%s]: %s", id_str, tempBuffer );
            else
                sprintf( buffer, "[%s]: %s", "LOCAL", tempBuffer );

            send( fd, buffer, strlen( buffer ), 0 );
        }
        memset( buffer, 0, BUFFER_SIZE );
    }

    #if defined(_WIN32) || defined(_WIN64)
    closesocket( client_fd );
    #else
    close( client_fd );
    #endif

    #if defined(_WIN32) || defined(_WIN64)
    _endthread();
    #else
    return NULL;
    #endif
}

int main() {
    socket_t server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof( address );

    #if defined(_WIN32) || defined(_WIN64)
    WSADATA wsaData;
    if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
        std::cerr << "WSAStartup failed" << std::endl;
        return EXIT_FAILURE;
    }
    #endif

    // Create socket
    server_fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( server_fd == -1 ) {
        perror( "socket failed" );
        #if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
        #endif
        return EXIT_FAILURE;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    // Bind socket to port
    if ( bind( server_fd, ( struct sockaddr * )&address, sizeof( address ) ) < 0 ) {
        perror( "bind failed" );
        #if defined(_WIN32) || defined(_WIN64)
        closesocket( server_fd );
        WSACleanup();
        #else
        close( server_fd );
        #endif
        return EXIT_FAILURE;
    }

    // Listen for connections
    if ( listen( server_fd, MAX_CLIENTS ) < 0 ) {
        perror( "listen failed" );
        #if defined(_WIN32) || defined(_WIN64)
        closesocket( server_fd );
        WSACleanup();
        #else
        close( server_fd );
        #endif
        return EXIT_FAILURE;
    }

    std::cout << "Server listening on port " << PORT << std::endl;

    while ( true ) {
        // Accept client connection
        client_fd = accept( server_fd, ( struct sockaddr * )&address, &addrlen );
        if ( client_fd < 0 ) {
            perror( "accept failed" );
            continue;
        }

        client_fds.push_back( client_fd );

        #if defined(_WIN32) || defined(_WIN64)
        // Create thread to handle client in Windows
        _beginthread( handle_client, 0, &client_fd );
        #else
        // Create thread to handle client in Unix-based systems
        pthread_t thread;
        pthread_create( &thread, NULL, handle_client, &client_fd );
        pthread_detach( thread );
        #endif

        // Send network update to all clients
        std::string update = "Broadcast: <Client " + std::to_string( client_fd ) + " joined>";
        for ( socket_t fd : client_fds ) {
            send( fd, update.c_str(), update.length(), 0 );
        }
    }

    #if defined(_WIN32) || defined(_WIN64)
    closesocket( server_fd );
    WSACleanup();
    #else
    close( server_fd );
    #endif

    return 0;
}
