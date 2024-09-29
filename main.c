#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>


static char printf_buffer[160000000]; // size of printf buffer
#define ACCUMULATOR_ITEMS 32
#define ACCUMULATOR_ITEM_MAX_LENGTH 32768

// Function to extract Elo rating from a line
int extract_elo(char *line) {
    char *elo_str = strstr(line, "\"");
    if (elo_str) {
        return atoi(elo_str + 1);
    }
    return 0;
}


// Function to check if both WhiteElo and BlackElo are within the target Elo range
bool is_within_elo_range(char accumulator[ACCUMULATOR_ITEMS][ACCUMULATOR_ITEM_MAX_LENGTH], int low_elo, int high_elo) {
    int white_elo = 0;
    int black_elo = 0;

    int early_exit = 0;

    for (int i = 0; i < ACCUMULATOR_ITEMS; i++) {  // Iterate over the entire accumulator to see if we have WhiteElo or BlackElo.
        if (strstr(accumulator[i], "WhiteElo")) {
            white_elo = extract_elo(accumulator[i]);
            early_exit++;
        } else if (strstr(accumulator[i], "BlackElo")) {
            black_elo = extract_elo(accumulator[i]);
            early_exit++;
        }

        if (early_exit >= 2) {  // If we have parsed 2 elo's, we shouldn't ever have more!
            return (white_elo >= low_elo && white_elo <= high_elo && black_elo >= low_elo && black_elo <= high_elo);
        }
    }
    printf("WARNING: RECORD REACHED END WITHOUT ELO");
    return false;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <low_elo> <high_elo> <file_to_filter.pgn>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int low_elo = atoi(argv[1]);
    int high_elo = atoi(argv[2]);
    char *file = argv[3];

    char accumulator[ACCUMULATOR_ITEMS][ACCUMULATOR_ITEM_MAX_LENGTH];  // Array of 32 items (lines), each having 32k items (chars)
    int accumulator_index = 0;   // Index to keep track of the accumulator
    unsigned long long records_parsed = 0;
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t linelength;

    // Set buffer to make printf faster
    setvbuf(stdout, printf_buffer, _IOFBF, sizeof(printf_buffer));

    fp = fopen(file, "r");
    if (fp == NULL) {
        printf("Error opening file %s...\n", file);
        exit(EXIT_FAILURE);
    }

    bool first_newline = true;
    while ((linelength = getline(&line, &len, fp)) != -1) {  // Read through entire file
        if (linelength == 1 && first_newline == false) {  // We are at the end of a record now
            records_parsed++;

            // Check if the record is within the target Elo range
            if (is_within_elo_range(accumulator, low_elo, high_elo)) {
                printf("\n");  // Only print newline if a match is found, this ensures we have that extra break between pgn data and pgn moves

                // Print accumulator
                for (int i = 0; i < accumulator_index; i++) {
                    printf("%s", accumulator[i]);
                }
            }
            // Clear the accumulator. We can leave the array full of prior data cos we keep track of ends of things, and use null terminated strings!
            accumulator_index = 0;

            first_newline = true;
        } else if (linelength == 1 && first_newline == true) {  // We just passed the first newline seperating the moves and pgn tags
            first_newline = false;
        } else {  // we are in the middle of the record
            // Append new `line` to accumulator
            if (accumulator_index < ACCUMULATOR_ITEMS) { // Sanity check for if we are exceeding the tag limit. This "should" only happen if we somehow miss a newline, and overflow to the next record
                memcpy(accumulator[accumulator_index], line, linelength);
                accumulator[accumulator_index][linelength] = '\0';
                accumulator_index++;
            } else {
                printf("Accumulator overflow!\n");
                break;
            }
        }
    }
    printf("Total records parsed: %llu\n", records_parsed);

    fclose(fp);
    if (line)  // If we have hit the end
        free(line);
    exit(EXIT_SUCCESS);
}
