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
  mongo_connection* client;
  /* The root database name */
  const char* dbname;
  /* The prefix of the GridFS's collections, default is NULL */
  const char* prefix;
  /* The namespace where the file's metadata is stored */
  const char* files_ns;
  /* The namespace where the files's data is stored in chunks */
  const char* chunks_ns;
} gridfs;

/* A GridFile contains the GridFS it is located in and the file 
   metadata */
typedef struct {
  /* The GridFS where the GridFile is located */
  gridfs* gfs;
  /* The GridFile's bson object where all it's metadata is located */
  bson* obj;
  /* The position is the offset in the file */
  size_t pos;
} gridfile;

/*--------------------------------------------------------------------*/


/** Initializes the GridFS object
 *  @param client - db connection
 *  @param dbname - database name
 *  @param prefix - collection prefix, default is fs if NULL or empty
 *  @param gfs - the GridFS object to intialize
 *  @return - 1 if successful, 0 otherwiseor 
 */
int gridfs_init(mongo_connection* client, const char* dbname, 
		const char* prefix, gridfs* gfs);

/** Destroys the GridFS object
 */
void gridfs_destroy(gridfs* gfs);

/** Puts the file reference by filename into the db
 *  @param gfs - the working GridFS
 *  @param data - pointer to buffer to store in GridFS
 *  @param length - length of the buffer
 *  @param remotename - filename for use in the database
 *  @param contenttype - optional MIME type for this object
 *  @return - the file object
 */
bson gridfs_store_buffer(gridfs* gfs, const char* data, size_t length, 
			 const char* remotename, 
			 const char * contenttype);

/** Puts the file reference by filename into the db
 *  @param gfs - the working GridFS
 *  @param filename - local filename relative to the process
 *  @param remotename - optional filename for use in the database
 *  @param contenttype - optional MIME type for this object
 *  @return - the file object
 */
bson gridfs_store_file(gridfs* gfs, const char* filename, 
		       const char* remotename, const char* contenttype);

/** Removes the files referenced by filename from the db
 *  @param gfs - the working GridFS
 *  @param filename - the filename of the file/s to be removed
 */
void gridfs_remove_filename(gridfs* gfs, const char* filename);

/** Find the first query within the GridFS and return it as a GridFile 
 *  @param gfs - the working GridFS
 *  @param query - a pointer to the bson with the query data
 *  @param gfile - the output GridFile to be initialized
 *  @return 1 if successful, 0 otherwise
 */
int gridfs_find_query(gridfs* gfs, bson* query, gridfile* gfile );

/** Find the first file referenced by filename within the GridFS 
 *  and return it as a GridFile 
 *  @param gfs - the working GridFS
 *  @param filename - filename of the file to find
 *  @param gfile - the output GridFile to be intialized
 *  @return 1 if successful, 0 otherwise
 */
int gridfs_find_filename(gridfs* gfs, const char *filename,
			      gridfile* gfile);

/*--------------------------------------------------------------------*/


/** Initializes a  GridFile containing the GridFS and file bson
 *  @param gfs - the GridFS where the GridFile is located
 *  @param obj - the file object
 *  @param gfile - the output GridFile that is being initialized
 *  @return 1 if successful, 0 otherwise
 */
int gridfile_init(gridfs* gfs, bson* obj, gridfile* gfile);

/** Destroys the GridFile 
 *  @param oGridFIle - the GridFile being destroyed
 */
void gridfile_destroy(gridfile* gfile);

/** Returns whether or not the GridFile exists
 *  @param gfile - the GridFile being examined
 */
int gridfile_exists(gridfile* gfile);

/** Returns the filename of GridFile
 *  @param gfile - the working GridFile
 *  @return - the filename of the Gridfile
 */
const char * gridfile_get_filename(gridfile* gfile);

/** Returns the size of the chunks of the GridFile
 *  @param gfile - the working GridFile
 *  @return - the size of the chunks of the Gridfile
 */
int gridfile_get_chunksize(gridfile* gfile);

/** Returns the length of GridFile's data
 *  @param gfile - the working GridFile
 *  @return - the length of the Gridfile's data
 */
gridfs_offset gridfile_get_contentlength(gridfile* gfile);

/** Returns the MIME type of the GridFile
 *  @param gfile - the working GridFile
 *  @return - the MIME type of the Gridfile
 *            (NULL if no type specified)
 */
const char* gridfile_get_contenttype(gridfile* gfile);

/** Returns the upload date of GridFile
 *  @param gfile - the working GridFile
 *  @return - the upload date of the Gridfile
 */
bson_date_t gridfile_get_uploaddate(gridfile* gfile);

/** Returns the MD5 of GridFile
 *  @param gfile - the working GridFile
 *  @return - the MD5 of the Gridfile
 */
const char* gridfile_get_md5(gridfile* gfile);

/** Returns the field in GridFile specified by name
 *  @param gfile - the working GridFile
 *  @param name - the name of the field to be returned
 *  @return - the data of the field specified
 *            (NULL if none exists)
 */
const char *gridfile_get_filefield(gridfile* gfile, 
				  const char* name);
/** Returns the metadata of GridFile
 *  @param gfile - the working GridFile
 *  @return - the metadata of the Gridfile in a bson object
 *            (an empty bson is returned if none exists)
 */
bson gridfile_get_metadata(gridfile* gfile);

/** Returns the number of chunks in the GridFile
 *  @param gfile - the working GridFile
 *  @return - the number of chunks in the Gridfile
 */
int gridfile_get_numchunks(gridfile* gfile);
  
/** Returns chunk n of GridFile
 *  @param gfile - the working GridFile
 *  @return - the nth chunk of the Gridfile
 */
bson gridfile_get_chunk(gridfile* gfile, int n);

/** Writes the GridFile to a stream 
 *  @param gfile - the working GridFile
 *  @param stream - the file stream to write to
 */
gridfs_offset gridfile_write_file(gridfile* gfile, FILE * stream);

/** Writes the GridFile to a buffer 
 *  (assumes the buffer is large enough)
 *  @param gfile - the working GridFile
 *  @param buf - the buffer to write to
 */
gridfs_offset gridfile_write_buffer(gridfile* gfile, char * buf);

/** Reads length bytes from the GridFile to a buffer 
 *  and updates the position in the file. 
 *  (assumes the buffer is large enough)
 *  (if size is greater than EOF gridfile_read reads until EOF)
 *  @param gfile - the working GridFile
 *  @param size - the amount of bytes to be read
 *  @param buf - the buffer to read to
 *  @return - the number of bytes read
 */
size_t gridfile_read(gridfile* gfile, size_t size, char* buf);

/** Updates the position in the file
 *  (If the offset goes beyond the contentlength,
 *  the position is updated to the end of the file.)
 *  @param gfile - the working GridFile
 *  @param offset - the position to update to
 *  @return - resulting offset location
 */
size_t gridfile_seek(gridfile* gfile, size_t offset);

#endif
