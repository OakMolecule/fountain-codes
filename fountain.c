#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "fountain.h"

#define BLK_SIZE 4
#define BUFFER_SIZE 256

static inline void memerror(int line) {
    printf("Memory allocation error, Line: %d", line);
    return;
}

static char * xorncpy (char* destination, const char* source, register size_t n) {
    register char* d = destination;
    register const char* s = source;
    do {
        if (*s) *d++ ^= *s++;
        else break;
    } while (--n != 0);
    return (destination);
}

static int size_in_blocks(const char* string, int blk_size) {
    int string_len = strlen(string);
    return (string_len % blk_size)
        ? (string_len / blk_size) + 1 : string_len / blk_size;
}

fountain_t* make_fountain(const char* string, int blk_size) {
    fountain_t* output = malloc(sizeof *output);
    if (output == NULL) {
        memerror(__LINE__);
        return NULL;
    }
    memset(output, 0 , sizeof(fountain_t));
    int n = size_in_blocks(string, blk_size);

    // Create distibution like 111112222333445
    int* dist = malloc((n*(n+1)/2) * sizeof(*dist));
    if (!dist)
        goto free_dist;

    int i, j, m, *lpdist;
    lpdist = dist;
    for (m=n; m > 0; m--) {
        for (i=0; i<m; i++) {
            *lpdist = n - m + 1;
            lpdist++;
        }
    }

    // Pick d
    int d = rand() % (n*(n+1)/2);
    d = dist[d];

    output->num_blocks = d;
    output->block = malloc(d * sizeof *(output->block));
    if (!output->block)
        goto free_ob;
    for (i = 0; i < d; i++) {
        output->block[i] = rand() % n;
        for (j = 0; j < i; j++) {
            if (output->block[i] == output->block[j]) {
                i--;
                break;
            }
        }
    }

    // XOR blocks together
    output->string = calloc(blk_size+1, sizeof *(output->string));
    if (!output->string)
        goto free_os;
    for (i = 0; i < d; i++) {
        m = output->block[i] * blk_size;
        xorncpy(output->string, string + m, blk_size);
    }

    // Cleanup
    free(dist);

    return output;

free_os:
free_ob:
    free_fountain(output);
free_dist:
    free(dist);
    return NULL;
}

void free_fountain(fountain_t* ftn) {
    if (ftn->block) free(ftn->block);
    if (ftn->string) free(ftn->string);
    free(ftn);
}

int cmp_fountain(fountain_t* ftn1, fountain_t* ftn2) {
    int ret;
    if (ret = (ftn1->num_blocks - ftn2->num_blocks))
        return ret;
    if (ret = strcmp(ftn1->string, ftn2->string))
        return ret;

    int i=0;
    for (; i < ftn1->num_blocks; ++i) {
        if (ret = (ftn1->block[i] - ftn2->block[i]))
            return ret;
    }

    return 0;
}

char* decode_fountain(const char* string, int blk_size) {
    int n = size_in_blocks(string, blk_size);
    char * output = calloc(strlen(string) + 1 , sizeof *output);
    if (output == NULL) {
        memerror(__LINE__);
        return NULL;
    }
    fountain_t * curr_fountain = NULL;
    packethold_t hold;

    // Create the hold for storing packets for later
    hold.fountain = malloc(BUFFER_SIZE * sizeof *(hold.fountain));
    if (hold.fountain == NULL) {
        memerror(__LINE__);
        goto exit;
    }
    hold.iSlots = BUFFER_SIZE;
    hold.iPackets = 0;
    hold.offset = 0;


    int * blkdecoded = calloc(n, sizeof *blkdecoded);
    if (blkdecoded == NULL) {
        memerror(__LINE__);
        goto exit;
    }

    int i, j, solved = 0, newfount = 1, f_num = 0;

    while (!solved) {
        // recv fountain packet
        if (newfount) {
            curr_fountain = make_fountain(string, blk_size);
            f_num++;
        }
        newfount = 1;

        // Case one, block size one
        if (curr_fountain->num_blocks == 1) {
            if (blkdecoded[curr_fountain->block[0]] == 0) {
                strncpy(output + (curr_fountain->block[0] * blk_size),
                        curr_fountain->string, blk_size);
                blkdecoded[curr_fountain->block[0]] = 1;
            } else {
                continue; // continue if receiving solved block
            }

            //Part two check against blocks in hold
            int match = 0;
            for (i = 0; i < hold.iPackets; i++) {
                for (j = 0; j < hold.fountain[i].num_blocks; j++ ) {
                    if (hold.fountain[i].block[j]
                            == curr_fountain->block[0]) {
                        // Xor out the hold block
                        xorncpy(hold.fountain[i].string,
                                curr_fountain->string,
                                blk_size);

                        // Remove removed blk number
                        for (j = i; j < hold.fountain[i].num_blocks-1; j++) {
                            hold.fountain[i].block[j] =
                                hold.fountain[i].block[j + 1];
                        }
                        j = hold.fountain[i].num_blocks - 1;
                        hold.fountain[i].block[j] = 0;
                        // Reduce number of blocks held
                        hold.fountain[i].num_blocks--;
                        match = 1;
                        break;
                    }
                }
                if (!match) continue;
                // on success check if hold packet is of size one block
                if (hold.fountain[i].num_blocks == 1) {
                    //move into output
                    if (blkdecoded[hold.fountain[i].block[0]] == 0) {
                        strncpy(output + (hold.fountain[i].block[0]*blk_size),
                                hold.fountain[i].string,
                                blk_size);

                        blkdecoded[hold.fountain[i].block[0]] = 1;
                    }
                    //remove from hold
                    for (j = i; j < hold.iPackets - 1; j++) {
                        hold.fountain[j] = hold.fountain[j+1];
                    }
                    memset(hold.fountain + hold.offset - 1, 0,
                            sizeof *(hold.fountain));
                    hold.offset--;
                    hold.iPackets--;
                }
            }
        } else {
            //Check against solved blocks
            for (i = 0; i < curr_fountain->num_blocks; i++) {
                if (blkdecoded[curr_fountain->block[i]]) {
                    //Xor the decoded block out of new packet
                    xorncpy(curr_fountain->string,
                            output + (curr_fountain->block[i]*blk_size),
                            blk_size);

                    // Remove decoded block number
                    for (j = i; j < curr_fountain->num_blocks - 1; j++) {
                        curr_fountain->block[j] = curr_fountain->block[j+1];
                    }
                    j = curr_fountain->num_blocks - 1;
                    curr_fountain->block[j] = 0;

                    //reduce number of blocks held
                    curr_fountain->num_blocks--;

                    //retest current reduced packet
                    newfount = 0;
                    break;
                }
            }
            if (!newfount) continue;

            // check if packet is already in hold
            int inhold = 0;
            for (i = 0; i < hold.iSlots; i++) {
                if (!cmp_fountain(curr_fountain, hold.fountain + i)) {
                    inhold = 1;
                    break;
                }
            }
            if (!inhold) {
                // Add packet to hold
                if (hold.offset >= hold.iSlots) {
                    int multi = (hold.iSlots / BUFFER_SIZE) + 1;

                    hold.fountain = realloc(hold.fountain,
                            multi * BUFFER_SIZE * sizeof *(hold.fountain));

                    if (hold.fountain == NULL) {
                        memerror(__LINE__);
                        goto exit;
                    }
                    hold.iSlots = multi * BUFFER_SIZE;

                }
                hold.fountain[hold.offset++] = *curr_fountain;
                hold.iPackets++;
            }

            free(curr_fountain); /* we can free it now that it's been copied */
        }

        // update solved
        solved = 1;
        for (i = 0; i < n; i++) {
            if(!blkdecoded[i]) {
                solved = 0;
                break;
            }
        }
    }
    printf("Number of packets required: %d\n", f_num);
    // Cleanup
    free_fountain(curr_fountain);

    return (char*) output;

exit:
    if (blkdecoded) free(blkdecoded);
    if (hold.fountain) free_fountain(hold.fountain);

    return NULL;
}

