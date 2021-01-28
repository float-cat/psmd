/*
 ============================================================================
 Name        : client.h
 Author      : float.cat
 Version     : 0.31
 Description : Заголовок асинхронного клиент-клиента с самодеспетчиризацией
 ============================================================================
 */

#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Почему не определен MISC? */
#ifndef __USE_MISC
 #define __USE_MISC
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define IPADDR_LOCALHOST      (0x7F000001)
#define DISPATCHER_PORT             (7800)
#define DEFAULT_NETADDRSCOUNT         (10)
#define INVALID_DISTANCE      (0xFFFFFFFF)
#define NUMBER_SLOTS                   (8)
#define INVALID_SLOT          (0xFFFFFFFF)
#define FREE_SLOT             INVALID_SLOT

/* Для наглядного отличия хранимых и отправляемых
    адресов от адресов записаных в сетевом порядке */
typedef in_addr_t addr_data_t;

struct unit_t
{
    int socket;
    unsigned int distance;
};

struct unit_node_t
{
    struct unit_t unit;
    struct unit_node_t *next;
};

struct dispatcher_t
{
    /* Список клиентов для диспетчерезации */
    struct unit_node_t *units;
    /* Множество дескрипторов для асинхронного чтения */
    fd_set fds;
    /* Адрес диспетчеризуемой сети */
    addr_data_t netaddr;
    /* Дескриптор сокета для отправки по UDP */
    int socketUDP;
    /* Дескрипторы сокетов для приема */
    int sdTCP;
    int sdUDP;
    /* Верхняя граница множества для асинхронного чтения -
        старший дескриптор + 1 для select */
    int topsock;
    /* Барьер синхронизации для одновременного старта */
    pthread_barrier_t starter;
    /* Дескрипторы нитей */
    pthread_t thrdUDP;
    pthread_t thrdTCP;
    pthread_t thrdacceptor;
    /* Мьютекс для работы со списком */
    pthread_mutex_t listlocker;
};

/* Состояние протокола */
struct protocol_state_t
{
    /* Идентификатор состояния протокола */
    unsigned int state;
    /* Атрибут состояния, например, количество соседей,
        подключение которых ожидает клиент */
    unsigned int attr;
};

/* Данные о сети, к которой подключен компьютер клиента */
struct net_data_t
{
    addr_data_t ipaddr;
    addr_data_t netmask;
    addr_data_t broadaddr;
};

/* Статус слотов */
enum
{
    /* Слот свободен для подключения */
    SLOT_STATUS_FREE,
    /* Слот занят, но полезная нагрузка протокола не
        обрабатывается, клиент получает необходимые
        настройки от диспетчера и клиента, который дал слот */
    SLOT_STATUS_PREPARE,
    /* Слот занят и обрабатывает полезную
        нагрузку протокола */
    SLOT_STATUS_READY
};

struct slot_t
{
    int socket;
    int status;
    addr_data_t ipaddr;
    unsigned short port;
};

struct client_t
{
    /* Дескрипторы сокетов и статус соединения для клиент-клиент */
    struct slot_t slots[NUMBER_SLOTS];
    /* Множество дескрипторов для взаимодействия с соседями */
    fd_set fds;
    /* Граница для обработки множества дескрипторов select'ом */
    int topsock;
    /* Диспетчер, если ноль, то клиент не занимается диспетчеризацией
        подключений других клиентов */
    struct dispatcher_t *dispatcher;
    /* Сокеты слушатель для регистрации соединений TCP */
    int listenerTCP;
    /* барьер синхронизации для одновременного старта */
    pthread_barrier_t starter;
    /* Нити для работы с TCP */
    pthread_t thrddialog;
    pthread_t thrdTCP, thrdacceptor;
    /* Сокет для отправки сообщений по UDP и TCP диспетчеру */
    int sockUDP, sockTCP;
    /* Адрес компьютера в диспетчеризуемой сети */
    addr_data_t ipaddr;
    /* Случайно выбранный порт */
    unsigned short portTCP;
    /* Широковещательный адрес, маска сетей и айпи-адрес компьютера в сетях,
        к которым подключен компьютер */
    struct net_data_t *netsdata;
    unsigned char netscount;
    /* Состояние протокола */
    struct protocol_state_t state;
    /* Удаление от диспетчера используется для раскручивания построения
        сперва будут принимать те, кто ближе к центру */
    unsigned int distance;
};

/* Создает клиент и возвращает
    указатель на структуру-описатель */
struct client_t *client_create
(
    void
);

/* Уничтожает клиент, закрывая
    все ранее открытые соединения */
void client_destroy
(
    struct client_t *client
);

/* Ищет айпи-адрес, соответствующий
    адресу подсети */
addr_data_t client_get_ipaddr_by_netaddr
(
    struct client_t *client,
    addr_data_t netmask
);

/* Ищет адрес подсети, соответствующий
    айпи-адресу */
addr_data_t client_get_netaddr_by_ipaddr
(
    struct client_t *client,
    addr_data_t ipaddr
);

/* Ждет ответ от диспетчера в течении 50мс
    если ответ пришел - возвращает адрес
    отправителя ответа, иначе 0 */
addr_data_t client_wait_dispatcher_discover_anwer
(
    struct client_t *client
);

/* Инициализирует клиенту функции диспетчера */
void client_dispatchering_init
(
    struct client_t *client
);

/* Освобождает клиент от функций диспетчера */
void client_dispatcher_release
(
    struct client_t *client
);

/* Опрашивает ioctl для получения данных о
    сетевых интерфейсов, возвращает указатель
    на массив данных, по указателю count возвращает
    количество элементов в массиве данных  */
struct net_data_t *client_prepare_netsdata
(
    unsigned char *count
);

/* Обработка подключения в слот нового клиента */
/* Проверяет наличие свободного слота у клиента,
    возвращает свободный слот, если такой имеется,
    или некорректный слот в ином случае */
unsigned int client_has_free_slot
(
    struct client_t *client
);

/* Выполняет подключение клиента к клиенту */
void client_connect_to_client
(
    struct client_t *client,
    unsigned int slotid,
    addr_data_t ipaddr,
    unsigned short port
);

/* Определяет старший из дескрипторов сокетов соседей,
    возвращает его, увеличив на 1, для select, O(n),
    используется только для поиска меньшего чем был,
    иначе - переназначается за O(1) */
int client_gettopsock
(
    struct client_t *client
);

/* Безопасное занятие слота */
void client_use_slot
(
    struct client_t *client,
    unsigned int slotid,
    int socket,
    addr_data_t ipaddr,
    unsigned short port
);

/* Безопасное освобождение слота */
void client_release_slot
(
    struct client_t *client,
    unsigned int slotid
);

/* Взаимная замена двух слотов для
    перемещения соединения по границе */
void client_slots_swap(
    struct client_t *client,
    unsigned int slotid,
    unsigned int newslotid
);

/* Перевод слота в режим готовности
    приема полезной нагрузки */
void client_slot_ready
(
    struct client_t *client,
    unsigned int slotid
);

/* Список участников диспетчеризации */
/* Добавление к списку */
void client_dispatcher_add_unit
(
    struct client_t *client,
    int socket
);

/* Удаление из списка */
void client_dispatcher_remove_unit
(
    struct client_t *client,
    int socket
);

/* Операции над элементами списка диспетчера */
/* Поиск элемента по заданному дескриптору сокета, вернет
    указатель на предыдущий элемент или ничего, если не найдено */
struct unit_node_t *client_dispatcher_prev_unit
(
    struct dispatcher_t *dispatcher,
    int socket
);

/* Возвращает границу дескрипторов для select, O(n), 
    нужна только при поиске меньшего чем был, в другом
    случае используется переназначение за O(1) */
int client_dispatcher_gettopsock
(
    struct dispatcher_t *dispatcher
);

/* Обработчики входящих сообщений для диспетчера */
/* Обработчик UDP для диспетчера, в качестве аргумента
    получает указатель на структуру клиента */
void *client_dispatcher_udp_handler
(
    void *arg
);

/* TCP приемщик диспетчера, в качестве аргумента
    получает указатель на структуру клиента */
void *client_dispatcher_tcp_acceptor
(
    void *arg
);

/* Обработчик TCP для диспетчера, в качестве аргумента
    получает указатель на структуру клиента */
void *client_dispatcher_tcp_handler
(
    void *arg
);

/* Обработчики входящих сообщений для клиента */
/* Обработчик TCP для диалога с диспетчером, в качестве аргумента
    получает указатель на структуру клиента */
void *client_tcp_dialog
(
    void *arg
);

/* TCP приемщик для соседей клиента, в качестве аргумента
    получает указатель на структуру клиента */
void *client_tcp_acceptor
(
    void *arg
);

/* Обработчик TCP для соседей клиента, в качестве аргумента
    получает указатель на структуру клиента */
void *client_tcp_handler
(
    void *arg
);

#define PROTO_PRINT printf

#endif /* ifndef CLIENT_H */
