#include "deck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

CardDeck game_deck;

void init_deck(CardDeck *deck) {
    const char *suits[] = {"acorn", "ball", "green", "heart"};
    const char *values[] = {"7", "8", "9", "10", "jack", "queen", "king", "ace"};
    int index = 0;

    for (int s = 0; s < 4; s++) {
        for (int v = 0; v < 8; v++) {
            snprintf(deck->deck[index++], BUFFER_SIZE, "%s_%s", suits[s], values[v]);
        }
    }

    deck->topCardIndex = 31;

    for (int i = 0; i < 32; i++) {
        int j = rand() % 32;
        char temp[BUFFER_SIZE];
        memcpy(temp, deck->deck[i], BUFFER_SIZE);
        memcpy(deck->deck[i], deck->deck[j], BUFFER_SIZE);
        memcpy(deck->deck[j], temp, BUFFER_SIZE);
    }
}
