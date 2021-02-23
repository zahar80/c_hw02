#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

/* Size of the End of Central Directory Record, not including comment. */
#define EOCDR_BASE_SZ 22
#define EOCDR_SIGNATURE 0x06054b50  /* "PK\5\6" little-endian. */
#define CFH_SIGNATURE 0x02014b50  /* "PK\1\2" little-endian. */

/* End of Central Directory Record. */
typedef struct {
    uint16_t disk_nbr;        /* Number of this disk. */
    uint16_t cd_start_disk;   /* Nbr. of disk with start of the CD. */
    uint16_t disk_cd_entries; /* Nbr. of CD entries on this disk. */
    uint16_t cd_entries;      /* Nbr. of Central Directory entries. */
    uint32_t cd_size;         /* Central Directory size in bytes. */
    uint32_t cd_offset;       /* Central Directory file offset. */
    uint16_t comment_len;     /* Archive comment length. */
    uint8_t *comment;         /* Archive comment. */
} EOCDR;

/* Central File Header (Central Directory Entry) */
typedef struct {
    uint16_t made_by_ver;    /* Version made by. */
    uint16_t extract_ver;    /* Version needed to extract. */
    uint16_t gp_flag;        /* General purpose bit flag. */
    uint16_t method;         /* Compression method. */
    uint16_t mod_time;       /* Modification time. */
    uint16_t mod_date;       /* Modification date. */
    uint32_t crc32;          /* CRC-32 checksum. */
    uint32_t comp_size;      /* Compressed size. */
    uint32_t uncomp_size;    /* Uncompressed size. */
    uint16_t name_len;       /* Filename length. */
    uint16_t extra_len;      /* Extra data length. */
    uint16_t comment_len;    /* Comment length. */
    uint16_t disk_nbr_start; /* Disk nbr. where file begins. */
    uint16_t int_attrs;      /* Internal file attributes. */
    uint32_t ext_attrs;      /* External file attributes. */
    uint32_t lfh_offset;     /* Local File Header offset. */
    uint8_t *name;           /* Filename. */
    uint8_t *extra;          /* Extra data. */
    uint8_t *comment;        /* File comment. */
} CFH;

void exit_program(const char* error) {
    fprintf(stderr, "%s\n", error);
    exit(1);
}

uint32_t read32(FILE* fp) {
    uint32_t out;
    uint8_t* c = (uint8_t *)&out;

    for (int i = 0; i < 4; i++) {
        *(c + i) = fgetc(fp);
    }

    return out;
}

uint16_t read16(FILE* fp) {
    uint16_t out;
    uint8_t* c = (uint8_t *)&out;

    for (int i = 0; i < 2; i++) {
        *(c + i) = fgetc(fp);
    }

    return out;
}

uint8_t* get_string(FILE* fp, size_t length) {
    uint8_t* str = malloc(length + 1);

    for (size_t i = 0; i < length; i++) {
        *(str + i) = fgetc(fp);
    }
    str[length] = '\0';

    return str;
}

bool is_zip_found(FILE* fp) {
    bool zip_found = false;
    
    fseek(fp, 0L, SEEK_END);
    unsigned long src_len = ftell(fp);

    uint32_t signature;
    for (size_t comment_len = 0; comment_len <= UINT16_MAX; comment_len++) {
        if (src_len < EOCDR_BASE_SZ + comment_len) {
            break;
        }

        fseek(fp, src_len - EOCDR_BASE_SZ - comment_len, SEEK_SET);
        signature = read32(fp);

        if (signature == EOCDR_SIGNATURE) {
            zip_found = true;
            break;
        }
    }

    return zip_found;
}

EOCDR* get_eocdr(FILE* fp) {
    EOCDR* eocdr = malloc(sizeof(EOCDR));

    eocdr->disk_nbr = read16(fp);
    eocdr->cd_start_disk = read16(fp);
    eocdr->disk_cd_entries = read16(fp);
    eocdr->cd_entries = read16(fp);
    eocdr->cd_size = read32(fp);
    eocdr->cd_offset = read32(fp);
    eocdr->comment_len = read16(fp);
    eocdr->comment = NULL;

    if (eocdr->comment_len) {
        eocdr->comment = get_string(fp, eocdr->comment_len);
    }

    return eocdr;
}

void free_eocdr(EOCDR* r) {
    free(r->comment);
    free(r);
}

CFH* get_cfh(FILE* fp) {
    fseek(fp, sizeof CFH_SIGNATURE, SEEK_CUR);
    CFH* cfh = malloc(sizeof(CFH));

    cfh->made_by_ver = read16(fp);
    cfh->extract_ver = read16(fp);
    cfh->gp_flag = read16(fp);
    cfh->method = read16(fp);
    cfh->mod_time = read16(fp);
    cfh->mod_date = read16(fp);
    cfh->crc32 = read32(fp);
    cfh->comp_size = read32(fp);
    cfh->uncomp_size = read32(fp);
    cfh->name_len = read16(fp);
    cfh->extra_len = read16(fp);
    cfh->comment_len = read16(fp);
    cfh->disk_nbr_start = read16(fp);
    cfh->int_attrs = read16(fp);
    cfh->ext_attrs = read32(fp);
    cfh->lfh_offset = read32(fp);
    cfh->name = NULL;
    cfh->extra = NULL;
    cfh->comment = NULL;

    if (cfh->name_len) {
        cfh->name = get_string(fp, cfh->name_len);
    }

    if (cfh->extra_len) {
        cfh->extra = get_string(fp, cfh->extra_len);
    }

    if (cfh->comment_len) {
        cfh->comment = get_string(fp, cfh->comment_len);
    }

    return cfh;
}

void free_cfh(CFH* cfh) {
    free(cfh->name);
    free(cfh->extra);
    free(cfh->comment);
    free(cfh);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        exit_program("Args must contain the filename");
    }

    FILE* fp = fopen(argv[1], "r");
    if (fp == NULL) {
        exit_program("File opening error");
    }

    if (is_zip_found(fp)) {
        printf("Zip was found\n");
        const long start_of_eocdr = ftell(fp) - sizeof EOCDR_SIGNATURE;

        EOCDR* eocdr_p = get_eocdr(fp);
        const int files_number = eocdr_p->cd_entries;
        printf("Number of files: %d\n", files_number);

        fseek(fp, start_of_eocdr - eocdr_p->cd_size, SEEK_SET);
        CFH* cfhp_array[files_number];
        for (int i = 0; i < files_number; i++) {
            cfhp_array[i] = get_cfh(fp);
        }

        for (int i = 0; i < files_number; i++) {
            printf("%d. File name: %s\n", i + 1, cfhp_array[i]->name);
        }

        for (int i = 0; i < files_number; i++) {
            free(cfhp_array[i]);
        }
        free_eocdr(eocdr_p);
    } else {
        printf("Zip was not found\n");
    }

    fclose(fp);

    return 0;
}
