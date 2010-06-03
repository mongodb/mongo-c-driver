/*--------------------------------------------------------------------*/
/* gridfs.c                                                           */
/* Author: Christopher Triolo                                         */
/*--------------------------------------------------------------------*/

#include "gridfs.h"
#include "mongo.h"
#include "bson.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#define TRUE 1
#define FALSE 0


/*--------------------------------------------------------------------*/

static bson * Chunk_new(bson_oid_t id, int chunkNumber, 
		const char * data, int len)

{
  bson * b;
  bson_buffer buf;

  b = (bson *)malloc(sizeof(bson));
  if (b == NULL) return NULL;

  bson_buffer_init(&buf);
  bson_append_oid(&buf, "files_id", &id);
  bson_append_int(&buf, "n", chunkNumber);
  bson_append_binary(&buf, "data", 5, data, len);
  bson_from_buffer(b, &buf);
  return  b; 
}

/*--------------------------------------------------------------------*/

static void Chunk_free(bson * oChunk)

{
  bson_destroy(oChunk);
  free(oChunk);
}

/*--------------------------------------------------------------------*/

int GridFS_init(mongo_connection * client, const char * dbName, 
		const char * prefix, GridFS* oGridFS)
{  
  
  oGridFS->client = client;
  
  /* Allocate space to own the dbName */
  oGridFS->dbName = (char *)malloc(strlen(dbName)+1);
  if (oGridFS->dbName == NULL) {
    return FALSE;
  }
  strcpy(oGridFS->dbName, dbName);
  
  /* Allocate space to own the prefix */
  if (prefix == NULL) prefix == "fs";
  oGridFS->prefix = (char *)malloc(strlen(prefix)+1);
  if (oGridFS->prefix == NULL) {
    free(oGridFS->dbName);
    return FALSE;
  }
  strcpy(oGridFS->prefix, prefix);

  /* Allocate space to own filesNS */
  oGridFS->filesNS = 
    (char *) malloc (strlen(prefix)+strlen(dbName)+strlen(".files")+2);
  if (oGridFS->filesNS == NULL) {
    free(oGridFS->dbName);
    free(oGridFS->prefix);
    return FALSE;
  }
  strcpy(oGridFS->filesNS, dbName);
  strcat(oGridFS->filesNS, ".");
  strcat(oGridFS->filesNS, prefix);
  strcat(oGridFS->filesNS, ".files");
    
  /* Allocate space to own chunksNS */
  oGridFS->chunksNS = (char *) malloc(strlen(prefix) + strlen(dbName) 
				      + strlen(".chunks") + 2);
  if (oGridFS->chunksNS == NULL) {
    free(oGridFS->dbName);
    free(oGridFS->prefix);
    free(oGridFS->filesNS);
    return FALSE;
  }
  strcpy(oGridFS->chunksNS, dbName);
  strcat(oGridFS->chunksNS, ".");
  strcat(oGridFS->chunksNS, prefix);
  strcat(oGridFS->chunksNS, ".chunks");
  			      
  return TRUE;
}

/*--------------------------------------------------------------------*/

void GridFS_destroy(GridFS* oGridFS)

{
  if (oGridFS == NULL) return;
  if (oGridFS->dbName) free(oGridFS->dbName);
  if (oGridFS->prefix) free(oGridFS->prefix);
  if (oGridFS->filesNS) free(oGridFS->filesNS);
  if (oGridFS->chunksNS) free(oGridFS->chunksNS);
}

/*--------------------------------------------------------------------*/

static bson GridFS_insertFile( GridFS* oGridFS, const char* name, 
			       const bson_oid_t id, size_t length, 
			       const char* contentType)
{
  bson command;
  bson res;
  bson ret;
  bson_buffer buf;
  bson_iterator it;

  /* Check run md5 */
  bson_buffer_init(&buf);
  bson_append_oid(&buf, "filemd5", &id);
  bson_append_string(&buf, "root", oGridFS->prefix);
  bson_from_buffer(&command, &buf);
  assert(mongo_run_command(oGridFS->client, oGridFS->dbName, 
			   &command, &res));
  bson_destroy(&command);
 
  /* Create and insert BSON for file metadata */
  bson_buffer_init(&buf);
  bson_append_oid(&buf, "_id", &id);
  bson_append_string(&buf, "filename", name);
  bson_append_int(&buf, "length", length);
  bson_append_int(&buf, "chunkSize", DEFAULT_CHUNK_SIZE);
  bson_append_date(&buf, "uploadDate", (bson_date_t)1000*time(NULL));
  bson_find(&it, &res, "md5");
  bson_append_string(&buf, "md5", bson_iterator_string(&it));
  bson_destroy(&res);
  if (contentType != NULL && strlen(contentType) != 0) {
    bson_append_string(&buf, "contentType", contentType);
  }
  bson_from_buffer(&ret, &buf);
  mongo_insert(oGridFS->client, oGridFS->filesNS, &ret);

  return ret;
}

/*--------------------------------------------------------------------*/

bson GridFS_storeFileFromBuffer( GridFS* oGridFS, const char* data, 
				size_t length, const char* remoteName, 
				const char * contentType)

{
  char const * const end = data + length;
  
  /* Generate and append an oid*/
  bson_oid_t id;
  bson_oid_gen(&id);
  
  /* Insert the file chunk by chunk */
  int chunkNumber = 0;
  while (data < end) {
    int chunkLen = DEFAULT_CHUNK_SIZE < (unsigned int)(end-data) ?
      DEFAULT_CHUNK_SIZE : (unsigned int)(end-data);
    bson * oChunk =  Chunk_new( id, chunkNumber, data, chunkLen );
    mongo_insert(oGridFS->client, oGridFS->chunksNS, oChunk);
    Chunk_free(oChunk);
    chunkNumber++;
    data += chunkLen;
  }
  
  if (remoteName == NULL || strlen(remoteName)==0) 
    remoteName = "untitled";

  return GridFS_insertFile(oGridFS, remoteName, id, length, 
			   contentType);
}

/*--------------------------------------------------------------------*/

bson GridFS_storeFileFromFileName( GridFS* oGridFS, 
				  const char* fileName, 
				  const char* remoteName, 
				  const char* contentType)
{
  char buffer[DEFAULT_CHUNK_SIZE];

  /* Open the file and the correct stream */
  FILE * fd;
  if (strcmp(fileName, "-") == 0) fd = stdin;
  else fd = fopen(fileName, "rb");
  assert(fd != NULL); // No such file

  /* Generate and append an oid*/
  bson_oid_t id;
  bson_oid_gen(&id);
  
  /* Insert the file chunk by chunk */
  int chunkNumber = 0;
  size_t length = 0;
  size_t chunkLen = 0;
  do {
    chunkLen = fread(buffer, 1, DEFAULT_CHUNK_SIZE, fd);
    bson *oChunk =  Chunk_new( id, chunkNumber, buffer, chunkLen );
    mongo_insert(oGridFS->client, oGridFS->chunksNS, oChunk);
    Chunk_free(oChunk);
    length += chunkLen;
    chunkNumber++;
  } while (chunkLen == DEFAULT_CHUNK_SIZE);

  if (fd != stdin) fclose(fd);

  // Large files assertion?
  
  if (remoteName == NULL || strlen(remoteName)==0) {
    remoteName = fileName; }

  return GridFS_insertFile(oGridFS, remoteName, id, length, 
			   contentType);
}

/*--------------------------------------------------------------------*/

void GridFS_removeFile(GridFS* oGridFS, const char* fileName )

{
  bson query;
  bson_buffer buf;
  bson_buffer_init(&buf);
  bson_append_string(&buf, "filename", fileName);
  bson_from_buffer(&query, &buf);
  mongo_cursor* files = mongo_find(oGridFS->client,  oGridFS->filesNS,
				   &query, NULL, 0, 0, 0);
  bson_destroy(&query);

  while (mongo_cursor_next(files)) {
    bson file = files->current;
    bson_iterator it;
    bson_find(&it, &file, "_id");
    bson_oid_t id = *bson_iterator_oid(&it);
    
    bson_buffer_init(&buf);
    bson_append_oid(&buf, "_id", &id);
    bson b;
    bson_from_buffer(&b, &buf);
    mongo_remove( oGridFS->client, oGridFS->filesNS, &b);
    bson_destroy(&b);

    bson_buffer_init(&buf);
    bson_append_oid(&buf, "files_id", &id);
    bson_from_buffer(&b, &buf);
    mongo_remove( oGridFS->client, oGridFS->chunksNS, &b);
    bson_destroy(&b);
  }

}

/*--------------------------------------------------------------------*/

int GridFS_findFileByQuery(GridFS* oGridFS, bson* query, 
			   GridFile* oGridFile )

{
  bson_buffer date_buffer;
  bson_buffer_init(&date_buffer);
  bson_append_int(&date_buffer, "uploadDate", -1);
  
  bson uploadDate;
  bson_from_buffer(&uploadDate, &date_buffer);
  
  bson_buffer buf;
  bson_buffer_init(&buf);
  bson_append_bson(&buf, "query", query);
  bson_append_bson(&buf, "orderby", &uploadDate);
  
  bson finalQuery;
  bson_from_buffer(&finalQuery, &buf);

  bson out;
  int i = (mongo_find_one(oGridFS->client, oGridFS->filesNS, 
			   &finalQuery, NULL, &out));
  bson_destroy(&uploadDate);
  bson_destroy(&finalQuery);

  if (!i)
    return FALSE;
  else {
    GridFile_init(oGridFS, &out, oGridFile);
    //bson_destroy(&out);
    return TRUE;
  }
}

/*--------------------------------------------------------------------*/

int GridFS_findFileByFileName(GridFS* oGridFS, 
			      const char* fileName,
			      GridFile* oGridFile)

{
  bson query;
  bson_buffer buf;
  bson_buffer_init(&buf);
  bson_append_string(&buf, "filename", fileName);
  bson_from_buffer(&query, &buf) ;
  int i = GridFS_findFileByQuery(oGridFS, &query, oGridFile);
  bson_destroy(&query);
  return i;
}

/*--------------------------------------------------------------------*/

int GridFile_init(GridFS* oGridFS, bson* obj, GridFile* oGridFile)

{
  oGridFile->oGridFS = oGridFS;
  oGridFile->obj = (bson*)malloc(sizeof(bson));
  if (oGridFile->obj == NULL) return FALSE;
  bson_copy(oGridFile->obj, obj);
  return TRUE;
}

/*--------------------------------------------------------------------*/

void GridFile_destroy(GridFile* oGridFile)

{
  bson_destroy(oGridFile->obj);
  free(oGridFile->obj);
}

/*--------------------------------------------------------------------*/

bson_bool_t GridFile_exists(GridFile* oGridFile)

{
  return (bson_bool_t)(oGridFile != NULL || oGridFile->obj == NULL);
}

/*--------------------------------------------------------------------*/

const char *GridFile_getFilename(GridFile* oGridFile)

{
  bson_iterator it;
  bson_find(&it, oGridFile->obj, "filename");
  return bson_iterator_string(&it);
}

/*--------------------------------------------------------------------*/

int GridFile_getChunkSize(GridFile* oGridFile)

{
  bson_iterator it;
  bson_find(&it, oGridFile->obj, "chunkSize");
  return bson_iterator_int(&it);
}

/*--------------------------------------------------------------------*/

gridfs_offset GridFile_getContentLength(GridFile* oGridFile)

{  
  bson_iterator it;
  bson_find(&it, oGridFile->obj, "length");
  return (gridfs_offset)bson_iterator_int( &it );
}

/*--------------------------------------------------------------------*/

const char *GridFile_getContentType(GridFile* oGridFile)

{  
  bson_iterator it;
  if (bson_find(&it, oGridFile->obj, "contentType"))
    return bson_iterator_string( &it );
  else return NULL;
}

/*--------------------------------------------------------------------*/

bson_date_t GridFile_getUploadDate(GridFile* oGridFile)

{  
  bson_iterator it;
  bson_find(&it, oGridFile->obj, "uploadDate");
  return bson_iterator_date( &it );
}

/*--------------------------------------------------------------------*/

const char *GridFile_getMD5(GridFile* oGridFile)

{  
  bson_iterator it;
  bson_find(&it, oGridFile->obj, "md5");
  return bson_iterator_string( &it );
}

/*--------------------------------------------------------------------*/

const char *GridFile_getFileField(GridFile* oGridFile, const char* name)

{  
  bson_iterator it;
  bson_find(&it, oGridFile->obj, name);
  return bson_iterator_value( &it ); 
}

/*--------------------------------------------------------------------*/
bson GridFile_getMetaData(GridFile* oGridFile)

{
  bson sub;
  bson_iterator it;
  if (bson_find(&it, oGridFile->obj, "metadata")) {
    bson_iterator_subobject( &it, &sub ); 
    return sub;
  }
  else {
    bson_empty(&sub);
    return sub;
  }
}

/*--------------------------------------------------------------------*/

int GridFile_getNumChunks(GridFile* oGridFile)
  
{
  bson_iterator it;
  bson_find(&it, oGridFile->obj, "length");
  size_t length = bson_iterator_int(&it); 
  bson_find(&it, oGridFile->obj, "chunkSize");
  size_t chunkSize = bson_iterator_int(&it);
  return ceil((double)length/(double)chunkSize);
}

/*--------------------------------------------------------------------*/

bson GridFile_getChunk(GridFile* oGridFile, int n)

{
  bson query;
  bson out;
  bson_buffer buf;
  bson_iterator it;

  bson_buffer_init(&buf);
  bson_find(&it, oGridFile->obj, "_id");
  bson_oid_t id = *bson_iterator_oid(&it); 
  bson_append_oid(&buf, "files_id", &id);
  bson_append_int(&buf, "n", n);
  bson_from_buffer(&query, &buf);
  
  assert(mongo_find_one(oGridFile->oGridFS->client, 
			oGridFile->oGridFS->chunksNS,
			&query, NULL, &out));
  return out;
}

/*--------------------------------------------------------------------*/

gridfs_offset GridFile_write(GridFile* oGridFile, FILE *stream)

{
  const int num = GridFile_getNumChunks( oGridFile );
  
  for ( int i=0; i<num; i++ ){
    bson chunk = GridFile_getChunk( oGridFile, i );
    bson_iterator it;
    bson_find( &it, &chunk, "data" );
    int len = bson_iterator_bin_len( &it );
    const char * data = bson_iterator_bin_data( &it );
    fwrite( data , sizeof(char), len, stream );
  }

  return GridFile_getContentLength(oGridFile);
}

/*--------------------------------------------------------------------*/

gridfs_offset GridFile_writeToBuf(GridFile* oGridFile, void * buf)

{
  const int num = GridFile_getNumChunks( oGridFile );
 
  for ( int i=0; i<num; i++ ){
    bson chunk = GridFile_getChunk( oGridFile, i );
    bson_iterator it;
    bson_find( &it, &chunk, "data" );
    size_t len = bson_iterator_bin_len( &it );
    const char * data =  bson_iterator_bin_data( &it );
    memcpy( buf, data, len);
    buf += len;
  }
  
  return GridFile_getContentLength(oGridFile);
}
 
/*--------------------------------------------------------------------*/
