#ifndef BSON_TOOLS_COMMON_H_INCLUDED
#define BSON_TOOLS_COMMON_H_INCLUDED

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

enum { PRINT_TRACE = 0 };
#define TRACE(S, ...)                        \
   if (PRINT_TRACE) {                        \
      fprintf (stderr, S "\n", __VA_ARGS__); \
   } else                                    \
      ((void) (0))

typedef struct read_result {
   uint8_t *data;
   size_t len;
   int error;
} read_result;

static inline read_result
read_stream (FILE *strm)
{
   size_t buf_size = 0;
   uint8_t *data = NULL;
   size_t total_nread = 0;
   while (true) {
      // Calc how much is space is left in our buffer:
      const size_t buf_remain = buf_size - total_nread;
      if (buf_remain == 0) {
         // Increase the buffer size:
         buf_size += 1024;
         TRACE ("Increase buffer size to %zu bytes", buf_size);
         data = realloc (data, buf_size);
         if (!data) {
            fputs ("Failed to allocate a buffer for input\n", stderr);
            free (data);
            return (read_result){.error = ENOMEM};
         }
         // Try again
         continue;
      }
      // Set the output pointer to the beginning of the unread area:
      uint8_t *const ptr = data + total_nread;
      // Read some more
      TRACE ("Try to read %zu bytes", buf_remain);
      const size_t part_nread = fread (ptr, 1, buf_remain, stdin);
      TRACE ("Read %zu bytes", part_nread);
      if (part_nread == 0) {
         // EOF
         break;
      }
      total_nread += part_nread;
   }
   return (read_result){.data = data, .len = total_nread};
}

#endif // BSON_TOOLS_COMMON_H_INCLUDED
