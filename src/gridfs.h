/*--------------------------------------------------------------------*/
/* gridfs.h                                                           */
/* Author: Christopher Triolo                                         */
/*--------------------------------------------------------------------*/

#include "mongo.h"
#include "bson.h"
#include <stdio.h>

#ifndef GRIDFS_INCLUDED
#define GRIDFS_INCLUDED

enum {DEFAULT_CHUNK_SIZE = 256 * 1024};

typedef unsigned long long gridfs_offset;

/* A GridFS contains a db connections, a root database name, and a 
   optional prefix */
typedef struct {
  /* The client to db-connection. */
  mongo_connection * client;
  /* The root database name */
  char * dbName;
  /* The prefix of the GridFS's collections, default is NULL */
  char * prefix;
  /* The namespace where the file's metadata is stored */
  char * filesNS;
  /* The namespace where the files's data is stored in chunks */
  char * chunksNS;
} GridFS;

/* A GridFile contains the GridFS it is located in and the file 
   metadata */
typedef struct {
  /* The GridFS where the GridFile is located */
  GridFS *oGridFS;
  /* The GridFile's bson object where all it's metadata is located */
  bson *obj;
} GridFile;

/*--------------------------------------------------------------------*/


/** Initializes the GridFS object
 *  @param client - db connection
 *  @param dbname - database name
 *  @param prefix - collection prefix, default is fs if NULL or empty
 *  @param oGridFS - the GridFS object to intialize
 *  @return - 1 if successful, 0 otherwiseor 
 */
int GridFS_init(mongo_connection* client, const char* dbName, 
		const char* prefix, GridFS* oGridFS);

/** Destroys the GridFS object
 */
void GridFS_destroy(GridFS* oGridFS);

/** Puts the file reference by fileName into the db
 *  @param oGridFS - the working GridFS
 *  @param data - pointer to buffer to store in GridFS
 *  @param length - length of the buffer
 *  @param remoteName - filename for use in the database
 *  @param contentType - optional MIME type for this object
 *  @return - the file object
 */
bson GridFS_storeFileFromBuffer( GridFS* oGridFS, const char* data, 
				size_t length, 
				const char* remoteName, 
				const char * contentType);

/** Puts the file reference by fileName into the db
 *  @param oGridFS - the working GridFS
 *  @param fileName - local filename relative to the process
 *  @param remoteName - optional filename for use in the database
 *  @param contentType - optional MIME type for this object
 *  @return - the file object
 */
bson GridFS_storeFileFromFileName( GridFS* oGridFS, 
				  const char* fileName, 
				  const char* remoteName, 
				  const char* contentType);

/** Removes the files referenced by fileName from the db
 *  @param oGridFS - the working GridFS
 *  @param fileName - the filename of the file/s to be removed
 */
void GridFS_removeFile(GridFS* oGridFS, const char* fileName);

/** Find the first query within the GridFS and return it as a GridFile 
 *  @param oGridFS - the working GridFS
 *  @param query - a pointer to the bson with the query data
 *  @param oGridFile - the output GridFile to be initialized
 *  @return 1 if successful, 0 otherwise
 */
int GridFS_findFileByQuery(GridFS* oGridFS, bson * query, 
			   GridFile* oGridFile );

/** Find the first file referenced by fileName within the GridFS 
 *  and return it as a GridFile 
 *  @param oGridFS - the working GridFS
 *  @param fileName - fileName of the file to find
 *  @param oGridFile - the output GridFile to be intialized
 *  @return 1 if successful, 0 otherwise
 */
int GridFS_findFileByFileName(GridFS* oGridFS, const char *fileName,
			      GridFile* oGridFile);

/*--------------------------------------------------------------------*/


/** Initializes a  GridFile containing the GridFS and file bson
 *  @param oGridFS - the GridFS where the GridFile is located
 *  @param obj - the file object
 *  @param oGridFile - the output GridFile that is being initialized
 *  @return 1 if successful, 0 otherwise
 */
int GridFile_init(GridFS* oGridFS, bson* obj, GridFile* oGridFile);

/** Destroys the GridFile 
 *  @param oGridFIle - the GridFile being destroyed
 */
void GridFile_destroy(GridFile* oGridFile);

/** Returns whether or not the GridFile exists
 *  @param oGridFile - the GridFile being examined
 */
int GridFile_exists(GridFile* oGridFile);

/** Returns the filename of GridFile
 *  @param oGridFile - the working GridFile
 *  @return - the filename of the Gridfile
 */
const char * GridFile_getFilename(GridFile* oGridFile);

/** Returns the size of the chunks of the GridFile
 *  @param oGridFile - the working GridFile
 *  @return - the size of the chunks of the Gridfile
 */
int GridFile_getChunkSize(GridFile* oGridFile);

/** Returns the length of GridFile's data
 *  @param oGridFile - the working GridFile
 *  @return - the length of the Gridfile's data
 */
gridfs_offset GridFile_getContentLength(GridFile* oGridFile);

/** Returns the MIME type of the GridFile
 *  @param oGridFile - the working GridFile
 *  @return - the MIME type of the Gridfile
 *            (NULL if no type specified)
 */
const char *GridFile_getContentType(GridFile* oGridFile);

/** Returns the upload date of GridFile
 *  @param oGridFile - the working GridFile
 *  @return - the upload date of the Gridfile
 */
bson_date_t GridFile_getUploadDate(GridFile* oGridFile);

/** Returns the MD5 of GridFile
 *  @param oGridFile - the working GridFile
 *  @return - the MD5 of the Gridfile
 */
const char *GridFile_getMD5(GridFile* oGridFile);

/** Returns the field in GridFile specified by name
 *  @param oGridFile - the working GridFile
 *  @param name - the name of the field to be returned
 *  @return - the data of the field specified
 *            (NULL if none exists)
 */
const char *GridFile_getFileField(GridFile* oGridFile, 
				  const char* name);
/** Returns the metadata of GridFile
 *  @param oGridFile - the working GridFile
 *  @return - the metadata of the Gridfile in a bson object
 *            (an empty bson is returned if none exists)
 */
bson GridFile_getMetaData(GridFile* oGridFile);

/** Returns the number of chunks in the GridFile
 *  @param oGridFile - the working GridFile
 *  @return - the number of chunks in the Gridfile
 */
int GridFile_getNumChunks(GridFile* oGridFile);
  
/** Returns chunk n of GridFile
 *  @param oGridFile - the working GridFile
 *  @return - the nth chunk of the Gridfile
 */
bson GridFile_getChunk(GridFile* oGridFile, int n);

/** Writes the GridFile to a stream 
 *  @param oGridFile - the working GridFile
 *  @param stream - the file stream to write to
 */
gridfs_offset GridFile_write(GridFile* oGridFile, FILE * stream);

/** Writes the GridFile to a buffer 
 *  (assumes the buffer is large enough)
 *  @param oGridFile - the working GridFile
 *  @param buf - the buffer to write to
 */
gridfs_offset GridFile_writeToBuf(GridFile* oGridFile, void * buf);

#endif
