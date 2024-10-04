#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>
#include <vector>
#include <ncursesw/ncurses.h>  // ncurses library

#if defined(_WIN32) || defined(_WIN64)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")  // Link with Winsock library
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>  // For getaddrinfo
#endif

#include <thread>

#define PORT "8000"  // String format for getaddrinfo
#define BUFFER_SIZE 256

#if defined(_WIN32) || defined(_WIN64)
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

WINDOW *message_win, *input_win, *user_win;

std::vector<std::string> users;

// Function to initialize ncurses and create windows
void init_ncurses() {
    initscr();              // Initialize ncurses mode
    cbreak();               // Disable line buffering
    noecho();               // Do not echo user input
    keypad( stdscr, TRUE ); // Enable special keys
    curs_set( 0 );          // Hide the cursor

    int height, width;
    getmaxyx( stdscr, height, width );

    // Create user list window on the left
    user_win = newwin( height - 3, width / 4, 0, 0 ); // 1/4 of the width
    box( user_win, 0, 0 );
    mvwprintw( user_win, 0, 1, " Users " );
    wrefresh( user_win );

    // Create message window on the right
    message_win = newwin( height - 3, ( width * 3 ) / 4, 0, width / 4 ); // 3/4 of the width
    box( message_win, 0, 0 );
    scrollok( message_win, TRUE );
    wrefresh( message_win );

    // Create input window at the bottom
    input_win = newwin( 3, width, height - 3, 0 );
    box( input_win, 0, 0 );
    wrefresh( input_win );
}

// Function to close ncurses and clean up
void close_ncurses() {
    delwin( message_win );
    delwin( input_win );
    delwin( user_win );
    endwin();  // End ncurses mode
}

// Function to display a message in the chat pane
void display_message( const std::string &message ) {
    int y, x;
    getyx( message_win, y, x );

    // Check if the message window is full and scroll if needed
    if( y>=getmaxy( message_win )-2 ) {
        wscrl( message_win, 1 );
    }

    // Print message with padding
    wmove( message_win, y, 1 ); // Add a margin from the left
    wprintw( message_win, "%s\n", message.c_str() );
    wrefresh( message_win ); // Refresh message window to show new content
}

// Function to display the list of users in the user pane
void display_users() {
    wclear( user_win );
    box( user_win, 0, 0 );
    mvwprintw( user_win, 0, 1, " Users " );
    for ( size_t i = 0; i < users.size(); ++i ) {
        mvwprintw( user_win, i + 1, 1, "%s", users[i].c_str() );
    }
    wrefresh( user_win ); // Refresh user window to show updated list
}

// Function to handle input from the user and send to the server
void handle_input( socket_t sock_fd ) {
    char input_buffer[BUFFER_SIZE];
    while ( true ) {
        // Clear input window
        wclear( input_win );
        box( input_win, 0, 0 );
        mvwprintw( input_win, 1, 1, "> " );
        wrefresh( input_win );

        // Temporarily enable echoing in input window
        echo();

        // Get user input
        wgetnstr( input_win, input_buffer, BUFFER_SIZE - 1 );

        // Disable echo after input is complete
        noecho();

        // Send input to server
        send( sock_fd, input_buffer, strlen( input_buffer ), 0 );
    }
}

int main() {
    socket_t sock_fd;
    struct addrinfo hints, *result;
    char buffer[BUFFER_SIZE] = {0};

    #if defined(_WIN32) || defined(_WIN64)
    // Initialize Winsock
    WSADATA wsaData;
    if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
        std::cerr << "WSAStartup failed" << std::endl;
        return EXIT_FAILURE;
    }
    #endif

    // Set up hints for getaddrinfo
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_INET;      // Use IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets

    // Resolve the server address and port
    if ( getaddrinfo( "127.0.0.1", PORT, &hints, &result ) != 0 ) {
        std::cerr << "getaddrinfo failed" << std::endl;
        #if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
        #endif
        return EXIT_FAILURE;
    }

    // Create socket
    sock_fd = socket( result->ai_family, result->ai_socktype, result->ai_protocol );
    if ( sock_fd < 0 ) {
        perror( "socket failed" );
        freeaddrinfo( result );
        #if defined(_WIN32) || defined(_WIN64)
        WSACleanup();
        #endif
        return EXIT_FAILURE;
    }

    // Connect to server
    if ( connect( sock_fd, result->ai_addr, result->ai_addrlen ) < 0 ) {
        perror( "connect failed" );
        freeaddrinfo( result );
        #if defined(_WIN32) || defined(_WIN64)
        closesocket( sock_fd );
        WSACleanup();
        #else
        close( sock_fd );
        #endif
        return EXIT_FAILURE;
    }

    freeaddrinfo( result ); // No longer needed after connection

    // Initialize ncurses UI
    init_ncurses();

    // Retrieve usernames
    char user[6];

    // Start thread to handle incoming messages from the server
    std::thread receive_thread( [&sock_fd, &user, &users]() {
        char buffer[BUFFER_SIZE];
        while ( true ) {
            memset( buffer, 0, BUFFER_SIZE );
            int bytes_received = recv( sock_fd, buffer, BUFFER_SIZE, 0 );

            strncpy( user, buffer+1, 5 ); // First 6 items
            user[5]='\0'; // Null terminator

            // Add user to users if it doesn't exist already
            if( std::find( users.begin(), users.end(), user ) == users.end() )
                users.push_back( user ); // Append to vector
            else {

                    std::cout << "user exists " << users.size() << std::endl;

            }
            display_users(); // Display initial users

            if ( bytes_received > 0 ) {
                display_message( std::string( buffer ) );
            }
        }
    } );

    // Handle input in the main thread
    handle_input( sock_fd );

    receive_thread.join();
    close_ncurses();

    #if defined(_WIN32) || defined(_WIN64)
    closesocket( sock_fd );
    WSACleanup();
    #else
    close( sock_fd );
    #endif

    return 0;
}
