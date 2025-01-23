#ifndef GAME_H
#define GAME_H

#include "deck.h"

#define MAX_SESSIONS 10  // Maximum number of concurrent sessions
#define DECK_SIZE 32
#define MAX_MISSED_HEARTBEATS 20

typedef struct {
    Player *players[2];
    CardDeck drawDeck;              // Deck for cards that can still be drawn
    CardDeck discardDeck;           // Deck for discarded cards
    int currentTurn;                // Track whose turn it is (0 or 1)
    char activeSuit[10];            // Track active suit
    char activeValue[10];           // Track active value
    int skipPending;   // New flag: 1 = Skip is pending, 0 = No skip
    int force_draw_pending;
    int force_draw_count;
} GameSession;


extern GameSession sessions[MAX_SESSIONS]; // Array to hold game sessions
extern int session_count;                  // Declare session_count as extern

void deal_initial_hands(GameSession *session);
void reshuffle_discard_to_draw(GameSession *session);
GameSession* find_session_by_username(const char* username);
void start_game(GameSession *session);
int find_available_session_index();
void broadcast_game_state(GameSession *session, int playerIndex, int broadcast);
void cleanup_session(GameSession *session);
void parse_card_info(const char *card, char *suit, char *value);
int validate_move(const char *played_suit, const char *played_value, const char *active_suit, const char *active_value);
void send_validation_response(int sockfd, int is_valid, const char *card_name, int game_over);
void switch_turn(GameSession *session);
void check_player_activity();
int is_socket_valid(int sockfd);
#endif