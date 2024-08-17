#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

#ifndef WINDOWSIZE
#error "WINDOWSIZE" is not defined!!!!
#endif

// Function to extract Elo rating from a line
int extract_elo(char *line) {
    char *elo_str = strstr(line, "\"");
    if (elo_str) {
        return atoi(elo_str + 1);
    }
    return 0;
}

// Zerocopy optimizations
#define l(x) __builtin_expect(!!(x), 1)
#define u(x) __builtin_expect(!!(x), 0)
#define PAGESIZE (0x1000)

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <low_elo> <high_elo> <file_to_filter.pgn>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int low_elo = atoi(argv[1]);
    int high_elo = atoi(argv[2]);
    char *file = argv[3];
    struct stat stats; 

    int fd = open(file, O_RDWR);
    fstat(fd, &stats);
    size_t filesize = stats.st_size;
    
    char *begin, *end, *headerstart, *datastart, *index; 
    size_t indexoffset = 0;
    struct iovec iov[40];
    size_t iovecoffset = 0;
    bool print_data = false;
    bool isheader = true;
    size_t fileoffset = 0;

    if (filesize < WINDOWSIZE)  // If we can fit the entire file in our "window", we don't need to juggle data around
        goto done;

    for (fileoffset = 0; fileoffset < filesize - WINDOWSIZE; fileoffset += WINDOWSIZE) {
        if (fileoffset != 0)
            munmap(begin, WINDOWSIZE);
        void *addr = mmap(NULL, WINDOWSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, fileoffset);
        madvise(addr, WINDOWSIZE, MADV_WILLNEED | MADV_SEQUENTIAL);
        begin = addr;
        end = addr + WINDOWSIZE;
        index = begin + indexoffset;

        if (fileoffset == 0)
            headerstart = begin;

        while (index < end) {
            if (isheader) {
                headerstart = index;
                datastart = memmem(index, end - index, "\n\n", 2);
                if (datastart == NULL) {
                    indexoffset = (size_t)index - ((size_t)index & (~(PAGESIZE - 1)));
                    fileoffset += ((size_t)index & (~(PAGESIZE - 1))) - (size_t)begin;
                    writev(1, iov, iovecoffset);
                    iovecoffset = 0;
                    if (fileoffset < filesize - WINDOWSIZE)
                        goto remap;
                    else
                        goto done;
                }
                char *elo1 = memmem(index, datastart - index, "Elo", 3);
                index = elo1 + 3; 
                char *elo2 = memmem(index, datastart - index, "Elo", 3);
                index = elo1 + 3; 

                if (elo1 != NULL && elo2 != NULL) {
                    int elo1val = extract_elo(elo1);
                    int elo2val = extract_elo(elo2);
                    if (elo1val >= low_elo && elo1val <= high_elo && elo2val >= low_elo && elo2val <= high_elo) {
                        iov[iovecoffset].iov_base = headerstart; 
                        iov[iovecoffset++].iov_len = (char *)datastart + 2 - headerstart;
                        if (iovecoffset == 40) {
                            writev(1, iov, 40);
                            iovecoffset = 0;
                        }
                        print_data = true;
                    }
                }
                datastart += 2; 
                index = datastart;
                isheader = false;

            } else {
                datastart = index;
                headerstart = memmem(index, end - index, "\n\n", 2);
                if (headerstart == NULL) {
                    indexoffset = (size_t)index - ((size_t)index & (~(PAGESIZE - 1)));
                    fileoffset += ((size_t)index & (~(PAGESIZE - 1))) - (size_t)begin;
                    writev(1, iov, iovecoffset);
                    iovecoffset = 0;
                    if (fileoffset < filesize - WINDOWSIZE)
                        goto remap;
                    else
                        goto done;
                }

                while (headerstart < end && *headerstart == '\n')
                    headerstart++;

                if (print_data) {
                    iov[iovecoffset].iov_base = datastart; 
                    iov[iovecoffset++].iov_len = (char *)headerstart + 2 - index;
                    if (iovecoffset == 40) {
                        writev(1, iov, 40);
                        iovecoffset = 0;
                    }
                    print_data = false;
                }
                headerstart += 2;
                index = headerstart;
                isheader = true;
            }
        }
        writev(1, iov, iovecoffset);
        iovecoffset = 0;
        indexoffset = 0;
    }

done:
    if (filesize - fileoffset != 0) {
        if (fileoffset != 0)
            munmap(begin, WINDOWSIZE);
        void *addr = mmap(NULL, (((filesize - fileoffset) >> 12) + 1) << 12, PROT_READ | PROT_WRITE, MAP_SHARED, fd, fileoffset);
        madvise(addr, (((filesize - fileoffset) >> 12) + 1) << 12, MADV_WILLNEED | MADV_SEQUENTIAL);
        begin = addr;
        end = addr + filesize - fileoffset;
        index = begin + indexoffset;

        if (fileoffset == 0)
            headerstart = begin;

        while (index < end) {
            if (isheader) {
                datastart = memmem(index, end - index, "\n\n", 2);
                if (datastart == NULL) {
                    indexoffset = (size_t)index - ((size_t)index & (~(PAGESIZE - 1)));
                    fileoffset += ((size_t)index & (~(PAGESIZE - 1))) - (size_t)begin;
                    writev(1, iov, iovecoffset);
                    iovecoffset = 0;
                    break;
                }
                char *elo1 = memmem(index, datastart - index, "Elo", 3);
                index = elo1 + 3; 
                char *elo2 = memmem(index, datastart - index, "Elo", 3);
                index = elo1 + 3; 
                if (elo1 != NULL && elo2 != NULL) {
                    int elo1val = extract_elo(elo1);
                    int elo2val = extract_elo(elo2);
                    if (elo1val >= low_elo && elo1val <= high_elo && elo2val >= low_elo && elo2val <= high_elo) {
                        iov[iovecoffset].iov_base = headerstart; 
                        iov[iovecoffset++].iov_len = (char *)datastart + 2 - headerstart;
                        if (iovecoffset == 40) {
                            writev(1, iov, 40);
                            iovecoffset = 0;
                        }
                        print_data = true;
                    }
                }
                datastart += 2; 
                index = datastart;
                isheader = false;

            } else {
                headerstart = memmem(index, end - index, "\n\n", 2);
                if (headerstart == NULL) {
                    indexoffset = (size_t)index - ((size_t)index & (~(PAGESIZE - 1)));
                    fileoffset += ((size_t)index & (~(PAGESIZE - 1))) - (size_t)begin;
                    writev(1, iov, iovecoffset);
                    iovecoffset = 0;
                    break;
                }

                while (headerstart < end && *headerstart == '\n')
                    headerstart++;

                if (print_data) {
                    iov[iovecoffset].iov_base = datastart;
                    iov[iovecoffset++].iov_len = (char *)headerstart + 2 - index;
                    if (iovecoffset == 40) {
                        writev(1, iov, 40);
                        iovecoffset = 0;
                    }
                    print_data = false;
                }
                headerstart += 2;
                index = headerstart;
                isheader = true;
            }
        }
        writev(1, iov, iovecoffset);
    }

    exit(EXIT_SUCCESS);

remap:
    if (fileoffset != 0)
        munmap(begin, WINDOWSIZE);
    void *addr = mmap(NULL, WINDOWSIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, fileoffset);
    madvise(addr, WINDOWSIZE, MADV_WILLNEED | MADV_SEQUENTIAL);
    begin = addr;
    end = addr + WINDOWSIZE;
    index = begin + indexoffset;
    goto done;
}
