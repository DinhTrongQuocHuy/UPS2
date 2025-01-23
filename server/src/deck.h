#ifndef DECK_H
#define DECK_H

#include "player.h"
#define BUFFER_SIZE 512

typedef struct {
    char deck[32][BUFFER_SIZE];
    int topCardIndex;
} CardDeck;

extern CardDeck game_deck;

void init_deck(CardDeck *deck);

#endif
