/*--------------------------------------------------------------------*/
/* gridfs.c                                                           */
/* Author: Christopher Triolo                                         */
/*--------------------------------------------------------------------*/

#include "gridfs.h"
#include "mongo.h"
#include "bson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define TRUE 1
#define FALSE 0


/*--------------------------------------------------------------------*/

static bson * chunk_new(bson_oid_t id, int chunkNumber,
    const char * data, int len)

{
  bson * b;
  bson_buffer buf;

  b = (bson *)bson_malloc(sizeof(bson));
  if (b == NULL) return NULL;

  bson_buffer_init(&buf);
  bson_append_oid(&buf, "files_id", &id);
  bson_append_int(&buf, "n", chunkNumber);
  bson_append_binary(&buf, "data", 2, data, len);
  bson_from_buffer(b, &buf);
  return  b;
}

/*--------------------------------------------------------------------*/

static void chunk_free(bson * oChunk)

{
  bson_destroy(oChunk);
  free(oChunk);
}

/*--------------------------------------------------------------------*/

int gridfs_init(mongo_connection * client, const char * dbname,
    const char * prefix, gridfs* gfs)
{
  int options;
  bson_buffer bb;
  bson b;
  bson out;
  bson_bool_t success;

  gfs->client = client;

  /* Allocate space to own the dbname */
  gfs->dbname = (const char *)bson_malloc(strlen(dbname)+1);
  if (gfs->dbname == NULL) {
    return FALSE;
  }
  strcpy((char*)gfs->dbname, dbname);

  /* Allocate space to own the prefix */
  if (prefix == NULL) prefix = "fs";
  gfs->prefix = (const char *)bson_malloc(strlen(prefix)+1);
  if (gfs->prefix == NULL) {
    free((char*)gfs->dbname);
    return FALSE;
  }
  strcpy((char *)gfs->prefix, prefix);

  /* Allocate space to own files_ns */
  gfs->files_ns =
    (const char *) bson_malloc (strlen(prefix)+strlen(dbname)+strlen(".files")+2);
  if (gfs->files_ns == NULL) {
    free((char*)gfs->dbname);
    free((char*)gfs->prefix);
    return FALSE;
  }
  strcpy((char*)gfs->files_ns, dbname);
  strcat((char*)gfs->files_ns, ".");
  strcat((char*)gfs->files_ns, prefix);
  strcat((char*)gfs->files_ns, ".files");

  /* Allocate space to own chunks_ns */
  gfs->chunks_ns = (const char *) bson_malloc(strlen(prefix) + strlen(dbname)
              + strlen(".chunks") + 2);
  if (gfs->chunks_ns == NULL) {
    free((char*)gfs->dbname);
    free((char*)gfs->prefix);
    free((char*)gfs->files_ns);
    return FALSE;
  }
  strcpy((char*)gfs->chunks_ns, dbname);
  strcat((char*)gfs->chunks_ns, ".");
  strcat((char*)gfs->chunks_ns, prefix);
  strcat((char*)gfs->chunks_ns, ".chunks");

  bson_buffer_init(&bb);
  bson_append_int(&bb, "filename", 1);
  bson_from_buffer(&b, &bb);
  options = 0;
  success = mongo_create_index(gfs->client, gfs->files_ns, &b, options, &out);
  bson_destroy(&b);
  if (!success) {
    free((char*)gfs->dbname);
    free((char*)gfs->prefix);
    free((char*)gfs->files_ns);
    free((char*)gfs->chunks_ns);
    return FALSE;
  }

  bson_buffer_init(&bb);
  bson_append_int(&bb, "files_id", 1);
  bson_append_int(&bb, "n", 1);
  bson_from_buffer(&b, &bb);
  options = MONGO_INDEX_UNIQUE;
  success = mongo_create_index(gfs->client, gfs->chunks_ns, &b, options, &out);
  bson_destroy(&b);
  if (!success) {
    free((char*)gfs->dbname);
    free((char*)gfs->prefix);
    free((char*)gfs->files_ns);
    free((char*)gfs->chunks_ns);
    return FALSE;
  }

  return TRUE;
}

/*--------------------------------------------------------------------*/

void gridfs_destroy(gridfs* gfs)

{
  if (gfs == NULL) return;
  if (gfs->dbname) free((char*)gfs->dbname);
  if (gfs->prefix) free((char*)gfs->prefix);
  if (gfs->files_ns) free((char*)gfs->files_ns);
  if (gfs->chunks_ns) free((char*)gfs->chunks_ns);
}

/*--------------------------------------------------------------------*/

static bson gridfs_insert_file( gridfs* gfs, const char* name,
             const bson_oid_t id, gridfs_offset length,
             const char* contenttype)
{
  bson command;
  bson res;
  bson ret;
  bson_buffer buf;
  bson_iterator it;

  /* Check run md5 */
  bson_buffer_init(&buf);
  bson_append_oid(&buf, "filemd5", &id);
  bson_append_string(&buf, "root", gfs->prefix);
  bson_from_buffer(&command, &buf);
  assert(mongo_run_command(gfs->client, gfs->dbname,
         &command, &res));
  bson_destroy(&command);

  /* Create and insert BSON for file metadata */
  bson_buffer_init(&buf);
  bson_append_oid(&buf, "_id", &id);
  if (name != NULL && *name != '\0') {
    bson_append_string(&buf, "filename", name);
  }
  bson_append_long(&buf, "length", length);
  bson_append_int(&buf, "chunkSize", DEFAULT_CHUNK_SIZE);
  bson_append_date(&buf, "uploadDate", (bson_date_t)1000*time(NULL));
  bson_find(&it, &res, "md5");
  bson_append_string(&buf, "md5", bson_iterator_string(&it));
  bson_destroy(&res);
  if (contenttype != NULL && *contenttype != '\0') {
    bson_append_string(&buf, "contentType", contenttype);
  }
  bson_from_buffer(&ret, &buf);
  mongo_insert(gfs->client, gfs->files_ns, &ret);

  return ret;
}

/*--------------------------------------------------------------------*/

bson gridfs_store_buffer( gridfs* gfs, const char* data,
        gridfs_offset length, const char* remotename,
        const char * contenttype)

{
  char const * const end = data + length;
  bson_oid_t id;
  int chunkNumber = 0;
  int chunkLen;
  bson * oChunk;

  /* Large files Assertion */
  assert(length <= 0xffffffff);

  /* Generate and append an oid*/
  bson_oid_gen(&id);

  /* Insert the file's data chunk by chunk */
  while (data < end) {
    chunkLen = DEFAULT_CHUNK_SIZE < (unsigned int)(end - data) ?
      DEFAULT_CHUNK_SIZE : (unsigned int)(end - data);
    oChunk = chunk_new( id, chunkNumber, data, chunkLen );
    mongo_insert(gfs->client, gfs->chunks_ns, oChunk);
    chunk_free(oChunk);
    chunkNumber++;
    data += chunkLen;
  }

  /* Inserts file's metadata */
  return gridfs_insert_file(gfs, remotename, id, length, contenttype);
}

/*--------------------------------------------------------------------*/

void gridfile_writer_init( gridfile* gfile, gridfs* gfs,
    const char* remote_name, const char* content_type )
{
    gfile->gfs = gfs;

    bson_oid_gen( &(gfile->id) );
    gfile->chunk_num = 0;
    gfile->length = 0;
    gfile->pending_len = 0;
    gfile->pending_data = NULL;

    gfile->remote_name = (const char *)bson_malloc( strlen( remote_name ) + 1 );
    strcpy( (char *)gfile->remote_name, remote_name );

    gfile->content_type = (const char *)bson_malloc( strlen( content_type ) );
    strcpy( (char *)gfile->content_type, content_type );
}

/*--------------------------------------------------------------------*/

void gridfile_write_buffer( gridfile* gfile, const char* data, gridfs_offset length )
{

  int bytes_left = 0;
  int data_partial_len = 0;
  int chunks_to_write = 0;
  char* buffer;
  bson* oChunk;
  gridfs_offset to_write = length + gfile->pending_len;

  if ( to_write < DEFAULT_CHUNK_SIZE ) { /* Less than one chunk to write */
    if( gfile->pending_data ) {
      gfile->pending_data = (char *)bson_realloc((void *)gfile->pending_data, gfile->pending_len + to_write);
      memcpy( gfile->pending_data + gfile->pending_len, data, length );
    } else if (to_write > 0) {
      gfile->pending_data = (char *)bson_malloc(to_write);
      memcpy( gfile->pending_data, data, length );
    }
    gfile->pending_len += length;

  } else { /* At least one chunk of data to write */

    /* If there's a pending chunk to be written, we need to combine
     * the buffer provided up to DEFAULT_CHUNK_SIZE.
     */
    if ( gfile->pending_len > 0 ) {
      chunks_to_write = to_write / DEFAULT_CHUNK_SIZE;
      bytes_left = to_write % DEFAULT_CHUNK_SIZE;

      data_partial_len = DEFAULT_CHUNK_SIZE - gfile->pending_len;
      buffer = (char *)bson_malloc( DEFAULT_CHUNK_SIZE );
      memcpy(buffer, gfile->pending_data, gfile->pending_len);
      memcpy(buffer + gfile->pending_len, data, data_partial_len);

      oChunk = chunk_new(gfile->id, gfile->chunk_num, buffer, DEFAULT_CHUNK_SIZE);
      mongo_insert(gfile->gfs->client, gfile->gfs->chunks_ns, oChunk);
      chunk_free(oChunk);
      gfile->chunk_num++;
      gfile->length += DEFAULT_CHUNK_SIZE;
      data += data_partial_len;

      chunks_to_write--;

      free(buffer);
    }

    while( chunks_to_write > 0 ) {
      oChunk = chunk_new(gfile->id, gfile->chunk_num, data, DEFAULT_CHUNK_SIZE);
      mongo_insert(gfile->gfs->client, gfile->gfs->chunks_ns, oChunk);
      chunk_free(oChunk);
      gfile->chunk_num++;
      chunks_to_write--;
      gfile->length += DEFAULT_CHUNK_SIZE;
      data += DEFAULT_CHUNK_SIZE;
    }

    free(gfile->pending_data);

    /* If there are any leftover bytes, store them as pending data. */
    if( bytes_left == 0 )
      gfile->pending_data = NULL;
    else {
      gfile->pending_data = (char *)bson_malloc( bytes_left );
      memcpy( gfile->pending_data, data, bytes_left );
    }

    gfile->pending_len = bytes_left;
  }
}

/*--------------------------------------------------------------------*/

bson gridfile_writer_done( gridfile* gfile )
{

  /* write any remaining pending chunk data.
   * pending data will always take up less than one chunk
   */
  bson* oChunk;
  if( gfile->pending_data )
  {
    oChunk = chunk_new(gfile->id, gfile->chunk_num, gfile->pending_data, gfile->pending_len);
    mongo_insert(gfile->gfs->client, gfile->gfs->chunks_ns, oChunk);
    chunk_free(oChunk);
    free(gfile->pending_data);
    gfile->length += gfile->pending_len;
  }

  /* insert into files collection */
  return gridfs_insert_file(gfile->gfs, gfile->remote_name, gfile->id,
      gfile->length, gfile->content_type);
}

/*--------------------------------------------------------------------*/

bson gridfs_store_file(gridfs* gfs, const char* filename,
      const char* remotename, const char* contenttype)
{
  char buffer[DEFAULT_CHUNK_SIZE];
  FILE * fd;
  bson_oid_t id;
  int chunkNumber = 0;
  gridfs_offset length = 0;
  gridfs_offset chunkLen = 0;
  bson* oChunk;

  /* Open the file and the correct stream */
  if (strcmp(filename, "-") == 0) fd = stdin;
  else fd = fopen(filename, "rb");
  assert(fd != NULL); /* No such file */

  /* Generate and append an oid*/
  bson_oid_gen(&id);

  /* Insert the file chunk by chunk */
  chunkLen = fread(buffer, 1, DEFAULT_CHUNK_SIZE, fd);
  do {
    oChunk = chunk_new( id, chunkNumber, buffer, chunkLen );
    mongo_insert(gfs->client, gfs->chunks_ns, oChunk);
    chunk_free(oChunk);
    length += chunkLen;
    chunkNumber++;
    chunkLen = fread(buffer, 1, DEFAULT_CHUNK_SIZE, fd);
  } while (chunkLen != 0);

  /* Close the file stream */
  if (fd != stdin) fclose(fd);

  /* Large files Assertion */
  /* assert(length <= 0xffffffff); */

  /* Optional Remote Name */
  if (remotename == NULL || *remotename == '\0') {
    remotename = filename; }

  /* Inserts file's metadata */
  return gridfs_insert_file(gfs, remotename, id, length, contenttype);
}

/*--------------------------------------------------------------------*/

void gridfs_remove_filename(gridfs* gfs, const char* filename )

{
  bson query;
  bson_buffer buf;
  mongo_cursor* files;
  bson file;
  bson_iterator it;
  bson_oid_t id;
  bson b;

  bson_buffer_init(&buf);
  bson_append_string(&buf, "filename", filename);
  bson_from_buffer(&query, &buf);
  files = mongo_find(gfs->client, gfs->files_ns, &query, NULL, 0, 0, 0);
  bson_destroy(&query);

  /* Remove each file and it's chunks from files named filename */
  while (mongo_cursor_next(files)) {
    file = files->current;
    bson_find(&it, &file, "_id");
    id = *bson_iterator_oid(&it);

    /* Remove the file with the specified id */
    bson_buffer_init(&buf);
    bson_append_oid(&buf, "_id", &id);
    bson_from_buffer(&b, &buf);
    mongo_remove( gfs->client, gfs->files_ns, &b);
    bson_destroy(&b);

    /* Remove all chunks from the file with the specified id */
    bson_buffer_init(&buf);
    bson_append_oid(&buf, "files_id", &id);
    bson_from_buffer(&b, &buf);
    mongo_remove( gfs->client, gfs->chunks_ns, &b);
    bson_destroy(&b);
  }

}

/*--------------------------------------------------------------------*/

int gridfs_find_query(gridfs* gfs, bson* query,
         gridfile* gfile )

{
  bson_buffer date_buffer;
  bson uploadDate;
  bson_buffer buf;
  bson finalQuery;
  bson out;
  int i;

  bson_buffer_init(&date_buffer);
  bson_append_int(&date_buffer, "uploadDate", -1);
  bson_from_buffer(&uploadDate, &date_buffer);
  bson_buffer_init(&buf);
  bson_append_bson(&buf, "query", query);
  bson_append_bson(&buf, "orderby", &uploadDate);
  bson_from_buffer(&finalQuery, &buf);


  i = (mongo_find_one(gfs->client, gfs->files_ns,
         &finalQuery, NULL, &out));
  bson_destroy(&uploadDate);
  bson_destroy(&finalQuery);
  if (!i)
    return FALSE;
  else {
    gridfile_init(gfs, &out, gfile);
    bson_destroy(&out);
    return TRUE;
  }
}

/*--------------------------------------------------------------------*/

int gridfs_find_filename(gridfs* gfs, const char* filename,
       gridfile* gfile)

{
  bson query;
  bson_buffer buf;
  int i;

  bson_buffer_init(&buf);
  bson_append_string(&buf, "filename", filename);
  bson_from_buffer(&query, &buf) ;
  i = gridfs_find_query(gfs, &query, gfile);
  bson_destroy(&query);
  return i;
}

/*--------------------------------------------------------------------*/

int gridfile_init(gridfs* gfs, bson* meta, gridfile* gfile)

{
  gfile->gfs = gfs;
  gfile->pos = 0;
  gfile->meta = (bson*)bson_malloc(sizeof(bson));
  if (gfile->meta == NULL) return FALSE;
  bson_copy(gfile->meta, meta);
  return TRUE;
}

/*--------------------------------------------------------------------*/

void gridfile_destroy(gridfile* gfile)

{
  bson_destroy(gfile->meta);
  free(gfile->meta);
}

/*--------------------------------------------------------------------*/

bson_bool_t gridfile_exists(gridfile* gfile)

{
  return (bson_bool_t)(gfile != NULL || gfile->meta == NULL);
}

/*--------------------------------------------------------------------*/

const char* gridfile_get_filename(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "filename");
  return bson_iterator_string(&it);
}

/*--------------------------------------------------------------------*/

int gridfile_get_chunksize(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "chunkSize");
  return bson_iterator_int(&it);
}

/*--------------------------------------------------------------------*/

gridfs_offset gridfile_get_contentlength(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "length");

  if( bson_iterator_type( &it ) == bson_int )
    return (gridfs_offset)bson_iterator_int( &it );
  else
    return (gridfs_offset)bson_iterator_long( &it );
}

/*--------------------------------------------------------------------*/

const char *gridfile_get_contenttype(gridfile* gfile)

{
  bson_iterator it;

  if (bson_find(&it, gfile->meta, "contentType"))
    return bson_iterator_string( &it );
  else return NULL;
}

/*--------------------------------------------------------------------*/

bson_date_t gridfile_get_uploaddate(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "uploadDate");
  return bson_iterator_date( &it );
}

/*--------------------------------------------------------------------*/

const char* gridfile_get_md5(gridfile* gfile)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, "md5");
  return bson_iterator_string( &it );
}

/*--------------------------------------------------------------------*/

const char* gridfile_get_field(gridfile* gfile, const char* name)

{
  bson_iterator it;

  bson_find(&it, gfile->meta, name);
  return bson_iterator_value( &it );
}

/*--------------------------------------------------------------------*/

bson_bool_t gridfile_get_boolean(gridfile* gfile, const char* name)
{
  bson_iterator it;

  bson_find(&it, gfile->meta, name);
  return bson_iterator_bool( &it );
}

/*--------------------------------------------------------------------*/
bson gridfile_get_metadata(gridfile* gfile)

{
  bson sub;
  bson_iterator it;

  if (bson_find(&it, gfile->meta, "metadata")) {
    bson_iterator_subobject( &it, &sub );
    return sub;
  }
  else {
    bson_empty(&sub);
    return sub;
  }
}

/*--------------------------------------------------------------------*/

int gridfile_get_numchunks(gridfile* gfile)

{
  bson_iterator it;
  gridfs_offset length;
  gridfs_offset chunkSize;
  double numchunks;

  bson_find(&it, gfile->meta, "length");

  if( bson_iterator_type( &it ) == bson_int )
    length = (gridfs_offset)bson_iterator_int( &it );
  else
    length = (gridfs_offset)bson_iterator_long( &it );

  bson_find(&it, gfile->meta, "chunkSize");
  chunkSize = bson_iterator_int(&it);
  numchunks = ((double)length/(double)chunkSize);
  return (numchunks - (int)numchunks > 0)
    ? (int)(numchunks+1)
    : (int)(numchunks);
}

/*--------------------------------------------------------------------*/

bson gridfile_get_chunk(gridfile* gfile, int n)

{
  bson query;
  bson out;
  bson_buffer buf;
  bson_iterator it;
  bson_oid_t id;

  bson_buffer_init(&buf);
  bson_find(&it, gfile->meta, "_id");
  id = *bson_iterator_oid(&it);
  bson_append_oid(&buf, "files_id", &id);
  bson_append_int(&buf, "n", n);
  bson_from_buffer(&query, &buf);

  assert(mongo_find_one(gfile->gfs->client,
      gfile->gfs->chunks_ns,
      &query, NULL, &out));
  return out;
}

/*--------------------------------------------------------------------*/

mongo_cursor* gridfile_get_chunks(gridfile* gfile, int start, int size)

{
  bson_iterator it;
  bson_oid_t id;
  bson_buffer gte_buf;
  bson gte_bson;
  bson_buffer query_buf;
  bson query_bson;
  bson_buffer orderby_buf;
  bson orderby_bson;
  bson_buffer command_buf;
  bson command_bson;

  bson_find(&it, gfile->meta, "_id");
  id = *bson_iterator_oid(&it);

  bson_buffer_init(&query_buf);
  bson_append_oid(&query_buf, "files_id", &id);
  if (size == 1) {
    bson_append_int(&query_buf, "n", start);
  } else {
    bson_buffer_init(&gte_buf);
    bson_append_int(&gte_buf, "$gte", start);
    bson_from_buffer(&gte_bson, &gte_buf);
    bson_append_bson(&query_buf, "n", &gte_bson);
  }
  bson_from_buffer(&query_bson, &query_buf);

  bson_buffer_init(&orderby_buf);
  bson_append_int(&orderby_buf, "n", 1);
  bson_from_buffer(&orderby_bson, &orderby_buf);

  bson_buffer_init(&command_buf);
  bson_append_bson(&command_buf, "query", &query_bson);
  bson_append_bson(&command_buf, "orderby", &orderby_bson);
  bson_from_buffer(&command_bson, &command_buf);

  return mongo_find(gfile->gfs->client, gfile->gfs->chunks_ns,
        &command_bson, NULL, size, 0, 0);
}

/*--------------------------------------------------------------------*/

gridfs_offset gridfile_write_file(gridfile* gfile, FILE *stream)

{
  int i;
  size_t len;
  bson chunk;
  bson_iterator it;
  const char* data;
  const int num = gridfile_get_numchunks( gfile );

  for ( i=0; i<num; i++ ){
    chunk = gridfile_get_chunk( gfile, i );
    bson_find( &it, &chunk, "data" );
    len = bson_iterator_bin_len( &it );
    data = bson_iterator_bin_data( &it );
    fwrite( data , sizeof(char), len, stream );
  }

  return gridfile_get_contentlength(gfile);
}

/*--------------------------------------------------------------------*/

gridfs_offset gridfile_read(gridfile* gfile, gridfs_offset size, char* buf)

{
  mongo_cursor* chunks;
  bson chunk;

  int first_chunk;
  int last_chunk;
  int total_chunks;
  gridfs_offset chunksize;
  gridfs_offset contentlength;
  gridfs_offset bytes_left;
  int i;
  bson_iterator it;
  gridfs_offset chunk_len;
  const char * chunk_data;

  contentlength = gridfile_get_contentlength(gfile);
  chunksize = gridfile_get_chunksize(gfile);
  size = (contentlength - gfile->pos < size)
    ? contentlength - gfile->pos
    : size;
  bytes_left = size;

  first_chunk = (gfile->pos)/chunksize;
  last_chunk = (gfile->pos+size-1)/chunksize;
  total_chunks = last_chunk - first_chunk + 1;
  chunks = gridfile_get_chunks(gfile, first_chunk, total_chunks);

  for (i = 0; i < total_chunks; i++) {
    mongo_cursor_next(chunks);
    chunk = chunks->current;
    bson_find(&it, &chunk, "data");
    chunk_len = bson_iterator_bin_len( &it );
    chunk_data = bson_iterator_bin_data( &it );
    if (i == 0) {
      chunk_data += (gfile->pos)%chunksize;
      chunk_len -= (gfile->pos)%chunksize;
    }
    if (bytes_left > chunk_len) {
      memcpy(buf, chunk_data, chunk_len);
      bytes_left -= chunk_len;
      buf += chunk_len;
    } else {
      memcpy(buf, chunk_data, bytes_left);
    }
  }

  mongo_cursor_destroy(chunks);
  gfile->pos = gfile->pos + size;

  return size;
}

/*--------------------------------------------------------------------*/

gridfs_offset gridfile_seek(gridfile* gfile, gridfs_offset offset)

{
  gridfs_offset length;

  length = gridfile_get_contentlength(gfile);
  gfile->pos = length < offset ? length : offset;
  return gfile->pos;
}
