/*
 * Test for request_load
 */
#include "request.h"

    int
main(void)
{
    request_init();
    request_load("test_1.json");
    request_dump("test_1.obj");
    request_cleanup();
    return 0;
}
