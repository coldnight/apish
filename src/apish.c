#include "request.h"
#include "command.h"
#include <stdio.h>

    int
main(void)
{
    /* request_init(); */
    /* RequestContainer *rc; */

    /* rc = create_request_container(HTTP_SCHEME, "localhost", 8089); */
    /* add_request(rc, "/i/login", POST); */
    /* add_header(rc, "/i/login", POST, "nice", "tooo"); */
    /* add_query(rc, "/i/login", POST, "token", "to&o"); */
    /* add_request(rc, "/i/signup", POST); */
    /* add_query(rc, "/i/signup", POST, "token", "to&o"); */
    /* create_request_container(HTTP_SCHEME, "103.30.41.9", 8089); */
    /* request_dump(); */
    /* request_cleanup(); */

    request_load();
    request_init();
    command_init();
    command_loop();
    request_dump();
    command_cleanup();
    request_cleanup();
    return 0;
}
