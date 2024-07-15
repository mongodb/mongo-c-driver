#include <mongoc/mongoc.h>
#include <stdio.h>

int
main (int argc, char *argv[])
{
    const char *certificate_path;
    const char *ca_path;
    mongoc_uri_t *uri;
    mongoc_client_t *client;
    mongoc_database_t *database;
    bool r;

    mongoc_init ();

    certificate_path = "/Users/julia.garland/Desktop/Code/drivers-evergreen-tools/.evergreen/x509gen/server.pem";
    ca_path = "/Users/julia.garland/Desktop/Code/drivers-evergreen-tools/.evergreen/x509gen/ca.pem";

    // Make URI to create all clients from
    uri = mongoc_uri_new ("mongodb://localhost:27017/");
    mongoc_uri_set_option_as_bool (uri, MONGOC_URI_TLS, true);

    mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCERTIFICATEKEYFILE, certificate_path);
    mongoc_uri_set_option_as_utf8 (uri, MONGOC_URI_TLSCAFILE, ca_path);

    // Create the client
    client = mongoc_client_new_from_uri (uri);

    if (!client) {
        printf("Client failed to initialize.\n");
    }

    mongoc_client_set_error_api (client, 2);
    database = mongoc_client_get_database (client, "test");

    for (int i = 0; i < 10000; i++) {
        bson_t ping;
        bson_t reply;
        bson_error_t error;

        // Send a ping to the server
        bson_init (&ping);
        bson_append_int32 (&ping, "ping", 4, 1);

        r = mongoc_database_command_with_opts (database, &ping, NULL, NULL, &reply, &error);

        if (r) {
            //fprintf (stdout, "Success on client %d\n", i);
        } else {
            fprintf (stderr, "Ping failure on client %d: %s\n", i, error.message);
        }

        // Cleanup
        bson_destroy (&ping);
        bson_destroy (&reply);
    }
    mongoc_uri_destroy (uri);
    mongoc_database_destroy (database);
    mongoc_client_destroy (client);
    mongoc_cleanup();
}
