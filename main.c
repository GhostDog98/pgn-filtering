#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

static char printf_buffer[160000000]; // size of printf buffer
#define ACCUMULATOR_ITEMS 40  // Maximum number of tags in a game, plus 3 for the newlines and pgn data
#define ACCUMULATOR_ITEM_MAX_LENGTH 19202 // Max lichess game length plus worst case pgn notation https://lichess.org/forum/general-chess-discussion/what-is-the-highest-number-of-moves-played-in-a-game-on-lichess

// Function to extract Elo rating from a line, tell gcc it's static (not called from anything else) and should be inlined for speeeeed
int extract_elo(char *line) {
    char *elo_str = strstr(line, "\""); // strstr returns us a pointer of the first character that has a \" character
    return atoi(elo_str + 1);
}


// Function to check if both WhiteElo and BlackElo are within the target Elo range
bool is_within_elo_range(char accumulator[ACCUMULATOR_ITEMS][ACCUMULATOR_ITEM_MAX_LENGTH], int low_elo, int high_elo) {

    // We can get the chance of whiteelo being on any given line using:
        // ./pgn 1 9999 lichess_db_standard_rated_2023-05.pgn | grep "Found white on line" | sed 's/Found white on line //g' > data.txt
    // Common values are 7 and 9
    /*
    int common_lines[] = {7, 9};
    int common_lines_elements = sizeof(common_lines) / sizeof(common_lines[0]);
    int i = 0;
    while (i < common_lines_elements){
        if(strstr(accumulator[common_lines[i]], "WhiteElo")){
            int white_elo = extract_elo(accumulator[common_lines[i]]);
            // We know that the black elo HAS TO BE NEXT, saves a bunch of time
            int black_elo = extract_elo(accumulator[common_lines[i] + 1]);

            return (white_elo >= low_elo && white_elo <= high_elo && black_elo >= low_elo && black_elo <= high_elo);
        }
        i++;
    }*/



    for (int i = 0; i < ACCUMULATOR_ITEMS; i++){
        if (strstr(accumulator[i], "WhiteElo")){
            int white_elo = extract_elo(accumulator[i]);
            // We know that the black elo HAS TO BE NEXT, saves a bunch of time
            int black_elo = extract_elo(accumulator[i + 1]);

            return (white_elo >= low_elo && white_elo <= high_elo && black_elo >= low_elo && black_elo <= high_elo);
        }
    }



    printf("WARNING: RECORD REACHED END WITHOUT ELO");
    exit(EXIT_FAILURE);
    return false;
}


int main(int argc, char *argv[]) {
    if (__builtin_expect(argc != 4, 0)) { // We expect this to usually pass through...
        printf("Usage: %s <low_elo> <high_elo> <file_to_filter.pgn>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int low_elo = atoi(argv[1]);
    int high_elo = atoi(argv[2]);
    char *file = argv[3];

    // Open file
    int filedescriptor = open(file, O_RDONLY);
    if (filedescriptor < 0) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Get file size
    struct stat stats;
    fstat(filedescriptor, &stats);
    size_t filesize = stats.st_size;

    // Memory-map the file
    char *file_data = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, filedescriptor, 0);
    if (file_data == MAP_FAILED) {
        perror("Error with mmap");
        exit(EXIT_FAILURE);
    }

    // Advise the kernel about the memory access pattern
    if (madvise(file_data, filesize, MADV_SEQUENTIAL | MADV_WILLNEED) != 0) {
        perror("Error with madvise");
        exit(EXIT_FAILURE);
    }

    char accumulator[ACCUMULATOR_ITEMS][ACCUMULATOR_ITEM_MAX_LENGTH];  // Array of 32 items (lines), each having 19202 items (chars)
    int accumulator_index = 0;   // Index to keep track of the accumulator
    unsigned long long records_parsed = 0;

    // Set buffer to make printf faster
    setvbuf(stdout, printf_buffer, _IOFBF, sizeof(printf_buffer));

    bool first_newline = true;
    char *line_start = file_data;
    char *line_end = file_data;

    while (line_end < file_data + filesize) {
        if (*line_end == '\n') {  // Found end of line
            size_t linelength = line_end - line_start + 1;

            if (linelength == 1 && first_newline == false) {  // End of a record
                records_parsed++;

                // Check if the record is within the target Elo range
                if (is_within_elo_range(accumulator, low_elo, high_elo)) {
                    printf("\n");  // Only print newline if a match is found

                    // Print accumulator
                    for (int i = 0; i < accumulator_index; i++) {
                        printf("%s", accumulator[i]);
                    }
                }

                // Clear the accumulator
                accumulator_index = 0;
                first_newline = true;
            } else if (linelength == 1 && first_newline == true) {  // First newline separating tags and moves
                first_newline = false;
            } else {  // Inside a record
                if (accumulator_index < ACCUMULATOR_ITEMS) {  // Add line to accumulator
                    memcpy(accumulator[accumulator_index], line_start, linelength);
                    accumulator[accumulator_index][linelength] = '\0';
                    accumulator_index++;
                } else {
                    printf("Accumulator overflow!\n");
                    break;
                }
            }

            line_start = line_end + 1;  // Move to next line
        }

        line_end++;
    }

    printf("Total records parsed: %llu\n", records_parsed);

    // Cleanup
    munmap(file_data, filesize);
    close(filedescriptor);

    exit(EXIT_SUCCESS);
}