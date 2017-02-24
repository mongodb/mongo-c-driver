// Copyright 2017 MongoDB Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <mongoc.h>

int
main (int argc, const char **argv)
{
   mongoc_client_t *client;
   const char *uri_str;

   mongoc_init ();

   client = mongoc_client_new ("mongodb://hello-mongoc.org");
   uri_str = mongoc_uri_get_string (mongoc_client_get_uri (client));
   printf ("%s !\n", uri_str);

   mongoc_client_destroy (client);
   mongoc_cleanup ();

   return 0;
}
