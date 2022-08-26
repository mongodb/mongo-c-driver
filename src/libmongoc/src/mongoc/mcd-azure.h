#ifndef MCD_AZURE_H_INCLUDED
#define MCD_AZURE_H_INCLUDED

#include <mongoc/mongoc.h>

#include <mongoc/mongoc-http-private.h>

/**
 * @brief An Azure OAuth2 access token obtained from the Azure API
 */
typedef struct mcd_azure_access_token {
   /// The access token string
   char *access_token;
   /// The resource of the token (the Azure resource for which it is valid)
   char *resource;
   /// The HTTP type of the token
   char *token_type;
} mcd_azure_access_token;

/**
 * @brief Try to parse an Azure access token from an IMDS metadata JSON response
 *
 * @param out The token to initialize. Should be uninitialized.
 * @param json The JSON string body
 * @param len The length of 'body'
 * @param error An output parameter for errors
 * @return true If 'out' was successfully initialized to a token.
 * @return false Otherwise
 *
 * @note The 'out' token must later be given to @ref mc_azure_auth_token_free
 */
bool
mcd_azure_access_token_try_init_from_json_str (mcd_azure_access_token *out,
                                               const char *json,
                                               int len,
                                               bson_error_t *error);

/**
 * @brief Destroy an access token struct
 *
 * @param c The access token to destroy
 */
void
mcd_azure_access_token_destroy (mcd_azure_access_token *token);

/**
 * @brief An Azure IMDS HTTP request
 *
 */
typedef struct mcd_azure_imds_request {
   /// The underlying HTTP request object to be sent
   mongoc_http_request_t req;
} mcd_azure_imds_request;

/**
 * @brief Initialize a new IMDS HTTP request
 *
 * @param out The object to initialize
 *
 * @note the request must later be destroyed with mcd_azure_imds_request_destroy
 */
void
mcd_azure_imds_request_init (mcd_azure_imds_request *out);

/**
 * @brief Destroy an IMDS request created with mcd_azure_imds_request_init()
 *
 * @param req
 */
void
mcd_azure_imds_request_destroy (mcd_azure_imds_request *req);


#endif // MCD_AZURE_H_INCLUDED
