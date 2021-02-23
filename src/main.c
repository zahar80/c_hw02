#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

/* Size of the End of Central Directory Record, not including comment. */
#define EOCDR_BASE_SZ 22
#define EOCDR_SIGNATURE 0x06054b50  /* "PK\5\6" little-endian. */

void exit_program(const char* error) {
    fprintf(stderr, "%s\n", error);
    exit(1);
}

void read32(FILE* fp, long offset, uint32_t* out) {
    fseek(fp, offset, SEEK_SET);
    for (int i = 0; i < 4; i++) {
        *((uint8_t *)out + i) = fgetc(fp);
    }
}

bool is_zip_found(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        exit_program("File opening error");
    }

    fseek(fp, 0L, SEEK_END);
    unsigned long src_len = ftell(fp);

    uint32_t signature;
    bool zip_found = false;
    for (size_t comment_len = 0; comment_len <= UINT16_MAX; comment_len++) {
        if (src_len < EOCDR_BASE_SZ + comment_len) {
            break;
        }

        read32(fp, src_len - EOCDR_BASE_SZ - comment_len, &signature);

        if (signature == EOCDR_SIGNATURE) {
            zip_found = true;
            break;
        }
    }

    fclose(fp);
    return zip_found;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        exit_program("Args must contain the filename");
    }

    if (is_zip_found(argv[1])) {
        printf("Zip was found\n");
    } else {
        printf("Zip was not found\n");
    }

    return 0;
}
