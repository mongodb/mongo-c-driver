#include "test.h"
#include "md5.h"
#include "mongo.h"
#include "gridfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define UPPER 1024*1024
#define LOWER 1024*128
#define DELTA 1024*128

void fill_buffer_randomly(char * data, size_t length)
{
    int i, random;
    char * letters = "abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int nletters = strlen(letters)+1;
    
    for (i = 0; i < length; i++) {
        random = rand() % nletters;
        data[i] = letters[random];
    }
}

static void digest2hex(mongo_md5_byte_t digest[16], char hex_digest[33]){
    static const char hex[16] = {'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
    int i;
    for (i=0; i<16; i++){
        hex_digest[2*i]     = hex[(digest[i] & 0xf0) >> 4];
        hex_digest[2*i + 1] = hex[ digest[i] & 0x0f      ];
    }
    hex_digest[32] = '\0';
}

void test_gridfile(gridfs *gfs, char *data_before, size_t length, char *filename, char *content_type) {
    gridfile gfile[1];
    char data_after[UPPER];
    FILE * fd;
    mongo_md5_state_t pms[1];
    mongo_md5_byte_t digest[16];
    char hex_digest[33];

    gridfs_find_filename(gfs, filename, gfile);
    ASSERT(gridfile_exists(gfile));
    
    fd = fopen("output", "w+");
    gridfile_write_file(gfile, fd);
    fseek(fd, 0, SEEK_SET);
    ASSERT(fread(data_after, length, sizeof(char), fd));
    fclose(fd);
    ASSERT( strncmp(data_before, data_after, length) == 0 );
    
    gridfile_read( gfile, length, data_after);
    ASSERT( strncmp(data_before, data_after, length) == 0 );

    ASSERT( strcmp( gridfile_get_filename( gfile ), filename ) == 0 ); 

    ASSERT( gridfile_get_contentlength( gfile ) == length );

    ASSERT( gridfile_get_chunksize( gfile ) == DEFAULT_CHUNK_SIZE );

    ASSERT( strcmp( gridfile_get_contenttype( gfile ), content_type ) == 0) ;

    mongo_md5_init(pms);
    mongo_md5_append(pms, (const mongo_md5_byte_t *)data_before, (int)length);
    mongo_md5_finish(pms, digest);
    digest2hex(digest, hex_digest);
    ASSERT( strcmp( gridfile_get_md5( gfile ), hex_digest ) == 0 );

    gridfile_destroy(gfile);
    gridfs_remove_filename(gfs, filename);
}

int main(void) {
    mongo_connection conn[1];
    mongo_connection_options opts;
    gridfs gfs[1];
    char data_before[UPPER];
    size_t i;
    FILE *fd;
    
    srand(time(NULL));

    INIT_SOCKETS_FOR_WINDOWS;

/*    strncpy(opts.host, TEST_SERVER, 255);*/
    strncpy(opts.host, "127.0.0.1", 255);
    opts.host[254] = '\0';
    opts.port = 27017;

    if (mongo_connect( conn , &opts )){
        printf("failed to connect\n");
        exit(1);
    }
      
    gridfs_init(conn, "test", "fs", gfs);
    
    for (i = LOWER; i <= UPPER; i+=DELTA) {
        fill_buffer_randomly(data_before, i);
        
        /* Input from buffer */ 
        gridfs_store_buffer(gfs, data_before, i, "input-buffer", "text/html");
        test_gridfile(gfs, data_before, i, "input-buffer", "text/html");
        
        /* Input from file */
        fd = fopen("input-file", "w");
        fwrite(data_before, sizeof(char), i, fd);
        fclose(fd);  
        gridfs_store_file(gfs, "input-file", "input-file", "text/html");
        test_gridfile(gfs, data_before, i, "input-file", "text/html");
    }
    
    gridfs_destroy(gfs);
    mongo_cmd_drop_db(conn, "test");
    mongo_destroy(conn);
    return 0;
}
