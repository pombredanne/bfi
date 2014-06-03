#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "murmur.h"
#include "bfi.h"

#define BFI_VERSION 0x02

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
char * bfi_generate(char * input[], int items) {
    // use 128 bytes and 12 sectors
    uint32_t hash;
    uint32_t i, offset, pos;
    char * bloom;
    
    //for(i=0; i<items; i++) printf("GENERATE: %s\n", input[i]);
    
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
int bfi_contains(char * haystack, char * needle, int len) {
    int i;
    
    for(i=0; i<len; i++) {
        //printf("%02x=%02x ", haystack[i] & needle[i], needle[i]);
        if((haystack[i] & needle[i]) != needle[i]) return 0;   
    }
    return 1;
}

void bfi_dump(bfi * index, int full) {
    if(full) {
        printf("--\nMagic number: %d\n", index->magic_number);
        printf("Version: %d\n", index->version);
        printf("Records: %d\n", index->records);
    }
    
    printf("%08lx Current page: %d, dirty: %d\n", (long)index, index->current_page, index->page_dirty);
}

bfi * bfi_open(char * filename) {
    bfi * result;
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
    
    result->current_page = -1;
    result->page_dirty = 0;
    
    //bfi_dump(result, 1);
    
    return result;
}

int bfi_sync(bfi * index) {
    int size;
    
    if(!index->page_dirty) return index->records;
    
    //printf("Syncing page %d\n", index->current_page);
    size = (sizeof(uint32_t) + BLOOM_SIZE) * BFI_PAGE_SIZE;
    
    fseek(index->fp, BFI_HEADER + (size * index->current_page), SEEK_SET);
    fwrite(&index->pks, sizeof(uint32_t), BFI_PAGE_SIZE, index->fp);
    fwrite(index->page, BFI_PAGE_SIZE, BLOOM_SIZE, index->fp);
    
    index->current_page = -1;
    index->page_dirty = 0;
    
    return index->records;
}

void bfi_close(bfi * index) {
    bfi_sync(index);
    
    //printf("Writing header\n");
    fseek(index->fp, 0, SEEK_SET);
    fwrite(index, 1, BFI_HEADER, index->fp);
    fclose(index->fp);
    
    free(index);
}

void bfi_load_page(bfi * index, int page) {
    int size, c;
    
    if(page == index->current_page)  return;
    bfi_sync(index);
    
    size = (sizeof(uint32_t) + BLOOM_SIZE) * BFI_PAGE_SIZE;
    
    fseek(index->fp, BFI_HEADER + (size * page), SEEK_SET);
    //printf("Loading page %d from 0x%04lx\n", page, ftell(index->fp));
    c = fread(index->pks, sizeof(uint32_t), BFI_PAGE_SIZE, index->fp);
    if(c < BFI_PAGE_SIZE) {
        // primary keys are always all or nothing
        memset(index->pks, 0, sizeof(uint32_t) * BFI_PAGE_SIZE);
    }
    c = fread(index->page, 1, BFI_PAGE_SIZE * BLOOM_SIZE, index->fp);
    if( c < BFI_PAGE_SIZE * BLOOM_SIZE) {
        memset(index->page, 0, BFI_PAGE_SIZE * BLOOM_SIZE);
    }
    
    // leave the file pointer at the end of the page so a full page is writen
    fseek(index->fp, BFI_HEADER + (size * (page + 1)), SEEK_SET);
    
    index->current_page = page;
    
    index->page_dirty = 0;
    //bfi_dump(index, 0);
}

int bfi_index(bfi * index, int pk, char * input[], int items) {
    int page, offset, i;
    char * p, * data;
    
    page = index->records / BFI_PAGE_SIZE;
    offset = index->records % BFI_PAGE_SIZE;
    
    //printf("Page: %d, offset: %d\n", page, offset);
    bfi_load_page(index, page);
    //bfi_dump(index, 0);
    
    data = bfi_generate(input, items);
    
    // write the PK
    index->pks[offset] = pk;
    
    // write the data
    p = index->page;
    p += offset;
    for(i=0; i<BLOOM_SIZE; i++) {
        *p = data[i];
        p += BFI_PAGE_SIZE;
    }
    
    index->page_dirty = 1;
    index->records++;
    
    return 0;
}

int bfi_lookup(bfi * index, char * input[], int items, uint32_t ** ptr) {
    int page, total_pages, i, j, buf_size, count;
    char * data, matches[BFI_PAGE_SIZE];
    char * p_data, * p_index;
    uint32_t * result;
    
    count = 0;
    result = NULL;
    
    total_pages = (index->records / BFI_PAGE_SIZE) + 1;
    
    data = bfi_generate(input, items);
    
    for(page=0; page<total_pages; page++) {
        bfi_load_page(index, page);
        //printf("PKS: ");
        //for(i=0; i<BFI_PAGE_SIZE; i++) printf("%4d ", index->pks[i]);
        //printf("\n");
        
        memset(matches, 1, BFI_PAGE_SIZE);
        
        p_data = data;
        p_index = index->page;
        
        for(i=0; i<BLOOM_SIZE; i++) {
            if(*p_data == 0) {
                p_index += BFI_PAGE_SIZE;
                //printf("-");
            } else {
                //printf("DATA: %02x\nINDEX: ", *p_data);
                for(j=0; j<BFI_PAGE_SIZE; j++) {
                    //printf("%02x ", *p_index);
                    if((*p_data & *p_index) != *p_data) {
                        matches[j] = 0;
                    }
                    p_index++;
                }
                //printf("\nMATCHES: ");
                //for(j=0; j<BFI_PAGE_SIZE; j++) printf("%d", (int)matches[j]);
                //printf("\n");
            }
            p_data++;
        }
        
        // output the result
        for(i=0; i<BFI_PAGE_SIZE; i++) {
            if(matches[i]) {
                if(count % 100 == 0) {
                    buf_size = ((count / 100) + 1) * 100;
                    result = realloc(result, sizeof(uint32_t) * buf_size);
                }
                result[count++] = index->pks[i];
                //printf("%d ", index->pks[i]);
            }
        }
        
    }
    
    // shrink it back to actual size
    if(count) {
        result = realloc(result, sizeof(uint32_t) * count);
    }
    
    *ptr = result;
    
    return count;
}
