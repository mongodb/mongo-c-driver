#include <mongoc.h>

typedef enum ssl_test_state {
   SSL_TEST_CRASH,
   SSL_TEST_SUCCESS,
   SSL_TEST_SSL_INIT,
   SSL_TEST_SSL_HANDSHAKE,
   SSL_TEST_SSL_VERIFY,
   SSL_TEST_TIMEOUT,
} ssl_test_state_t;

typedef struct ssl_test_result {
   ssl_test_state_t result;
   int err;
   unsigned long ssl_err;
} ssl_test_result_t;

void
ssl_test (mongoc_ssl_opt_t  *client,
          mongoc_ssl_opt_t  *server,
          const char        *host,
          ssl_test_result_t *client_result,
          ssl_test_result_t *server_result);
