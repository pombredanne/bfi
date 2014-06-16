#include <stdio.h>
#include <stdlib.h>
#include "bfi.h"

int index_stdin(bfi *index, int row) {
    char *values[100];
    size_t nbytes=10;
    int c, i, j, pk;

    char *line = malloc(100);

    for(;;) {
        c = getline(&line, &nbytes, stdin);
        if(c == -1) break;
        line[c-1] = 0;
        //printf("LINE: %s (%d bytes)\n", line, c);

        // rather hacky conversion of string to array
        i = 0;
        values[i++] = line;
        for(j=0; j<c; j++) {
            if(line[j] == ' ') {
                line[j] = 0;
                values[i++] = &line[j+1];
            }
        }

        bfi_append(index, values, i);

        row++;
    }

    return row;
}

int usage() {
    fprintf(stderr, "Usage: bfi append <file> <value> [<value> ...]\n");
    fprintf(stderr, "       bfi append <file> (read from stdin)\n");
    fprintf(stderr, "       bfi lookup <file> <value> [<value> ...]\n");
    return -255;
}

int main(int argc, char *argv[]) {
  bfi *index;
  int pk;

  if(argc < 3) return usage();

  index = bfi_open(argv[2], BFI_FORMAT_128);
  if(index == NULL) {
    fprintf(stderr, "Failed to open file: %s\n", argv[2]);
    return -42;
  }

  if(strcmp(argv[1], "append") == 0) {

    if(argc < 4) { // read from stdin
      printf("Indexed %d items\n", index_stdin(index, 0));
    } else { // single input
      sscanf(argv[3], "%d", &pk);
      bfi_append(index, &argv[3], argc-3);
    }
    
    bfi_close(index);
    return 0;
  }

  if(strcmp(argv[1], "lookup") == 0) {
    if(argc < 3) return usage();

    uint32_t *result;
    int c, i;

    c = bfi_lookup(index, &argv[3], argc-3, &result);
    if(c) {
      for(i=0; i<c; i++) printf("%d\n", result[i]);
      fprintf(stderr, "%d results found\n", c);
      free(result);
    } else {
      fprintf(stderr, "No results found\n");
    }

    bfi_close(index);
    return c ? 0 : 1;
  }

  if(strcmp(argv[1], "info") == 0) {
    printf("Slots: %d\n", index->slots);
    printf("Pages: %d\n", index->total_pages);
    printf("Bloom format: %dB\n", index->format);
    printf("Page size: %dkB\n", index->page_size/1024);
    bfi_close(index);
    return 0;
  }

  bfi_close(index);  
  return usage();
}
