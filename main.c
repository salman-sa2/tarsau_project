#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_FILES 32
#define MAX_TOTAL_SIZE (200 * 1024 * 1024) // 200 MB

int is_ascii_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;

    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch < 0 || ch > 127) {
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    return 1; 
}

void archive_files(int argc, char *argv[]) {
    printf("..\n");
}


void extract_files(int argc, char *argv[]) {
    
    printf("..\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Hata: Eksik parametre!\n");
        return 1;
    }

    if (strcmp(argv[1], "-b") == 0) {
        archive_files(argc, argv);
    } else if (strcmp(argv[1], "-a") == 0) {
        extract_files(argc, argv);
    } else {
        fprintf(stderr, "Hata: Gecersiz parametre!\n");
        return 1;
    }

    return 0;
}
