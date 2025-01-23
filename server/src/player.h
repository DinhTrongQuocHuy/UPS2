#ifndef PLAYER_H
#define PLAYER_H

#include <time.h>

#define MAX_PLAYERS 10
#define BUFFER_SIZE 512
#define MAX_DISCONNECT_TIMEOUT 20

typedef enum {
    STATE_IDLE,
    STATE_WAITING, // In queue
    STATE_PLAYING, // In game
    STATE_DISCONNECTED, // In game, but disconnected
    STATE_GAMEOVER
} PlayerState;

typedef struct {
    int sockfd;
    PlayerState state;
    char hand[32][BUFFER_SIZE];
    int handSize;
    int missedHeartbeats;
    int pendingHeartbeat;
    char buffer[BUFFER_SIZE];
    int bufferPtr;
    char username[BUFFER_SIZE];
    time_t opponentDisconnectTime;
} Player;

extern Player players[MAX_PLAYERS];

void init_players();
void clear_player_data(Player *player);
void check_graceful_timeout();
void disconnect_player(Player *player);
void handle_player_message(Player *player);
int handle_reconnection(Player *player, const char *username);
void handle_enter_queue(Player *player);
void handle_play_card(Player *player);
void handle_suit_change(Player *player);
void handle_draw_card(Player *player, int force_draw);
void handle_skip_opponent(Player *player);
void handle_force_draw(Player *player);
void handle_victory(Player *player);
#endif