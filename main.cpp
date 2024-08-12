#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

static char buf[16000000000];



// Function to extract Elo rating from a line
int extract_elo(char *line) {
    char *elo_str = strstr(line, "\"");
    if (elo_str) {
        return atoi(elo_str + 1);
    }
    return 0;
}

// Function to check if both WhiteElo and BlackElo are within the target Elo range
bool is_within_elo_range(char accumulator[32][2048], int low_elo, int high_elo) {
    int white_elo = 0;
    int black_elo = 0;
    for (int i = 0; i < 32; i++) {
        if (strstr(accumulator[i], "WhiteElo")) {
            white_elo = extract_elo(accumulator[i]);
        } else if (strstr(accumulator[i], "BlackElo")) {
            black_elo = extract_elo(accumulator[i]);
        }
    }
    return (white_elo >= low_elo && white_elo <= high_elo && black_elo >= low_elo && black_elo <= high_elo);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <low_elo> <high_elo> <file_to_filter.pgn>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int low_elo = atoi(argv[1]);
    int high_elo = atoi(argv[2]);
    char *file = argv[3];

    char accumulator[32][2048]; // Array of 32 items, each having 2048 items
    int accumulator_index = 0;  // Index to keep track of the accumulator
    unsigned long long records_parsed = 0;
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t linelength;

    // Set buffer to make printf faster
    setvbuf(stdout, buf, _IOFBF, sizeof(buf));

    fp = fopen(file, "r");
    if (fp == NULL) {
        printf("Error opening file...\n");
        exit(EXIT_FAILURE);
    }

    bool first_newline = true;
    while ((linelength = getline(&line, &len, fp)) != -1) { // Read through entire file

        if (linelength == 1 && first_newline == false) { // We got the end of a record now
            records_parsed++;

            // Check if the record is within the target Elo range
            if (is_within_elo_range(accumulator, low_elo, high_elo)) {
                printf("\n"); // Only print newline if a match is found

                // Print accumulator
                for (int i = 0; i < accumulator_index; i++) {
                    printf("%s", accumulator[i]);
                }
            }
            // Clear the accumulator
            accumulator_index = 0;
            memset(accumulator, 0, sizeof(accumulator));

            first_newline = true;
        } else if (linelength == 1 && first_newline == true) {
            first_newline = false;
        } else {

            // Append `line` to accumulator
            if (accumulator_index < 32) {
                strncpy(accumulator[accumulator_index], line, 2048);
                accumulator_index++;
            } else {
                printf("Accumulator overflow!\n");
                break;
            }
        }
    }
    printf("Total records parsed: %llu\n", records_parsed);

    fclose(fp);
    if (line) // If we have hit the end
        free(line);
    exit(EXIT_SUCCESS);
}
