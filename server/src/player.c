#include "player.h"
#include "game.h"
#include "network.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>

Player players[MAX_PLAYERS];

void init_players() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        clear_player_data(&players[i]);
    }
}

void clear_player_data(Player *player) {
    if (!player) return;

    // Close the socket if it's still open
    if (player->sockfd != -1) {
        close(player->sockfd);
        FD_CLR(player->sockfd, &all_fds);
    }

    // Reset all fields
    player->sockfd = -1;
    player->state = STATE_IDLE;
    player->handSize = 0;
    memset(player->hand, 0, sizeof(player->hand));
    player->missedHeartbeats = 0;
    player->opponentDisconnectTime = 0;
    memset(player->buffer, 0, BUFFER_SIZE);
    player->bufferPtr = 0;
    player->pendingHeartbeat = 0;
    memset(player->username, 0, BUFFER_SIZE);
}

void check_graceful_timeout() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *player = &players[i];

        if (player->sockfd == -1 || player->state == STATE_IDLE || player->state == STATE_DISCONNECTED) {
            continue;
        }

        // Check if the opponent was gracefully disconnected
        if (player->opponentDisconnectTime > 0) {
            time_t currentTime = time(NULL);
            double elapsed = difftime(currentTime, player->opponentDisconnectTime);

            if (elapsed >= MAX_DISCONNECT_TIMEOUT) {
                printf("Player %s has been waiting too long (%f seconds) for an opponent. Disconnecting.\n",
                       player->username, elapsed);
                GameSession *session = find_session_by_username(player->username);
                Player *opponent = (session->players[0] == player) ? session->players[1] : session->players[0];
                clear_player_data(opponent);
                cleanup_session(session);

                int temp_sockfd = player->sockfd;
                char temp_username[BUFFER_SIZE];
                strncpy(temp_username, player->username, BUFFER_SIZE);

                clear_player_data(player);

                // Restore critical data
                player->sockfd = temp_sockfd;
                strncpy(player->username, temp_username, BUFFER_SIZE);

                if (send(player->sockfd, "KIVUPSSESSION_TERMINATED\n", strlen("KIVUPSSESSION_TERMINATED\n"), MSG_NOSIGNAL) == -1) {
                    perror("Failed to notify opponent about disconnection");
                } else {
                    printf("Notified opponent %s about player %s's disconnection.\n", 
                           opponent->username, player->username);
                }
            }
        }
    }
}

void disconnect_player(Player *player) {
    if (!player || player->sockfd == -1) return;

    printf("Disconnecting player %s.\n", player->username);

    GameSession *session = find_session_by_username(player->username);
    if (session) {
        Player *opponent = (session->players[0] == player) ? session->players[1] : session->players[0];
        if (opponent && opponent->sockfd != -1) {
            send(opponent->sockfd, "KIVUPSOPPONENT_DISCONNECTED\n", 29, 0);
        }
    }

    close(player->sockfd);
    FD_CLR(player->sockfd, &all_fds);
    player->sockfd = -1;
    player->state = STATE_DISCONNECTED;

    printf("Player %s marked as disconnected.\n", player->username);
}

void handle_player_message(Player *player) {

    printf("MESSAGE RECEIVED\n");
    if (!player || player->sockfd == -1) return;
    char *buffer = player->buffer;
    int buffer_size = player->bufferPtr;

    char *message_start = buffer;
    char *newline;
    while ((newline = memchr(message_start, '\n', buffer_size - (message_start - buffer))) != NULL) {
        *newline = '\0';  // Null-terminate the message

        // Validate the message prefix
        if (strncmp(message_start, "KIVUPS", 6) != 0) {
            printf("Invalid packet from player %s. Disconnecting.\n", player->username);
            disconnect_player(player);
            return;
        }

        // Extract and process the opcode
        char opcode[8];
        strncpy(opcode, message_start + 6, 6);
        opcode[6] = '\0';
        printf("%s\n", opcode);
        if (strcmp(opcode, "enterQ") == 0) {
            printf("ENTER QUEUE\n");
            if (player->state == STATE_IDLE || player->state == STATE_DISCONNECTED) {
                handle_enter_queue(player);
            } else {
                disconnect_player(player);
            }
        } else if (strcmp(opcode, "reConn") == 0) {
            handle_reconnection(player);
        } else if (strcmp(opcode, "heartB") == 0) {
            player->pendingHeartbeat = 0;
        } else if (player->state == STATE_PLAYING) {
            GameSession *session = find_session_by_username(player->username);
            if (!session || session->players[session->currentTurn] != player) {
                printf("Player %s attempted an action out of turn.\n", player->username);
                disconnect_player(player);
                return;
            }
            if (strcmp(opcode, "playCa") == 0) {
                handle_play_card(player);
            } else if (strcmp(opcode, "suitCh") == 0) {
                handle_suit_change(player);
            } else if (strcmp(opcode, "drawCa") == 0) {
                handle_draw_card(player, 0);
            } else if (strcmp(opcode, "skipMv") == 0) {
                handle_skip_opponent(player);
            } else if (strcmp(opcode, "forceD") == 0) {
                handle_force_draw(player);
            } else {
                disconnect_player(player);
            }
        } else {
            printf("Unhandled opcode: %s. Disconnecting player %s.\n", opcode, player->username);
            disconnect_player(player);
        }

        message_start = newline + 1; // Move to the next message
    }

    // Shift remaining incomplete messages to the buffer's start
    int remaining = buffer_size - (message_start - buffer);
    if (remaining > 0) {
        memmove(buffer, message_start, remaining);
    }

    player->bufferPtr = remaining;  // Update the buffer pointer
}

int handle_reconnection(Player *player) {
    const char *username = player->username;

    for (int i = 0; i < session_count; i++) {
        GameSession *session = &sessions[i];

        for (int j = 0; j < 2; j++) {
            Player *disconnectedPlayer = session->players[j];

            if (disconnectedPlayer && disconnectedPlayer->state == STATE_DISCONNECTED &&
                strcmp(disconnectedPlayer->username, username) == 0) {

                printf("Reconnecting player %s to their session.\n", username);

                int temp_sockfd = player->sockfd;
                disconnectedPlayer->sockfd = temp_sockfd;
                // Update the existing player instance
                disconnectedPlayer->sockfd = player->sockfd;
                disconnectedPlayer->state = STATE_PLAYING;
                disconnectedPlayer->missedHeartbeats = 0;
                disconnectedPlayer->opponentDisconnectTime = 0;

                player->sockfd = -1;
                clear_player_data(player);

                // Notify the opponent about the reconnection
                Player *opponent = session->players[1 - j];
                if (opponent && opponent->sockfd != -1) {
                    opponent->opponentDisconnectTime = 0;
                }

                // Send the game state to the reconnecting player
                broadcast_game_state(session, -1, 1);

                printf("Reconnection successful for player %s.\n", username);
                return 1;  // Reconnection successful
                }
        }
    }

    printf("No session found for player %s.\n", username);
    return 0;  // No session found
}

void handle_enter_queue(Player *player) {
    const char *ptr = player->buffer + 12;   // Skip "KIVUPSenterQ"
    int username_len = atoi(ptr);
    ptr += 4;

    char username[BUFFER_SIZE] = {0};
    strncpy(username, ptr, username_len);
    username[username_len] = '\0';

    if (player->username[0] == '\0') {
        strncpy(player->username, username, BUFFER_SIZE - 1);
    }

    printf("Player %s attempting to enter the queue...\n", player->username);

    // Attempt reconnection first
    if (handle_reconnection(player)) {
        return;  // Reconnection successful
    }

    // If no reconnection, add the player to the queue
    player->state = STATE_WAITING;
    printf("Player %s added to the queue.\n", player->username);

    Player *opponent = NULL;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (&players[i] != player && players[i].state == STATE_WAITING) {
            opponent = &players[i];
            break;
        }
    }

    if (opponent) {
        int session_index = find_available_session_index();
        if (session_index == -1) {
            printf("No available session slots.\n");
            return;
        }

        GameSession *session = &sessions[session_index];
        session->players[0] = player;
        session->players[1] = opponent;
        player->state = STATE_PLAYING;
        opponent->state = STATE_PLAYING;
        session_count++;
        start_game(session);
    } else {
        printf("No opponent found for player %s. Waiting for another player.\n", player->username);
    }
}

void handle_play_card(Player *player) {
    GameSession *session = find_session_by_username(player->username);
    if (!session) {
        printf("Player is not part of an active session.\n");
        disconnect_player(player);
        return;
    }

    char *ptr = player->buffer + 12;

    int username_len = atoi(ptr);
    ptr += 4;

    char username[username_len + 1];
    strncpy(username, ptr, username_len);
    username[username_len] = '\0';
    ptr += username_len;

    int played_card_len = atoi(ptr);
    ptr += 4;

    char played_card[played_card_len + 1];
    strncpy(played_card, ptr, played_card_len);
    played_card[played_card_len] = '\0';

    char played_suit[10], played_value[10];
    parse_card_info(played_card, played_suit, played_value);

    // Check if a skip is pending and only allow an Ace to be played
    if (session->skipPending && strcmp(played_value, "ace") != 0) {
        printf("Invalid move: Only an Ace can be played when skip is pending.\n");
        send_validation_response(player->sockfd, 0, NULL, 0);
        return;
    }

    // Check if force draw is pending and only allow a 7 to be played
    if (session->force_draw_pending && strcmp(played_value, "7") != 0) {
        printf("Invalid move: Only a 7 can be played when force draw is pending.\n");
        send_validation_response(player->sockfd, 0, NULL, 0);
        return;
    }

    // Validate move using server's active suit and value
    if (validate_move(played_suit, played_value, session->activeSuit, session->activeValue)) {
        // Update active suit and value
        strncpy(session->activeSuit, played_suit, sizeof(session->activeSuit) - 1);
        strncpy(session->activeValue, played_value, sizeof(session->activeValue) - 1);
        session->activeSuit[sizeof(session->activeSuit) - 1] = '\0';
        session->activeValue[sizeof(session->activeValue) - 1] = '\0';

        // Add card to discard pile
        session->discardDeck.topCardIndex++;
        strncpy(session->discardDeck.deck[session->discardDeck.topCardIndex], played_card, BUFFER_SIZE - 1);
        session->discardDeck.deck[session->discardDeck.topCardIndex][BUFFER_SIZE - 1] = '\0';

        // Remove the card from the player's hand
        for (int i = 0; i < player->handSize; i++) {
            if (strcmp(player->hand[i], played_card) == 0) {
                // Shift remaining cards
                for (int j = i; j < player->handSize - 1; j++) {
                    strncpy(player->hand[j], player->hand[j + 1], BUFFER_SIZE - 1);
                    player->hand[j][BUFFER_SIZE - 1] = '\0';
                }
                player->handSize--;  // Decrement hand size
                break;
            }
        }

        // Check for game over
        int game_over = (player->handSize == 0);
        send_validation_response(player->sockfd, 1, played_card, game_over);
        // Notify the opponent of the last played card
        Player *opponent = (session->players[0] == player) ? session->players[1] : session->players[0];
        if (opponent) {
            char opponent_message[BUFFER_SIZE];
            snprintf(opponent_message, sizeof(opponent_message), "KIVUPSCARD_PLAYED_UPDATE|%s\n", played_card);
            send(opponent->sockfd, opponent_message, strlen(opponent_message), 0);
        }

        // Handle game over condition
        if (game_over) {
            handle_victory(player);
            return;
        }

        // Special effect handling
        if (strcmp(played_value, "7") == 0) {
            session->force_draw_pending = 1;   // Force draw is active
            session->force_draw_count += 2;   // Add 2 cards to the draw count
            printf("Force draw count incremented to %d.\n", session->force_draw_count);

            if (opponent->sockfd != -1) {
                char opponent_message[BUFFER_SIZE];
                snprintf(opponent_message, sizeof(opponent_message), "KIVUPSFORCEDRAW_PENDING\n");
                send(opponent->sockfd, opponent_message, strlen(opponent_message), 0);
            }
        } else if (strcmp(played_value, "ace") == 0) {
            session->skipPending = 1;  // Set skip pending
            if (opponent->sockfd != -1) {
                char opponent_message[BUFFER_SIZE];
                snprintf(opponent_message, sizeof(opponent_message), "KIVUPSSKIP_PENDING\n");
                send(opponent->sockfd, opponent_message, strlen(opponent_message), 0);
            }
        } else if (strcmp(played_value, "queen") == 0) {
            printf("Queen played. Waiting for suit change.\n");
            return;
        }

        // Switch turn after normal play or if no special effect interrupts
        switch_turn(session);
    } else {
        send_validation_response(player->sockfd, 0, NULL, 0);
    }
}

void handle_suit_change(Player *player) {
    GameSession *session = find_session_by_username(player->username);
    if (!session) {
        printf("Player is not part of an active session.\n");
        disconnect_player(player);
        return;
    }

    char *ptr = player->buffer + 12;

    int username_len = atoi(ptr);
    ptr += 4;

    char username[username_len + 1];
    strncpy(username, ptr, username_len);
    username[username_len] = '\0';
    ptr += username_len;

    int new_suit_len = atoi(ptr);
    ptr += 4;

    char new_suit[new_suit_len + 1];
    strncpy(new_suit, ptr, new_suit_len);
    new_suit[new_suit_len] = '\0';

    // Update the active suit in the session
    strncpy(session->activeSuit, new_suit, sizeof(session->activeSuit) - 1);
    session->activeSuit[sizeof(session->activeSuit) - 1] = '\0';

    // Notify both players about the new active suit
    for (int i = 0; i < 2; i++) {
        Player *p = session->players[i];
        if (p && p->sockfd != -1) { // Ensure player is valid and connected
            char message[BUFFER_SIZE];
            snprintf(message, sizeof(message), "KIVUPSSUIT_UPDATE|%s\n", session->activeSuit);
            if (send(p->sockfd, message, strlen(message), 0) == -1) {
                perror("Failed to send suit update notification");
            } else {
                printf("Notified player %s about suit change to %s.\n", p->username, session->activeSuit);
            }
        }
    }
    switch_turn(session);
    printf("Active suit updated to %s by player %s.\n", session->activeSuit, player->username);
}

void handle_draw_card(Player *player, int force_draw) {
    GameSession *session = find_session_by_username(player->username);
    if (!session) {
        printf("Player is not part of an active session.\n");
        disconnect_player(player);
        return;
    }

    // Reshuffle logic if the draw deck is empty
    if (session->drawDeck.topCardIndex < 0) {
        reshuffle_discard_to_draw(session);
    }

    // Draw the top card
    char *drawn_card = session->drawDeck.deck[session->drawDeck.topCardIndex--];
    strncpy(player->hand[player->handSize++], drawn_card, BUFFER_SIZE - 1);
    player->hand[player->handSize - 1][BUFFER_SIZE - 1] = '\0';

    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "KIVUPSDRAW_SUCCESS|%s\n", drawn_card);
    send(player->sockfd, response, strlen(response), 0);

    // Decrement force draw count
    if (session->force_draw_pending) {
        session->force_draw_count--;
        if (session->force_draw_count <= 0) {
            printf("Force draw completed. Resetting force draw flags.\n");
            session->force_draw_pending = 0;
            session->force_draw_count = 0;
        }
    }

    // Notify opponent
    Player *opponent = (session->players[0] == player) ? session->players[1] : session->players[0];
    if (opponent && opponent->sockfd != -1) {
        send(opponent->sockfd, "KIVUPSCARD_DRAWN_UPDATE\n", 24, 0);
    }

    // Switch turn if force draw is complete
    if (!force_draw) {
        switch_turn(session);
    }
}

void handle_skip_opponent(Player *player) {
    GameSession *session = find_session_by_username(player->username);
    if (!session) {
        printf("Player is not part of an active session.\n");
        disconnect_player(player);
        return;
    }

    if (session->skipPending) {
        // Clear the skipPending flag
        session->skipPending = 0;
        switch_turn(session);
    }
}

void handle_force_draw(Player *player) {
    GameSession *session = find_session_by_username(player->username);

    if (!session) {
        printf("Player is not part of an active session.\n");
        disconnect_player(player);
        return;
    }

    if (session->force_draw_pending && session->force_draw_count > 0) {
        int cards_to_draw = session->force_draw_count; // Store the initial draw count
        for (int i = 0; i < cards_to_draw; i++) {
            handle_draw_card(player, 1);
        }
    }
    switch_turn(session);
}

void handle_victory(Player *player) {
    GameSession *session = find_session_by_username(player->username);
    if (!session) {
        printf("Error: No session found for player %s.\n", player->username);
        return;
    }

    // Check victory condition
    if (player->handSize != 0) {
        printf("Player %s has not won yet (hand size: %d).\n", player->username, player->handSize);
        return;
    }

    printf("Player %s has won the game!\n", player->username);

    // Identify the opponent
    Player *opponent = (session->players[0] == player) ? session->players[1] : session->players[0];

    // Send victory message to the winner
    const char *victory_message = "KIVUPSGAME_OVER|VICTORY\n";
    if (send(player->sockfd, victory_message, strlen(victory_message), 0) == -1) {
        perror("Failed to send victory message");
    } else {
        printf("Victory message sent to player %s.\n", player->username);
    }

    // Send defeat message to the opponent
    if (opponent && opponent->sockfd != -1) {
        const char *defeat_message = "KIVUPSGAME_OVER|DEFEAT\n";
        if (send(opponent->sockfd, defeat_message, strlen(defeat_message), 0) == -1) {
            perror("Failed to send defeat message");
        } else {
            printf("Defeat message sent to player %s.\n", opponent->username);
        }
    }

    // Save critical data, clear, and restore
    for (int i = 0; i < 2; i++) {
        Player *currentPlayer = session->players[i];
        if (currentPlayer) {
            int temp_sockfd = currentPlayer->sockfd;
            char temp_username[BUFFER_SIZE];
            strncpy(temp_username, currentPlayer->username, BUFFER_SIZE);

            clear_player_data(currentPlayer);

            // Restore critical data
            currentPlayer->sockfd = temp_sockfd;
            strncpy(currentPlayer->username, temp_username, BUFFER_SIZE);
        }
    }

    // Use cleanup_session for session cleanup
    cleanup_session(session);
    printf("Session cleaned up after victory. Players notified to re-enter queue.\n");
}
