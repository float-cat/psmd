#include "include/client.h"

int main(void)
{
    /* Тестовая главная процедура 
        ждет 50 секунд перед отключением */
    struct client_t *client;
    client = client_create();
    sleep(50);
    client_destroy(client);
    return EXIT_SUCCESS;
}
