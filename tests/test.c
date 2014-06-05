#include "apish.h"
#include "request.h"
#include "command.h"
#include <stdio.h>

int global_colored = -1;

    int
main(void)
{
    request_init();
    RequestContainer *rc;
    Request *req;

    rc = create_request_container(HTTP_SCHEME, "localhost", 8089);
    add_request(rc, "/i/login", POST);
    req = find_request(rc, "/i/login", POST);
    add_header(req, "nice", "tooo");
    add_query(req, "token", "to&o");
    add_request(rc, "/i/signup", POST);

    req = find_request(rc, "/i/signup", POST);
    add_query(req, "token", "to&o");
    create_request_container(HTTP_SCHEME, "103.30.41.9", 8089);
    request_dump("test.json");
    request_cleanup();
    printf("Test Done!\n");
    return 0;
}
