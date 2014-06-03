#include <stdio.h>
#include "bfi.h"

int usage() {
    fprintf(stderr, "Usage: bfi index <file> <pk> <value> [<value> ...]\n");
    fprintf(stderr, "       bfi index <file> (read from stdin)");
    fprintf(stderr, "       bfi lookup <file> <value> [<value> ...]\n");
    return 1;
}

int main(int argc, char *argv[]) {
    bfi *index;
    int pk;
    
    if(argc < 3) return usage();
    
    index = bfi_open(argv[1]);
    if(index == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", argv[1]);
        return -42;
    }
    
    if(strcmp(argv[2], "index") == 0) {
        
        if(argc < 4) { // read from stdin
            printf("Indexed %d items\n", bfi_index_stdin(index, 0));
        } else { // single input
            sscanf(argv[3], "%d", &pk);
            bfi_index(index, pk, &argv[4], argc-3);
        }
        
    } else if(strcmp(argv[2], "lookup") == 0) {
        if(argc < 3) return usage();
        
        bfi_lookup(index, &argv[2], argc-2);
    } else {
        return usage();
    }
    
    bfi_close(index);
    return 0;
}