/*
 ============================================================================
 Name        : protocol.c
 Author      : float.cat
 Version     : 0.31
 Description : Файл реализациии протокола PSMD Protocol Scale Matrix Deploy
 ============================================================================
 */

#ifndef PROTOCOL_C
#define PROTOCOL_C

#include "protocol.h"

/* Вспомогательные функции */
/* Общая процедура отправки датаграммы */
void dg_send(int sock, in_addr_t broadaddr, dg_code_t code)
{
    struct sockaddr_in sa;
    struct in_addr dbg; /* для PROTO_PRINT */
    dbg.s_addr = broadaddr; /* для PROTO_PRINT */
    PROTO_PRINT("basic: dg_send(sock=%d, broadaddr=[%s], code=%d)\n",
        sock, inet_ntoa(dbg), code);
    /* Заполняем структуру адреса */
    sa.sin_family = PF_INET;
    sa.sin_addr.s_addr = broadaddr;
    sa.sin_port = htons(DISPATCHER_PORT);
    /* Отправляем сообщение по указанному адресу */
    sendto(sock, &code, sizeof(dg_code_t), 0, (struct sockaddr *)&sa,
        sizeof(struct sockaddr_in));
}

/* Функция вовращает размер данных сообщения,
    отправляемого по TCP (без кода сообщения) */
size_t size_of_msg_tcp_data(msg_code_t code)
{
    PROTO_PRINT("basic: size_of_msg_tcp_data(%d)\n", code);
    switch(code)
    {
        case DISPATCHER_CONFIRM:
            return sizeof(in_addr_t);
        case PLACE_DISCOVER:
            return sizeof(in_addr_t) +
                sizeof(unsigned short) +
                sizeof(unsigned int);
        case CONNECTION_READY:
        case PLACE_CONFIRM:
        case PLACE_REFUSE:
            return 0;
        case PLACE_ANCHOR:
            return sizeof(unsigned char) +
                sizeof(unsigned int);
        case CONNECTION_HANDSNAKE:
            return sizeof(unsigned char);
        case CONNECTION_BORDER:
            return sizeof(unsigned char);
        case CONNECTION_NEIGHBOR:
            return sizeof(unsigned int) +
                sizeof(unsigned short) +
                sizeof(unsigned char);
        case CONNECTION_DISTANCE:
            return sizeof(unsigned int);
    }
    return 0;
}

/* Общая процедура отправки сообщения в поток TCP */
void msg_send(int socket, char *msg, size_t msgsize)
{
    /* В Linux блокирующие сокеты отправляют все данные целиком,
        однако, для поддержки Unix необходимо проверять все ли было
        отправлено, и отправлять остатки, если нужно */
    size_t bytes = 0, check;
    do 
    {
        /* Отправляем данные по соединению */
        check = send(socket, msg+bytes, msgsize-bytes, 0);
        /* Проверяем все ли в порядке */
        if(check == 0)
            /* Если нет - уходим */
            break;
        /* Наращиваем общее кол-во байт отправленным только что */
        bytes += check;
        PROTO_PRINT("tcp: send(bytes=%ld, check=%ld, msgsize=%ld)\n", bytes, check, msgsize);
    } /* Отправка продолжается, пока не отправим все */
    while(bytes < msgsize);
}

/* Функция определяет идентификаторы общих соседей
    этого клиента и его соседа с идентификтором slotid */
unsigned int get_neighbors_by_slot(struct client_t *client,
    unsigned int slotid, unsigned int *slotids, unsigned int *actslotids)
{
    unsigned int count = 0;
    /* Для каждого идентификатора просто проверим возможных
        соседей - у углов 2 возможных, у середин 4,
        передаем два набора соответствий сразу, чтобы не писать
        лишний в данном случае алгоритм. Первое соответствие -
        соседи клиента, который обрабатывает протокол, второе -
        соседи адресата */
    switch(slotid)
    {
        case NEIGBOR_TOP_LEFT:
            CHECK_SLOT_TO_READY(client, NEIGBOR_TOP, NEIGBOR_RIGHT,
                                    slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_LEFT, NEIGBOR_BOTTOM,
                                     slotids, actslotids, count);
            break;
        case NEIGBOR_TOP_RIGHT:
            CHECK_SLOT_TO_READY(client, NEIGBOR_TOP, NEIGBOR_LEFT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_RIGHT, NEIGBOR_BOTTOM,
                                     slotids, actslotids, count);
            break;
        case NEIGBOR_BOTTOM_LEFT:
            CHECK_SLOT_TO_READY(client, NEIGBOR_BOTTOM, NEIGBOR_RIGHT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_LEFT, NEIGBOR_TOP,
                                     slotids, actslotids, count);
            break;
        case NEIGBOR_BOTTOM_RIGHT:
            CHECK_SLOT_TO_READY(client, NEIGBOR_BOTTOM, NEIGBOR_LEFT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_RIGHT, NEIGBOR_TOP,
                                     slotids, actslotids, count);
            break;
        case NEIGBOR_TOP:
            CHECK_SLOT_TO_READY(client, NEIGBOR_TOP_LEFT, NEIGBOR_LEFT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_LEFT, NEIGBOR_BOTTOM_LEFT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_TOP_RIGHT, NEIGBOR_RIGHT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_RIGHT, NEIGBOR_BOTTOM_RIGHT,
                                     slotids, actslotids, count);
            break;
        case NEIGBOR_BOTTOM:
            CHECK_SLOT_TO_READY(client, NEIGBOR_BOTTOM_LEFT, NEIGBOR_LEFT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_LEFT, NEIGBOR_TOP_LEFT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_BOTTOM_RIGHT, NEIGBOR_RIGHT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_RIGHT, NEIGBOR_TOP_RIGHT,
                                     slotids, actslotids, count);
            break;
        case NEIGBOR_RIGHT:
            CHECK_SLOT_TO_READY(client, NEIGBOR_TOP_RIGHT, NEIGBOR_TOP,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_TOP, NEIGBOR_TOP_LEFT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_BOTTOM_RIGHT, NEIGBOR_BOTTOM,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_BOTTOM, NEIGBOR_BOTTOM_LEFT,
                                     slotids, actslotids, count);
            break;
        case NEIGBOR_LEFT:
            CHECK_SLOT_TO_READY(client, NEIGBOR_TOP_LEFT, NEIGBOR_TOP,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_TOP, NEIGBOR_TOP_RIGHT,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_BOTTOM_LEFT, NEIGBOR_BOTTOM,
                                     slotids, actslotids, count);
            CHECK_SLOT_TO_READY(client, NEIGBOR_BOTTOM, NEIGBOR_BOTTOM_RIGHT,
                                     slotids, actslotids, count);
            break;
    }
    return count;
}

/* Часть прикладного протокола, надстроенная над UDP */
/* Широковещательный поиск диспетчера в сети */
void dg_dispatcher_discover(struct client_t *client)
{ /* Отправка */
    dg_code_t code;
    PROTO_PRINT("call: dg_dispatcher_discover(%p)\n", (void *)client);
    code = DISPATCHER_DISCOVER;
    dg_send(client->sockUDP, client->netsdata[0].broadaddr, code);
}

void on_dispatcher_discover(struct client_t *client, in_addr_t ipaddr)
{ /* Прием */
    PROTO_PRINT("catch: on_dispatcher_discover(%p, %d)\n", (void *)client, ipaddr);
    /* Только диспетчер обрабатывает UDP постоянно в данном протоколе */
    /* Отвечаем на DISPATCHER_DISCOVER сообщением DISPATCHER_IM */
    dg_dispatcher_im(client, ipaddr);
}

/* Диспетчер сообщает о своем присутствии и передает адрес
    сети, в которой выполняет диспетчеризацию */
void dg_dispatcher_im(struct client_t *client, in_addr_t ipaddr)
{ /* Отправка */
    dg_code_t code;
    PROTO_PRINT("call: dg_dispatcher_im(%p)\n", (void *)client);
    code = DISPATCHER_IM;
    dg_send(client->dispatcher->socketUDP, ipaddr, code);
}

/* Часть прикладного протокола, надстроенная над TCP */
/* Сообщение о готовности диспетчера */
void msg_dispatcher_confirm(struct client_t *client, int socket)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = DISPATCHER_CONFIRM;
    PROTO_PRINT("call: msg_dispatcher_confirm(%p, %d)\n", (void *)client, socket);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    MSG_SERIALIZE(client->dispatcher->netaddr, in_addr_t, msg, msgsize);
    /* Посылаем сообщение */
    msg_send(socket, msg, msgsize);
}

void on_dispatcher_confirm(struct client_t *client, char *msg, size_t msgsize)
{ /* Прием */
    in_addr_t netaddr;
    MSG_DESERIALIZE(netaddr, in_addr_t, msg, msgsize);
    PROTO_PRINT("catch: on_dispatcher_confirm(%p, %d)\n", (void *)client, netaddr);
    /* На основе адреса внешней сети, в пределах которой будут выполнятся
        соединения между компьютерами, выбираем и запоминаем адрес нашего
        клиента в этой сети */
    client->ipaddr = netaddr ?
        client_get_ipaddr_by_netaddr(client, netaddr) : IPADDR_LOCALHOST;
    /* Получив подтверждение DISPATCHER_CONFIRM от диспетчера,
        посылаем ему сообщение PLACE_DISCOVER */
    if(client->state.state == PROTOCOL_STARTED && client->distance)
    {
        client->state.state = WAIT_PLACE;
        PROTO_PRINT("call: msg_place_discover(%p)\n", (void *)client);
        msg_place_discover(client, client->sockTCP, client->ipaddr,
            client->portTCP, 0);
    }
}

/* Поиск незанятого слота */
void msg_place_discover(struct client_t *client, int socket,
    in_addr_t ipaddr, unsigned short port, unsigned int distance)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = PLACE_DISCOVER;
    PROTO_PRINT("call: msg_place_discover(%p, %d, %d, %d)\n", (void *)client, ipaddr, port, distance);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    MSG_SERIALIZE(ipaddr, in_addr_t, msg, msgsize);
    MSG_SERIALIZE(port, unsigned short, msg, msgsize);
    MSG_SERIALIZE(distance, unsigned int, msg, msgsize);
    /* Посылаем сообщение */
    msg_send(socket, msg, msgsize);
}

void relay_place_discover(struct client_t *client, char *msg, size_t msgsize)
{ /* Прием:Диспетчер */
    struct unit_node_t *ptr;
    in_addr_t ipaddr;
    unsigned short port;
    unsigned int distance;
    PROTO_PRINT("catch: relay_place_discover(%p)\n", (void *)client);
    /* Дополнительная проверка на то, вызвана ли процедура
        после инициализации диспетчера */
    if(client != NULL && client->dispatcher != NULL)
    {
        /* Забираем значение дистанции из сообщения */
        MSG_DESERIALIZE(ipaddr, in_addr_t, msg, msgsize);
        MSG_DESERIALIZE(port, unsigned short, msg, msgsize);
        MSG_DESERIALIZE(distance, unsigned int, msg, msgsize);
        /* Диспетчер пересылает сообщение всем клиентам, чье расстояние
            соответствует указанному в сообщении. */
        ptr = client->dispatcher->units;
        while(ptr!=NULL)
        {
            if(ptr->unit.distance == distance)
                msg_place_discover(client, ptr->unit.socket, ipaddr, port, 0);
            ptr = ptr->next;
        }
    }
}

void on_place_discover(struct client_t *client, char *msg, size_t msgsize)
{ /* Прием:Клиент */
    unsigned int slotid;
    in_addr_t ipaddr;
    unsigned short port;
    unsigned int distance;
    PROTO_PRINT("catch: on_place_discover(%p)\n", (void *)client);
    MSG_DESERIALIZE(ipaddr, in_addr_t, msg, msgsize);
    MSG_DESERIALIZE(port, unsigned short, msg, msgsize);
    MSG_DESERIALIZE(distance, unsigned int, msg, msgsize);
    if(client->distance == distance &&
        (slotid = client_has_free_slot(client)) != INVALID_SLOT)
        client_connect_to_client(client, slotid, ipaddr, port);
}

/* Сообщение параметров места в распределении новому клиенту */
void msg_place_anchor(struct client_t *client, int socket,
    unsigned char position, unsigned int distance)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = PLACE_ANCHOR;
    PROTO_PRINT("call: msg_place_anchor(%p, %d, %d, %d)\n", (void *)client, socket, position, distance);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    MSG_SERIALIZE(position, unsigned char, msg, msgsize);
    MSG_SERIALIZE(distance, unsigned int, msg, msgsize);
    /* Посылаем сообщение */
    msg_send(socket, msg, msgsize);
}

void on_place_anchor(struct client_t *client, unsigned int slotid,
    char *msg, size_t msgsize)
{ /* Прием */
    unsigned char newslotid;
    unsigned int distance;
    PROTO_PRINT("catch: on_place_anchor(%p, slotid:%d, %p, %ld)\n", (void *)client, slotid, (void *)msg, msgsize);
    MSG_DESERIALIZE(newslotid, unsigned char, msg, msgsize);
    MSG_DESERIALIZE(distance, unsigned int, msg, msgsize);
    PROTO_PRINT("\tattr: newslotid=[%d], distance=[%d]\n", newslotid, distance);
    client->distance = distance;
    client_slots_swap(client, slotid, newslotid);
}

/* Подтверждение получения места в распределении */
void msg_place_confirm(struct client_t *client, int socket)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = PLACE_CONFIRM;
    PROTO_PRINT("call: msg_place_confirm(%p, socket:%d)\n", (void *)client, socket);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    /* Посылаем сообщение */
    msg_send(socket, msg, msgsize);
}

void on_place_confirm(struct client_t *client, unsigned int slotid)
{ /* Прием */
    unsigned int i;
    unsigned int count;
    /* Сюда записываем соседей с точки зрения этой стороны */
    unsigned int slotids[NUMBER_SLOTS];
    /* Здесь соседи с точки зрения той стороны соединения */
    unsigned int actslotids[NUMBER_SLOTS];
    struct slot_t *slot, *neighbor;
    PROTO_PRINT("catch: on_place_confirm(%p, slotid:%d)\n", (void *)client, slotid);
    /* Если протокол ждет подключения всех соседей */
    if(client->state.state == WAIT_ALL_NEIGHBOR)
    {
        /* Уменьшаем количество соседей */
        client->state.attr--;
        /* Если этот сосед последний */
        if(!client->state.attr)
        {
            /* Переключаем состояние протокола в последнюю фазу
                соединение со всеми необходимыми участниками
                установлено, новые участники будут приниматься
                через обработчик новых соединений или через
                on_place_discover в зависимости от ситуации */
            client->state.state = IN_PROCESS;
            /* Ставим указатель на первый слот */
            slot = client->slots;
            for(i=0; i<NUMBER_SLOTS; i++, slot++)
            {
                if(slot->status == SLOT_STATUS_PREPARE)
                {
                    client_slot_ready(client, i);
                    msg_connection_ready(client, slot->socket);
                }
            }
            /* С этого момента клиент готов работать
                с полезной нагрузкой */
            return;
        }
    }
    /* Если протокол не в состоянии IN_PROCESS - уходим */
    if(client->state.state != IN_PROCESS)
        return;

    /* Смещаемся на указатель конкретного слота */
    slot = client->slots+slotid;
    /* Ассоциируем слот с портом отправителя (из сообщения) */
    /* Отправляем новому клиенту его позицию относительно принявшего,
        а также сообщаем ему, что его удаление на единицу больше,
        чем у принявшего клиента */
    msg_place_anchor(client, slot->socket,
        OPPOSITE_POSITION(slotid), client->distance+1);
    /* Отправляем характеристики границ отображения (холста),
        также отправляем количество соседей (исключая себя) */
    msg_connection_border(client, slot->socket,
        count = get_neighbors_by_slot(client, slotid, slotids, actslotids));
    PROTO_PRINT("\t neighbors:");
    for(i=0; i<count; i++)
        PROTO_PRINT(" [%d]", slotids[i]);
    PROTO_PRINT("\n");
    /* Сообщаем отправителю реквизиты его соседей в распределении */
    for(i=0; i<count; i++)
    {
        /* Смещаемся на указатель i-того соседа */
        neighbor = client->slots+slotids[i];
        /* Передаем его реквизиты отправителю */
        msg_connection_neighbor(client, slot->socket,
            neighbor->ipaddr, neighbor->port, actslotids[i]);
    }
}

/* Отказ от получения места в распределении */
void msg_place_refuse(struct client_t *client, int socket)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = PLACE_REFUSE;
    PROTO_PRINT("call: msg_place_refuse(%p, socket:%d)\n", (void *)client, socket);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    /* Посылаем сообщение */
    msg_send(socket, msg, msgsize);
}

void on_place_refuse(struct client_t *client, unsigned int slotid)
{ /* Прием */
    PROTO_PRINT("catch: on_place_refuse(%p, slotid:%d)\n", (void *)client, slotid);
    client_release_slot(client, slotid);
}

/* Рукопожатие с новыми соседями с целью
    совместить границы рамок матрицы распределения */
void msg_connection_handsnake(struct client_t *client, unsigned char position)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = CONNECTION_HANDSNAKE;
    PROTO_PRINT("call: msg_connection_handsnake(%p, %d)\n", (void *)client, position);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    MSG_SERIALIZE(position, unsigned char, msg, msgsize);
    /* Посылаем сообщение */
    msg_send(client->sockTCP, msg, msgsize);
}

void on_connection_handsnake(struct client_t *client, unsigned int slotid, unsigned char position)
{ /* Прием */
    PROTO_PRINT("catch: on_connection_handsnake(%p, %d, %d)\n", (void *)client, slotid, position);
    /* Перемещаем слот согласно указаниям из рукопожатия */
    client_slots_swap(client, slotid, position);
}

/* Функция передает кол-во соседей, перед отправкой их данных */
void msg_connection_border(struct client_t *client, int socket,
    unsigned char neighborcount)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = CONNECTION_BORDER;
    PROTO_PRINT("call: msg_connection_border(%p, %d)\n", (void *)client, neighborcount);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    MSG_SERIALIZE(neighborcount, unsigned char, msg, msgsize);
    /* Посылаем сообщение */
    msg_send(socket, msg, msgsize);
}

void on_connection_border(struct client_t *client, unsigned int slotid,
    char *msg, size_t msgsize)
{ /* Прием */
    unsigned char neighborcount;
    struct slot_t *slot;
    /* Смещаемся на указатель конкретного слота */
    slot = client->slots+slotid;
    MSG_DESERIALIZE(neighborcount, unsigned char, msg, msgsize);
    PROTO_PRINT("catch: on_connection_border(%p, slotid:%d, neighborcount:%d)\n",
        (void *)client, slotid, neighborcount);
    if(client->state.state == PLACE_SELECTED)
    {
        if(neighborcount > 0)
        {
            client->state.state = WAIT_ALL_NEIGHBOR;
            client->state.attr = neighborcount;
        }
        else
        {
            client->state.state = IN_PROCESS;
            client_slot_ready(client, slotid);
            msg_connection_ready(client, slot->socket);
        }
    }
}

/* Пересылка данных о общем соседе */
void msg_connection_neighbor(struct client_t *client, int socket,
    in_addr_t ipaddr, unsigned short port, unsigned char position)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = CONNECTION_NEIGHBOR;
    PROTO_PRINT("call: msg_connection_neighbor(%p, %d, %d, %d)\n", (void *)client, ipaddr, port, position);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    MSG_SERIALIZE(ipaddr, in_addr_t, msg, msgsize);
    MSG_SERIALIZE(port, unsigned short, msg, msgsize);
    MSG_SERIALIZE(position, unsigned char, msg, msgsize);
    /* Посылаем сообщение */
    msg_send(socket, msg, msgsize);
}

void on_connection_neighbor(struct client_t *client, unsigned int slotid,
    char *msg, size_t msgsize)
{ /* Прием */
    in_addr_t ipaddr;
    unsigned short port;
    unsigned char position;
    PROTO_PRINT("catch: on_connection_neighbor(%p, slotid:%d)\n", (void *)client, slotid);
    /* Получаем атрибуты сообщения */
    MSG_DESERIALIZE(ipaddr, in_addr_t, msg, msgsize);
    MSG_DESERIALIZE(port, unsigned short, msg, msgsize);
    MSG_DESERIALIZE(position, unsigned char, msg, msgsize);
    PROTO_PRINT("\t attr: ipaddr=%d, port=%d, position=%d\n", ipaddr, port, position);
    /* Выполняем подключение */
    client_connect_to_client(client, position, ipaddr, port);
    /* Отправляем рукопожатие с целью передать позицию себя относительно
        адресата (т.е. оппозицию) */
    msg_connection_handsnake(client, OPPOSITE_POSITION(position));
}

/* Сообщение о готовности принимать
    сообщения полезной нагрузки */
void msg_connection_ready(struct client_t *client, int socket)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = CONNECTION_READY;
    PROTO_PRINT("call: msg_connection_ready(%p)\n", (void *)client);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    /* Посылаем сообщение */
    msg_send(socket, msg, msgsize);
}

void on_connection_ready(struct client_t *client, unsigned int slotid)
{ /* Прием */
    PROTO_PRINT("catch: on_connection_ready(%p, %d)\n", (void *)client, slotid);
    client_slot_ready(client, slotid);
}

/* Сообщение о готовности обслуживать поиск слотов  */
void msg_connection_distance(struct client_t *client,
    unsigned int distance)
{ /* Отправка */
    char msg[TCP_MSG_SIZE];
    size_t msgsize = 0;
    msg_code_t code = CONNECTION_DISTANCE;
    PROTO_PRINT("call: msg_connection_distance(%p, %d)\n", (void *)client, distance);
    /* Формируем сообщение */
    MSG_SERIALIZE(code, msg_code_t, msg, msgsize);
    MSG_SERIALIZE(distance, unsigned int, msg, msgsize);
    /* Если перед нами диспетчер распределения, то он уже готов к работе */
    if(!distance)
        client->state.state = IN_PROCESS;
    /* Посылаем сообщение */
    msg_send(client->sockTCP, msg, msgsize);
}

void on_connection_distance(struct client_t *client, struct unit_t *unit,
    char *msg, size_t msgsize)
{ /* Прием */
    unsigned int distance;
    PROTO_PRINT("catch: on_connection_distance(%p, %p)\n", (void *)client, (void *)unit);
    MSG_DESERIALIZE(distance, unsigned int, msg, msgsize);
    PROTO_PRINT("\tattr: distance=[%d]\n", distance);
    unit->distance = distance;
}

/* Обработка UDP сообщений от клиентов к диспетчеру */
void dg_dispatcher_udp_handler(struct client_t *client, in_addr_t ipaddr,
    dg_code_t code)
{
    PROTO_PRINT("catch: dg_dispatcher_udp_handler(%p, %d)\n",
        (void *)client, code);
    if(code == DISPATCHER_IM)
        on_dispatcher_discover(client, ipaddr);
}

/* Обработка подключений клиентов к диспетчеру */
void msg_dispatcher_tcp_acceptor(struct client_t *client, int socket)
{
    PROTO_PRINT("catch: msg_dispatcher_tcp_acceptor(%p, socket:%d)\n",
        (void *)client, socket);
    msg_dispatcher_confirm(client, socket);
}

/* Обработка TCP сообщений от клиентов к диспетчеру */
void msg_dispatcher_tcp_handler(struct client_t *client,
    struct unit_t *unit, msg_code_t code, char *msg, size_t msgsize)
{
    PROTO_PRINT("catch: msg_dispatcher_tcp_handler(%p, code:%d, %p, size:%ld)\n",
        (void *)client, code, msg, msgsize);
    switch(code)
    {
        case PLACE_DISCOVER:
            /* Это сообщение получил диспетчер - рассылаем его клиентам */
            relay_place_discover(client, msg, msgsize);
            break;
        case CONNECTION_DISTANCE:
            on_connection_distance(client, unit, msg, msgsize);
            break;
    }
}

/* Обработка TCP сообщений от диспетчера к клиенту */
void msg_tcp_dialog(struct client_t *client, msg_code_t code,
    char *msg, size_t msgsize)
{
    PROTO_PRINT("catch: msg_tcp_dialog(%p, code:%d, %p, size:%ld)\n",
        (void *)client, code, msg, msgsize);
    switch(code)
    {
        case DISPATCHER_CONFIRM:
            on_dispatcher_confirm(client, msg, msgsize);
            break;
        case PLACE_DISCOVER:
            /* Это сообщение получил клиент */
            on_place_discover(client, msg, msgsize);
            break;
    }
}

/* Обработка подключений клиентов к клиенту, возвращает
    свободный слот или, если нет свободного, некорректный слот,
    если уже есть соединение с клиентом, но не послан READY,
    то тоже возвращает некорректный слот */
int msg_tcp_acceptor(struct client_t *client, int socket)
{
    unsigned int slotid = INVALID_SLOT;
    PROTO_PRINT("catch: msg_tcp_acceptor(%p, socket:%d)\n",
        (void *)client, socket);
    /* Принимаем соединения только если протокол находится
        в состоянии сборки распределения клиент-клиент (WAIT_#) или
        в состоянии готовности */
    if(client->state.state == WAIT_PLACE ||
        client->state.state == IN_PROCESS)
            /* Получаем идентификатор свободного слота */
            slotid = client_has_free_slot(client);
    /* Если у нас состояние WAIT_PLACE - переходим в состояние PLACE_SELECTED */
    if(client->state.state == WAIT_PLACE)
        client->state.state = PLACE_SELECTED;
    /* В любых других состояниях поиск свободных слотов не производится,
        а в переменной slotid останется идентификатор некорректного слота,
        показывая тем самым, что мы не готовы к соединению,
        кроме того, идентификатор некорректного слота будет возвращен, если
        свободного слота не найдено */
    /* Если слот некорректный посылаем отказ от соединения, иначе
        соглашаемся на соединение */
    if(slotid == INVALID_SLOT)
        msg_place_refuse(client, socket);
    else
        msg_place_confirm(client, socket);
    /* Возвращаем в обработчик клиента идентификатор слота, для операций
        по занятию слота и добавлению идентификатора во множество */
    return slotid;
}

/* Обработчик TCP сообщений от клиентов к клиенту */
void msg_tcp_handler(struct client_t *client, unsigned int slotid,
    msg_code_t code, char *msg, size_t msgsize)
{
    PROTO_PRINT("catch: msg_tcp_handler(%p, slotid:%d, code:%d, %p, size:%ld)\n",
        (void *)client, slotid, code, msg, msgsize);
    switch(code)
    {
        case PLACE_CONFIRM:
            on_place_confirm(client, slotid);
            break;
        case PLACE_REFUSE:
            on_place_refuse(client, slotid);
            break;
        case PLACE_ANCHOR:
            on_place_anchor(client, slotid, msg, msgsize);
            break;
        case CONNECTION_BORDER:
            on_connection_border(client, slotid, msg, msgsize);
            break;
        case CONNECTION_NEIGHBOR:
            on_connection_neighbor(client, slotid, msg, msgsize);
            break;
        case CONNECTION_READY:
            on_connection_ready(client, slotid);
            break;
    }
}

/* Специальные события */
/* Сообщает всем ранее подсоединившимся клиентам адрес сети,
    полученный от клиента вне локальной сети */
void on_netaddr_setup(struct client_t *client)
{
    struct unit_node_t *ptr;
    PROTO_PRINT("catch: on_netaddr_setup(%p)\n", (void *)client);
    /* Дополнительная проверка на то, вызвана ли процедура
        после инициализации диспетчера */
    if(client != NULL && client->dispatcher != NULL)
    {
        /* Всем, кто был подключен до определения адреса сети,
            передаем только что определенный адрес сети */
        ptr = client->dispatcher->units;
        while(ptr!=NULL)
        {
            PROTO_PRINT("\titer: %p->%d\n", (void *)ptr, ptr->unit.socket);
            msg_dispatcher_confirm(client, ptr->unit.socket);
            ptr = ptr->next;
        }
    }
}

#endif /* ifndef PROTOCOL_C */
