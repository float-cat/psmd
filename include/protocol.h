/*
 ============================================================================
 Name        : protocol.h
 Author      : float.cat
 Version     : 0.31
 Description : Заголовочный файл протокола PSMD Protocol Scale Matrix Deploy
 ============================================================================
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "client.h"

/* Коды широковещательных сообщений UDP */
#define DISPATCHER_DISCOVER        (0x49444944) /*DIDI*/
#define DISPATCHER_IM              (0x4D494944) /*DIIM*/

/* Размер буфера отправки и приема */
#define TCP_MSG_SIZE                       (32)

/* Сериализация данных для передачи */
#ifndef MSG_SERIALIZE
# define MSG_SERIALIZE(var, type, buffer, length) \
    memcpy(buffer+length, &var, sizeof(type)); \
    length += sizeof(type)
# define MSG_DESERIALIZE(var, type, buffer, length) \
    memcpy(&var, buffer+length, sizeof(type)); \
    length += sizeof(type)
#endif /* MSG_SERIALIZE */

/* Установка соответствия для противоположных позиций */
/* Пользуемся тем, что постоянная зависимость между
    противоположными сторонами - циклическая разница в 4 единицы */
#define OPPOSITE_POSITION(position) \
    ((position + 4) % 8)

/* Определение занятого и готового слота и добавление
    его в массив */
#define CHECK_SLOT_TO_READY(client_ptr, slotname, slotnameactual, cnt, cntact, number) \
    if(client_ptr->slots[slotname].status == SLOT_STATUS_READY) \
    { \
        cnt[number] = slotname; \
        cntact[number++] = slotnameactual; \
    }

/* Идентификаторы направлений */
enum
{
    /* Идентификаторы расположены по часовой стрелке,
        постоянная зависимость между противоположными сторонами -
        циклическая разница в 4 единицы */
    NEIGBOR_RIGHT, NEIGBOR_BOTTOM_RIGHT,
    NEIGBOR_BOTTOM, NEIGBOR_BOTTOM_LEFT,
    NEIGBOR_LEFT, NEIGBOR_TOP_LEFT,
    NEIGBOR_TOP, NEIGBOR_TOP_RIGHT
};

/* Состояния протокола */
enum
{
    PROTOCOL_STARTED,
    WAIT_PLACE,
    PLACE_SELECTED,
    WAIT_ALL_NEIGHBOR,
    IN_PROCESS
};

/* Коды сообщений TCP с типами параметров */
enum
{
    /* Ответ на подключение к диспетчеру */
    DISPATCHER_CONFIRM, /* u32bit */
    /* Поиск слота */
    PLACE_DISCOVER, /* u32bit, u16bit, u32bit */
    PLACE_CONFIRM, /* без параметров */
    PLACE_REFUSE, /* без параметров */
    PLACE_ANCHOR, /* u8bit, u32bit */
    /* Подключение */
    CONNECTION_HANDSNAKE, /* u8bit */
    CONNECTION_BORDER, /* u8bit */
    CONNECTION_NEIGHBOR, /* u32bit, u16bit, u8bit */
    CONNECTION_READY, /* без параметров */
    CONNECTION_DISTANCE /* u32bit */
};

/* Задаем тип кода сообщения для TCP */
typedef unsigned int msg_code_t;

/* Датаграмма UDP состоит только из 4-х байтного кода */
typedef unsigned int dg_code_t; 

/* Вспомогательные функции */
/* Общая процедура отправки датаграммы */
void dg_send
(
    int socket,
    in_addr_t broadaddr,
    dg_code_t code
);

/* Функция вовращает размер данных сообщения,
    отправляемого по TCP (без кода сообщения) */
size_t size_of_msg_tcp_data
(
    msg_code_t code
);

/* Общая процедура отправки сообщения в поток TCP */
void msg_send
(
    int socket,
    char *msg,
    size_t msgsize
);

/* Функция определяет идентификаторы слотов соседей переданного
    слота, и возвращает в аргумент массив идентификаторов,
    возвращает количество соседей - как результат функции */
unsigned int get_neighbors_by_slot
(
    struct client_t *client,
    unsigned int slotid,
    unsigned int *slotids,
    unsigned int *actslotids
);

/* Часть прикладного протокола, надстроенная над UDP */
/* Широковещательный поиск диспетчера в сети
    (DISPATCHER_DISCOVER) */
void dg_dispatcher_discover
( /* Отправка */
    struct client_t *client
);

void on_dispatcher_discover
( /* Прием */
    struct client_t *client,
    in_addr_t ipaddr
);

/* Диспетчер сообщает о своем присутствии и передает адрес
    сети, в которой выполняет диспетчеризацию
    (DISPATCHER_IM, адрес сети) */
void dg_dispatcher_im
( /* Отправка */
    struct client_t *client,
    in_addr_t ipaddr
);

/* Часть прикладного протокола, надстроенная над TCP */
/* Сообщение о готовности диспетчера
    (DISPATCHER_CONFIRM) */
void msg_dispatcher_confirm
( /* Отправка */
    struct client_t *client,
    int socket
);

void on_dispatcher_confirm
( /* Прием */
    struct client_t *client,
    char *msg,
    size_t msgsize
);

/* Поиск незанятого слота
    (PLACE_DISCOVER, адрес, порт, удаление от диспетчера) */
void msg_place_discover
( /* Отправка */
    struct client_t *client,
    int socket,
    in_addr_t ipaddr,
    unsigned short port,
    unsigned int distance
);

void relay_place_discover
( /* Прием:Диспетчер */
    struct client_t *client,
    char *msg,
    size_t msgsize
);

void on_place_discover
( /* Прием:Клиент */
    struct client_t *client,
    char *msg,
    size_t msgsize
);

/* Сообщение параметров места в распределении новому клиенту
    (CONNECTION_ANCHOR, расположение передающего
        относительно получателя, удаление от диспетчера) */
void msg_place_anchor
( /* Отправка */
    struct client_t *client,
    int socket,
    unsigned char position,
    unsigned int distance
);

void on_place_anchor
( /* Прием */
    struct client_t *client,
    unsigned int slotid,
    char *msg,
    size_t msgsize
);

/* Подтверждение получения места в распределении
    (PLACE_CONFIRM) */
void msg_place_confirm
( /* Отправка */
    struct client_t *client,
    int socket
);

void on_place_confirm
( /* Прием */
    struct client_t *client,
    unsigned int slotid
);

/* Отказ от получения места в распределении
    (PLACE_REFUSE) */
void msg_place_refuse
( /* Отправка */
    struct client_t *client,
    int socket
);

void on_place_refuse
( /* Прием */
    struct client_t *client,
    unsigned int slotid
);

/* Рукопожатие с новыми соседями с целью совместить границы рамок
    (CONNECTION_HANDSNAKE, расположение передающего относительно получателя) */
void msg_connection_handsnake
( /* Отправка */
    struct client_t *client,
    unsigned char position
);

void on_connection_handsnake
( /* Прием */
    struct client_t *client,
    unsigned int slotid,
    unsigned char position
);

/* Информация о величине рамки, чтобы у всех была одинаковая
    рамка, так же передает кол-во соседей, перед отправкой их данных
    (CONNECTION_BORDER, высота, ширина, количество активных соседей) */
void msg_connection_border
( /* Отправка */
    struct client_t *client,
    int socket,
    unsigned char neighborcount
);

void on_connection_border
( /* Прием */
    struct client_t *client,
    unsigned int slotid,
    char *msg,
    size_t msgsize
);

/* Пересылка данных о общем соседе
    (CONNECTION_NEIGHBOR, адрес, порт,
        расположение соседа относительно получателя) */
void msg_connection_neighbor
( /* Отправка */
    struct client_t *client,
    int socket,
    in_addr_t ipaddr,
    unsigned short port,
    unsigned char position
);

void on_connection_neighbor
( /* Прием */
    struct client_t *client,
    unsigned int slotid,
    char *msg,
    size_t msgsize
);

/* Сообщение о готовности принимать сообщения полезной нагрузки
    (CONNECTION_READY) */
void msg_connection_ready
( /* Отправка */
    struct client_t *client,
    int socket
);

void on_connection_ready
( /* Прием */
    struct client_t *client,
    unsigned int slotid
);

/* Сообщение о готовности обслуживать поиск слотов
    (CONNECTION_DISTANCE, удаление от диспетчера) */
void msg_connection_distance
( /* Отправка */
    struct client_t *client,
    unsigned int distance
);

void on_connection_distance
( /* Прием */
    struct client_t *client,
    struct unit_t *unit,
    char *msg,
    size_t msgsize
);

/* Обработчики протокола */
/* Обработка UDP сообщений от клиентов к диспетчеру */
void dg_dispatcher_udp_handler
(
    struct client_t *client,
    in_addr_t ipaddr,
    dg_code_t code
);

/* Обработка подключений клиентов к диспетчеру */
void msg_dispatcher_tcp_acceptor
(
    struct client_t *client,
    int sock
);

/* Обработка TCP сообщений от клиентов к диспетчеру */
void msg_dispatcher_tcp_handler
(
    struct client_t *client,
    struct unit_t *unit,
    msg_code_t code,
    char *buffer,
    size_t msgsize
);

/* Обработка TCP сообщений от диспетчера к клиенту */
void msg_tcp_dialog
(
    struct client_t *client,
    msg_code_t code,
    char *buffer,
    size_t msgsize
);

/* Обработка подключений клиентов к клиенту, возвращает
    свободный слот или, если нет свободного, некорректный слот,
    если уже есть соединение с клиентом, но не послан READY,
    то тоже возвращает некорректный слот */
int msg_tcp_acceptor
(
    struct client_t *client,
    int socket
);

/* Обработчик TCP сообщений от клиентов к клиенту */
void msg_tcp_handler
(
    struct client_t *client,
    unsigned int slotid,
    msg_code_t code,
    char *buffer,
    size_t msgsize
);

/* Специальные события */
/* Сообщает всем ранее подсоединившимся клиентам адрес сети,
    полученный от клиента вне локальной сети */
void on_netaddr_setup
(
    struct client_t *client
);

#endif /* ifndef PROTOCOL_H */
