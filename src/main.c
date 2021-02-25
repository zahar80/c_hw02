#include <stdio.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>

/* Size of the End of Central Directory Record, not including comment. */
#define EOCDR_BASE_SZ 22
#define EOCDR_SIGNATURE 0x06054b50  /* "PK\5\6" little-endian. */
#define CFH_SIGNATURE 0x02014b50  /* "PK\1\2" little-endian. */

#pragma pack(push,1)

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

#pragma pack(pop)

static void exit_program(const char* error) {
    fprintf(stderr, "%s\n", error);
    exit(1);
}

static uint32_t read32(FILE* fp) {
    uint32_t out;
    fread(&out, sizeof out, 1, fp);

    return out;
}

static uint8_t* get_string(FILE* fp, size_t length) {
    uint8_t* str = malloc(length + 1);

    fread(str, length, 1, fp);
    str[length] = '\0';

    return str;
}

static bool is_zip_found(FILE* fp) {
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

static EOCDR* get_eocdr(FILE* fp) {
    EOCDR* eocdr = malloc(sizeof(EOCDR));

    fread(eocdr, sizeof(EOCDR) - sizeof(uint8_t *), 1, fp);
    eocdr->comment = eocdr->comment_len ? get_string(fp, eocdr->comment_len) : NULL;

    return eocdr;
}

static void free_eocdr(EOCDR* r) {
    free(r->comment);
    free(r);
}

static CFH* get_cfh(FILE* fp) {
    fseek(fp, sizeof CFH_SIGNATURE, SEEK_CUR);
    CFH* cfh = malloc(sizeof(CFH));

    fread(cfh, sizeof(CFH) - sizeof(uint8_t *) * 3, 1, fp);
    cfh->name = cfh->name_len ? get_string(fp, cfh->name_len) : NULL;
    cfh->extra = cfh->extra_len ? get_string(fp, cfh->extra_len) : NULL;
    cfh->comment = cfh->comment_len ? get_string(fp, cfh->comment_len) : NULL;

    return cfh;
}

static void free_cfh(CFH* cfh) {
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
            free_cfh(cfhp_array[i]);
        }
        free_eocdr(eocdr_p);
    } else {
        printf("Zip was not found\n");
    }

    fclose(fp);

    return 0;
}
