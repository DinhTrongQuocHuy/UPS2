#include "game.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

GameSession sessions[MAX_SESSIONS];

void deal_initial_hands(GameSession *session) {
    // Validate that the draw deck has enough cards
    if (session->drawDeck.topCardIndex < 0) {
        printf("Error: Draw deck is empty. Cannot initialize discard pile.\n");
        return;
    }

    // Set the first card from the draw deck to the discard pile
    const char *discardCard = session->drawDeck.deck[session->drawDeck.topCardIndex--];
    session->discardDeck.topCardIndex = 0;
    strncpy(session->discardDeck.deck[session->discardDeck.topCardIndex], discardCard, BUFFER_SIZE - 1);
    session->discardDeck.deck[session->discardDeck.topCardIndex][BUFFER_SIZE - 1] = '\0';

    // Set initial active suit and value
    char temp[BUFFER_SIZE];
    strncpy(temp, discardCard, BUFFER_SIZE - 1);
    temp[BUFFER_SIZE - 1] = '\0';
    char *suit = strtok(temp, "_");
    char *value = strtok(NULL, "_");

    if (suit && value) {
        strncpy(session->activeSuit, suit, sizeof(session->activeSuit) - 1);
        session->activeSuit[sizeof(session->activeSuit) - 1] = '\0';

        strncpy(session->activeValue, value, sizeof(session->activeValue) - 1);
        session->activeValue[sizeof(session->activeValue) - 1] = '\0';
    } else {
        printf("Error: Invalid card format in draw deck.\n");
        return;
    }

    // Deal 5 cards to each player
    for (int i = 0; i < 2; i++) {
        session->players[i]->handSize = 0;

        for (int j = 0; j < 5; j++) {
            if (session->drawDeck.topCardIndex < 0) {
                printf("Warning: Not enough cards in draw deck to deal to player %s.\n", session->players[i]->username);
                return;  // Stop dealing if the deck is empty
            }

            // Copy the card to player's hand
            strncpy(session->players[i]->hand[j], session->drawDeck.deck[session->drawDeck.topCardIndex], BUFFER_SIZE - 1);
            session->players[i]->hand[j][BUFFER_SIZE - 1] = '\0';  // Ensure null termination
            session->players[i]->handSize++;

            // Clear the card in the draw deck
            memset(session->drawDeck.deck[session->drawDeck.topCardIndex], 0, BUFFER_SIZE);

            // Move to the next card
            session->drawDeck.topCardIndex--;
        }
    }

    printf("Initial hands dealt successfully.\n");
}

void reshuffle_discard_to_draw(GameSession *session) {
    if (session->discardDeck.topCardIndex < 1) {
        printf("Not enough cards to reshuffle.\n");
        return;
    }

    // Save the last card in discard pile
    char last_card[BUFFER_SIZE];
    strncpy(last_card, session->discardDeck.deck[session->discardDeck.topCardIndex], BUFFER_SIZE - 1);
    last_card[BUFFER_SIZE - 1] = '\0';

    // Move the rest of the discard pile to the draw deck
    for (int i = 0; i < session->discardDeck.topCardIndex; i++) {
        strncpy(session->drawDeck.deck[i], session->discardDeck.deck[i], BUFFER_SIZE - 1);
        session->drawDeck.deck[i][BUFFER_SIZE - 1] = '\0';
    }
    session->drawDeck.topCardIndex = session->discardDeck.topCardIndex - 1;

    // Reset discard deck and place the last card back
    session->discardDeck.topCardIndex = 0;
    strncpy(session->discardDeck.deck[0], last_card, BUFFER_SIZE - 1);
    session->discardDeck.deck[0][BUFFER_SIZE - 1] = '\0';

    printf("Discard deck reshuffled into draw deck.\n");
}

GameSession* find_session_by_username(const char* username) {
    if (!username || strlen(username) == 0) {
        printf("Error: Invalid username passed to find_session_by_username.\n");
        return NULL;
    }

    for (int i = 0; i < MAX_SESSIONS; i++) {
        GameSession *session = &sessions[i];

        // Skip cleared sessions
        if (!session->players[0] && !session->players[1]) {
            continue;
        }

        // Check if either player's username matches the given username
        if ((session->players[0] && strcmp(session->players[0]->username, username) == 0) ||
            (session->players[1] && strcmp(session->players[1]->username, username) == 0)) {
            return session;
        }
    }

    printf("Session not found for username: %s\n", username);
    return NULL;
}

void start_game(GameSession *session) {
    printf("Starting game session...\n");

    // Clear and reset session decks
    memset(session->drawDeck.deck, 0, sizeof(session->drawDeck.deck));
    memset(session->discardDeck.deck, 0, sizeof(session->discardDeck.deck));
    session->drawDeck.topCardIndex = -1;
    session->discardDeck.topCardIndex = -1;

    // Reset the active suit and value
    memset(session->activeSuit, 0, sizeof(session->activeSuit));
    memset(session->activeValue, 0, sizeof(session->activeValue));

    // Reinitialize the draw deck
    init_deck(&session->drawDeck);

    // Deal initial hands and set up the first card in the discard pile
    deal_initial_hands(session);

    // Ensure activeSuit and activeValue are set correctly
    if (strlen(session->activeSuit) == 0 || strlen(session->activeValue) == 0) {
        printf("Error: active suit and value not set after dealing hands.\n");
        return;
    }

    // Randomize the starting player
    session->currentTurn = rand() % 2;

    // Broadcast the initial game state to both players
    broadcast_game_state(session, -1, 1);

    printf("Game session started successfully.\n");
}

int find_available_session_index() {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].players[0] == NULL && sessions[i].players[1] == NULL) {
            return i; // Return the index of an available session
        }
    }
    return -1; // No available session found
}

void broadcast_game_state(GameSession *session, int playerIndex, int broadcast) {
    if (!session) {
        printf("Error: Invalid session.\n");
        return;
    }

    char gameState[BUFFER_SIZE] = "KIVUPSgameSt";
    strcat(gameState, "0000");  // Placeholder for possible future message length

    char discardCard[BUFFER_SIZE] = { 0 };
    snprintf(discardCard, sizeof(discardCard), "%s_%s", session->activeSuit, session->activeValue);

    for (int i = 0; i < 2; i++) {
        if (!broadcast && i != playerIndex) continue; // Skip other players if not broadcasting

        Player *player = session->players[i];
        Player *opponent = session->players[(i + 1) % 2];

        if (!player || player->sockfd == -1) {
            if (!broadcast) {
                printf("Error: Invalid socket for player %d.\n", i + 1);
            }
            continue;
        }

        // Prepare the player's hand information
        char handInfo[BUFFER_SIZE] = "";
        for (int j = 0; j < player->handSize; j++) {
            strncat(handInfo, player->hand[j], BUFFER_SIZE - strlen(handInfo) - 2);
            if (j < player->handSize - 1) strncat(handInfo, ",", 1);
        }

        // Opponent's card count
        int opponentCardCount = (opponent && opponent->sockfd != -1) ? opponent->handSize : 0;

        // Build the game state message
        snprintf(gameState + 14, BUFFER_SIZE - 14, "P%d:%s|D:%s|O:%d|T:%d|%s\n",
                 i + 1, handInfo, discardCard, opponentCardCount,
                 (session->currentTurn == i ? 1 : 0),
                 (session->skipPending ? "SKIP_PENDING" :
                  session->force_draw_count > 0 ? "FORCE_DRAW_PENDING" : ""));

        // Send the game state to the player
        if (send(player->sockfd, gameState, strlen(gameState), 0) == -1) {
            perror("Failed to send game state");
        } else {
            printf("Game state sent to player %s.\n", player->username);
        }

        // If sending to a specific player, break after sending
        if (!broadcast) break;
    }
}

void print_session_details(GameSession *session) {
    if (!session) {
        printf("Session is NULL.\n");
        return;
    }

    printf("\n=== SESSION DETAILS ===\n");
    printf("Current Turn: Player %d\n", session->currentTurn + 1);
    printf("Active Suit: %s\n", session->activeSuit);
    printf("Active Value: %s\n", session->activeValue);
    printf("Skip Pending: %d\n", session->skipPending);
    printf("Force Draw Pending: %d, Force Draw Count: %d\n", session->force_draw_pending, session->force_draw_count);

    for (int i = 0; i < 2; i++) {
        Player *player = session->players[i];
        if (player) {
            printf("\nPlayer %d:\n", i + 1);
            printf("  Username: %s\n", player->username[0] ? player->username : "(empty)");
            printf("  State: %d\n", player->state);
            printf("  Socket FD: %d\n", player->sockfd);
            printf("  Hand Size: %d\n", player->handSize);
            printf("  Hand: ");
            for (int j = 0; j < player->handSize; j++) {
                printf("%s%s", player->hand[j], (j < player->handSize - 1) ? ", " : "\n");
            }
            printf("  Missed Heartbeats: %d\n", player->missedHeartbeats);
            printf("  Pending Heartbeat: %d\n", player->pendingHeartbeat);
        } else {
            printf("\nPlayer %d: NULL\n", i + 1);
        }
    }

    printf("=== END OF SESSION DETAILS ===\n");
}

void cleanup_session(GameSession *session) {
    if (!session) return;

    printf("Cleaning up session...\n");

    if ((session->players[0] && session->players[0]->state == STATE_DISCONNECTED) && (session->players[1] && session->players[1]->state == STATE_DISCONNECTED)) {
        for (int i = 0; i < 2; i++) {
            if (session->players[i]) {
                printf("Player %s fully cleared.\n", session->players[i]->username);
                clear_player_data(session->players[i]);
            }
        }
    }
    
    // Always remove player references from the session
    for (int i = 0; i < 2; i++) {
        session->players[i] = NULL;
    }

    // Clear session data
    memset(session->activeSuit, 0, sizeof(session->activeSuit));
    memset(session->activeValue, 0, sizeof(session->activeValue));
    session->currentTurn = -1;
    session->skipPending = 0;
    session->force_draw_pending = 0;
    session->force_draw_count = 0;

    session->drawDeck.topCardIndex = -1;
    session->discardDeck.topCardIndex = -1;
    memset(session->drawDeck.deck, 0, sizeof(session->drawDeck.deck));
    memset(session->discardDeck.deck, 0, sizeof(session->discardDeck.deck));

    printf("Session cleanup complete.\n");
}

void parse_card_info(const char *card, char *suit, char *value) {
    char card_copy[20];
    strncpy(card_copy, card, sizeof(card_copy));
    char *suit_token = strtok(card_copy, "_");
    char *value_token = strtok(NULL, "_");

    if (suit_token && value_token) {
        strncpy(suit, suit_token, 10);
        strncpy(value, value_token, 10);
    } else {
        suit[0] = '\0';
        value[0] = '\0';
    }
}

int validate_move(const char *played_suit, const char *played_value, const char *active_suit, const char *active_value) {
    return (strcmp(played_suit, active_suit) == 0 || strcmp(played_value, active_value) == 0);
}

void send_validation_response(int sockfd, int is_valid, const char *card_name, int game_over) {
    char message[BUFFER_SIZE];

    if (game_over) {
        // Append game over information if the game has ended
        snprintf(message, sizeof(message), "KIVUPSCARD_PLAYED_VALID|%s|LAST_CARD_PLAYED\n", card_name);
    } else if (is_valid) {
        snprintf(message, sizeof(message), "KIVUPSCARD_PLAYED_VALID|%s\n", card_name);
    } else {
        strncpy(message, "KIVUPSCARD_PLAYED_INVALID\n", sizeof(message) - 1);
        message[sizeof(message) - 1] = '\0';
    }

    if (send(sockfd, message, strlen(message), 0) == -1) {
        perror("Failed to send validation response");
    }
}

void switch_turn(GameSession *session) {
    if (!session) return;

    // Switch to the next player
    session->currentTurn = (session->currentTurn + 1) % 2;

    for (int i = 0; i < 2; i++) {
        Player *player = session->players[i];
        if (!player || !is_socket_valid(player->sockfd)) {
            printf("Skipping invalid player during turn switch.\n");
            continue;
        }

        int isMyTurn = (i == session->currentTurn);
        char message[BUFFER_SIZE];
        snprintf(message, sizeof(message), "KIVUPSTURN_SWITCH|%d\n", isMyTurn);

        if (send(player->sockfd, message, strlen(message), 0) == -1) {
            perror("Failed to send turn switch notification");
        } else {
            printf("Notified Player %s: %s turn.\n", player->username, isMyTurn ? "their" : "not their");
        }
    }

    printf("Turn switched. Now it's Player %s's turn.\n", session->players[session->currentTurn]->username);
}

void check_player_activity() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player *player = &players[i];

        // Skip uninitialized or idle players
        if (player->sockfd == -1 || player->state == STATE_IDLE) {
            continue;
        }

        // Handle missed heartbeats
        if (player->pendingHeartbeat) {
            player->missedHeartbeats++;
            printf("Player %s missed heartbeat %d.\n", player->username, player->missedHeartbeats);

            // Mark as disconnected on the first missed heartbeat
            if (player->missedHeartbeats == 1) {
                printf("NO HEARTBEAT RESPONSE\n");

                // Mark the player as disconnected
                player->state = STATE_DISCONNECTED;

                // Properly close the socket and clear it from fd_set
                if (player->sockfd != -1) {
                    close(player->sockfd);  // Close the socket
                    FD_CLR(player->sockfd, &all_fds);  // Remove from fd_set
                    printf("Closed socket fd %d for player %s.\n", player->sockfd, player->username);
                    player->sockfd = -1;  // Mark socket as invalid
                }

                // Notify the opponent
                GameSession *session = find_session_by_username(player->username);
                if (session) {
                    Player *opponent = (session->players[0] == player) ? session->players[1] : session->players[0];
                    if (opponent && opponent->sockfd != -1) {
                        if (send(opponent->sockfd, "KIVUPSOPPONENT_DISCONNECTED\n", 29, MSG_NOSIGNAL) == -1) {
                            perror("Failed to notify opponent about disconnection");
                        } else {
                            printf("Notified opponent %s about player %s's disconnection.\n", opponent->username, player->username);
                        }
                    }
                }
            }

            // Cleanup the player if they exceed max missed heartbeats
            if (player->missedHeartbeats >= MAX_MISSED_HEARTBEATS) {
                if (player->state != STATE_DISCONNECTED) {
                    printf("Player %s exceeded maximum missed heartbeats. Disconnecting.\n", player->username);

                    GameSession *session = find_session_by_username(player->username);
                    if (session) {
                        Player *opponent = (session->players[0] == player) ? session->players[1] : session->players[0];
                        if (opponent && opponent->sockfd != -1) {
                            send(opponent->sockfd, "KIVUPSSESSION_TERMINATED\n", 28, MSG_NOSIGNAL);
                            printf("Notified opponent %s about session termination.\n", opponent->username);
                        }
                        cleanup_session(session);
                    }

                    clear_player_data(player);  // Cleanup happens here only once
                }
            }
        }

        // Send a new heartbeat if none is pending
        if (!player->pendingHeartbeat && player->sockfd != -1) {
            if (send(player->sockfd, "KIVUPSHEARTBEAT\n", 16, MSG_NOSIGNAL) == -1) {
                perror("Failed to send heartbeat");
                player->missedHeartbeats++;
            } else {
                player->pendingHeartbeat = 1; // Await response
                //printf("Sent heartbeat to player %s.\n", player->username);
            }
        }
    }
}

int is_socket_valid(int sockfd) {
    return fcntl(sockfd, F_GETFD) != -1 || errno != EBADF;
}