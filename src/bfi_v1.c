#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "murmur.h"
#include "bfi.h"

#define BFI_VERSION 0x01

/**
 * A super-simple bloom filter implementation optomised for alignment and efficiency
 * 
 * Uses a fixed length of 1024 bits with 4 sectors which according to my possibly
 * dodgy maths leads to the following probabilities of error.
 * 10 fields: 2.15e-6
 * 20 fields: 3.19e-5
 * 30 fields: 1.50e-4
 * 
 * The approximate target was 0.0001 so this holds till over 30 fields are indexed.
 */
char* bfi_generate(char *input[], int items) {
    // use 128 bytes and 12 sectors
    uint32_t hash;
    uint32_t i, offset, pos;
    char *bloom;
    
    bloom = malloc(BLOOM_SIZE);
    memset(bloom, 0, BLOOM_SIZE);
 
    for(i=0; i<items; i++) { 
        //printf("VALUE: %s\n", input[i]);
        hash = murmur3_32(input[i], strlen(input[i]), 362582);
        for(offset=0; offset<4; offset++) {
            pos = 0xFF & hash;
            //printf("POS: %d\n", pos);
            hash >>= 8;
            bloom[(offset * 32) + (pos / 8)] |= (1 << (pos % 8));
        }
    }
    
    //for(i=0; i<128; i++) printf("%02x ", bloom[i]);
    //printf("\n");
    return bloom;
}

/**
 * Checks whether one bloom filter is within another
 */
int bfi_contains(char *haystack, char *needle, int len) {
    int i;
    
    for(i=0; i<len; i++) {
        //printf("%02x=%02x ", haystack[i] & needle[i], needle[i]);
        if((haystack[i] & needle[i]) != needle[i]) return 0;   
    }
    return 1;
}

void bfi_dump(bfi *index, int full) {
    if(full) {
        printf("--\nMagic number: %d\n", index->magic_number);
        printf("Version: %d\n", index->version);
        printf("Records: %d\n", index->records);
    }
    
    printf("%08lx Current page: %d, dirty: %d\n", (long)index, index->current_page, index->page_dirty);
}

bfi* bfi_open(char *filename) {
    bfi *result;
    int i;
    
    result = malloc(sizeof(bfi));
    
    result->fp = fopen(filename, "r+b");
    if(!result->fp) result->fp = fopen(filename, "w+b");
    if(!result->fp) return NULL;
    
    fseek(result->fp, 0, 0);
    i = fread(result, 1, BFI_HEADER, result->fp);
    if(i == 0) {
        printf("Creating new file (only loaded %d bytes)\n", i);
        result->magic_number = BFI_MAGIC;
        result->version = BFI_VERSION;
        result->records = 0;
    }
    
    if(result->magic_number != BFI_MAGIC) {
        fprintf(stderr, "Bad magic number: 0x%0x\n", result->magic_number);
        return NULL;
    }
    
    if(result->version != BFI_VERSION) {
        fprintf(stderr, "Incorrect version - expected %d, got %d\n", BFI_VERSION, result->version);
        return NULL;
    }
    
    fseek(result->fp, BFI_HEADER, SEEK_SET);
    
    return result;
}

void bfi_close(bfi *index) {
    
    //printf("Writing header\n");
    fseek(index->fp, 0, SEEK_SET);
    fwrite(index, 1, BFI_HEADER, index->fp);
    fclose(index->fp);
    
    free(index);
}

int bfi_index(bfi *index, int pk, char *input[], int items) {
    char *data;
    
    data = bfi_generate(input, items);
    
    
    fwrite(&pk, sizeof(uint32_t), 1, index->fp);
    fwrite(data, 1, BLOOM_SIZE, index->fp);
    
    index->records++;
}

int bfi_index_stdin(bfi *index, int row) {
    char *values[100];
    size_t nbytes=10;
    int c, i, j, pk;
    
    char *line = malloc(100);
    
    for(;;) {
        c = getline(&line, &nbytes, stdin);
        if(c == -1) break;
        line[c-1] = 0;
        //printf("LINE: %s (%d bytes)\n", line, c);
        
        // 
        if(sscanf(line, "%d", &pk) < 1) {
            fprintf(stderr, "Failed to parse primary key for row %d\n", row);
            return row;
        }
        //printf("PK: %d\n", pk);
        
        // rather hacky conversion of string to array
        i = 0;
        values[i++] = line;
        for(j=0; j<c; j++) {
            if(line[j] == ' ') {
                line[j] = 0;
                values[i++] = &line[j+1];
            }
        }
        
        bfi_index(index, pk, values, i);
        
        row++;
    }
    
    return row;
}

void bfi_lookup(bfi *index, char *input[], int items) {
    char *data, *mask;
    int pk;
    
    mask = bfi_generate(input, items);
    fseek(index->fp, BFI_HEADER, SEEK_SET);
    
    data = malloc(BLOOM_SIZE);
    
    while(!feof(index->fp)) {
        fread(&pk, sizeof(uint32_t), 1, index->fp);
        fread(data, 1, BLOOM_SIZE, index->fp);
        
        if(bfi_contains(data, mask, BLOOM_SIZE)) printf("%d ", pk);
        
    }
    
    free(data);
    
    printf("\n");
    
}
