#include "game.h"
#include "player.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <net/if.h>
#include <ifaddrs.h>

fd_set all_fds;
int max_fd;
int session_count = 0;

int is_local_ip(const char *ip) {
    struct ifaddrs *ifaddr, *ifa;
    int family, is_local = 0;

    // Retrieve the list of network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        exit(EXIT_FAILURE);
    }

    // Loop through the interfaces and check their IP addresses
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) {
            continue;
        }

        family = ifa->ifa_addr->sa_family;

        // Check only IPv4 addresses
        if (family == AF_INET) {
            char local_ip[INET_ADDRSTRLEN];
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;

            // Convert the IP address to a string
            if (inet_ntop(AF_INET, &sa->sin_addr, local_ip, INET_ADDRSTRLEN)) {
                if (strcmp(local_ip, ip) == 0) {
                    is_local = 1;
                    break;
                }
            }
        }
    }

    freeifaddrs(ifaddr);  // Free memory allocated by getifaddrs
    return is_local;
}

void* periodic_check(void* arg) {
    (void)arg;
    while (1) {
        check_player_activity();
        sleep(2); // Perform the check every 5 seconds
    }
}

void* monitor_graceful_timeout(void* arg) {
    (void)arg;
    while (1) {
        check_graceful_timeout();
        sleep(1);
    }
}

int main(int argc, char *argv[]) {
    // IP, PORT, PING CHECK
    char ip[INET_ADDRSTRLEN] = {0};
    int port = 0;
    int enable_check = 1;

    // SOCKETS
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    if (argc > 1 && strcmp(argv[1], "--no-check") == 0) {
        enable_check = 0;
        argv++; // Shift the argument array
        argc--; // Adjust the argument count
    }
    if (argc > 1) {
        strncpy(ip, argv[1], INET_ADDRSTRLEN - 1);
        ip[INET_ADDRSTRLEN - 1] = '\0'; // Ensure null termination

        // Verify if the IP is local
        if (!is_local_ip(ip)) {
            fprintf(stderr, "Error: Provided IP address %s is not a local IP address.\n", ip);
            exit(EXIT_FAILURE);
        }
    }

    if (argc > 2) {
        port = atoi(argv[2]); // Convert port string to integer
        if (port <= 0 || port > 65535) {
            printf("INVALID PORT!");
            exit(EXIT_FAILURE);
        }
    }

    // Launch periodic check thread if enabled
    if (enable_check) {
        pthread_t checker_thread;
        pthread_create(&checker_thread, NULL, periodic_check, NULL);
        pthread_detach(checker_thread);
    }

    // Launch graceful timeout check thread (always active)
    pthread_t graceful_timeout_thread;
    pthread_create(&graceful_timeout_thread, NULL, monitor_graceful_timeout, NULL);
    pthread_detach(graceful_timeout_thread);

    // Server initialization and main loop...
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &address.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&all_fds);
    FD_SET(server_fd, &all_fds);
    max_fd = server_fd;

    srand(time(NULL));

    init_players();

    while (1) {
        fd_set read_fds = all_fds;
        select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &read_fds)) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            printf("New connection, socket fd: %d\n", new_socket);

            FD_SET(new_socket, &all_fds);
            if (new_socket > max_fd) max_fd = new_socket;

            // Assign the new socket to an available player slot
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].sockfd == -1 && players[i].state != STATE_DISCONNECTED) {
                    players[i].sockfd = new_socket;
                    printf("Assigned new player to slot %d (fd: %d)\n", i, new_socket);
                    break;
                }
            }
        }

        // Handle existing player messages
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (players[i].sockfd != -1 && FD_ISSET(players[i].sockfd, &read_fds)) {
                int valread = read(players[i].sockfd, players[i].buffer + players[i].bufferPtr,
                                BUFFER_SIZE - players[i].bufferPtr);

                if (valread <= 0) {  // Client disconnected or I/O error
                    printf("Player %s disconnected.\n", players[i].username);
                    close(players[i].sockfd);
                    FD_CLR(players[i].sockfd, &all_fds);

                    // Handle player in STATE_WAITING
                    if (players[i].state == STATE_WAITING || players[i].state == STATE_IDLE) {
                        clear_player_data(&players[i]);
                    // Handle player in STATE_PLAYING
                    } else if (players[i].state == STATE_PLAYING) {
                        GameSession *session = find_session_by_username(players[i].username);
                        if (session) {
                            Player *opponent = (session->players[0] == &players[i]) ? session->players[1] : session->players[0];

                            if (opponent && opponent->state == STATE_PLAYING) {
                                // Notify the opponent about the disconnection
                                if (send(opponent->sockfd, "KIVUPSOPPONENT_DISCONNECTED\n", 29, 0) == -1) {
                                    perror("Failed to notify opponent about disconnection");
                                } else {
                                    printf("Notified opponent %s about player %s's disconnection.\n", opponent->username, players[i].username);
                                }

                                // Mark the disconnected player and start timeout
                                players[i].state = STATE_DISCONNECTED;
                                opponent->opponentDisconnectTime = time(NULL);
                                printf("Player %s marked as disconnected. Timeout started.\n", players[i].username);

                            } else if (opponent && opponent->state == STATE_DISCONNECTED) {
                                // If both players are disconnected, clean up immediately
                                printf("Both players disconnected. Cleaning up session.\n");
                                cleanup_session(session);
                            }
                        }
                    }
                } else {
                    players[i].bufferPtr += valread;
                    players[i].buffer[players[i].bufferPtr] = '\0'; // Null-terminate buffer
                    handle_player_message(&players[i]);
                }
            }
        }
    }

    return 0;
}