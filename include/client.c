/*
 ============================================================================
 Name        : client.c
 Author      : float.cat
 Version     : 0.31
 Description : Реализация асинхронного клиент-клиента с самодеспетчеризацией
 ============================================================================
 */

#ifndef CLIENT_C
#define CLIENT_C

#include "protocol.h"
#include "client.h"

struct client_t *client_create(void)
{
    int i;
    struct client_t *client;
    struct sockaddr_in sa;
    addr_data_t dispatcher_addr;
    client = (struct client_t *)malloc(sizeof(struct client_t));
    /* Обнуляем структуру адреса */
    memset(&sa, 0, sizeof(struct sockaddr_in));
    /* Устанавливаем все слоты в состояние свободен */
    for(i=0; i<NUMBER_SLOTS; i++)
    {
        client->slots[i].socket = -1;
        client->slots[i].status = SLOT_STATUS_FREE;
    }
    /* Удаление от диспетчера отсутствует, т.к. клиент не занял
        слот в распределении */
    client->distance = INVALID_DISTANCE;
    client->netsdata = NULL;
    client->dispatcher = NULL;
    /* Состояние выполнения протокола сброшено в начало */
    client->state.state = PROTOCOL_STARTED;
    client->state.attr = 0;
    srand(time(NULL));

    /* Адрес клиента в сетях, в том числе localhost, и широковещательные адреса сетей */
    client->netsdata = client_prepare_netsdata(&client->netscount);

    /* Перед подключением к диспетчеру инициализируем сокет слушатель для TCP,
        который будет собирать участок матрицы соединений для этого клиента */
    client->listenerTCP = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    sa.sin_family = PF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    do /* Выбираем порт, пока не наткнемся на свободный */
    {
        client->portTCP = 10000+rand()%5000;
        sa.sin_port = htons(client->portTCP);
    }  /* Связываем сокет с портом, при неудаче генерируем порт заново */
    while(bind(client->listenerTCP,
        (struct sockaddr *)&sa, sizeof(struct sockaddr))<0);
    /* Очередь ожидания на подключение */
    listen(client->listenerTCP, 1);

    /* Подключаемся к диспетчеру */
    /* Инициализируем сокет для отправки сообщений диспетчеру по UDP */
    client->sockUDP = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);

    /* Ищем диспетчер в сети, если его нет, то либо он на этом
        компьютере, либо его вообще нет, поэтому клиент пробует
        инициализироваться, как диспетчер */
    dispatcher_addr = client_wait_dispatcher_discover_anwer(client);
    if(!dispatcher_addr)
        client_dispatchering_init(client);

    /* Инициализируем сокет для отправки сообщений диспетчеру по TCP */
    client->sockTCP = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    sa.sin_family = PF_INET;
    /* Используем адрес, который определен по UDP ответу от диспетчера
        или, если ответ не был получен, используем LOOPBACK */
    sa.sin_addr.s_addr = htonl(dispatcher_addr?dispatcher_addr:INADDR_LOOPBACK);
    sa.sin_port = htons(DISPATCHER_PORT);
    /* Соединяемся с диспетчером, даже если этот клиент и есть диспетчер */
    connect(client->sockTCP, (struct sockaddr*)&sa, sizeof(struct sockaddr_in));

    /* Для одновременного запуска используем классическую схему n+1
        Каждая нить проходит барьер синхронизации тогда, когда его
        проходит главная нить */
    pthread_barrier_init(&(client->starter), 0, 4);
    /* Создаем нити чтения из сетевых сокетов */
    pthread_create(&client->thrdTCP, NULL,
        client_tcp_handler, client);
    pthread_create(&client->thrdacceptor, NULL,
        client_tcp_acceptor, client);
    pthread_create(&client->thrddialog, NULL,
        client_tcp_dialog, client);
    /* Спускаем барьер старта всех нитей клиента */
    pthread_barrier_wait(&(client->starter));

    /* Отправляем сообщение о том, что удаление 0, если клиент
        инициализовал себя, как диспетчер */
    if(client->dispatcher != NULL)
        msg_connection_distance(client, client->distance = 0);

    return client;
}


void client_destroy(struct client_t * client)
{
    if(client == NULL)
        return;
    close(client->listenerTCP);
    client_dispatcher_release(client);
    if(client->netsdata!=NULL)
        free(client->netsdata);
    free(client);
}

addr_data_t client_get_ipaddr_by_netaddr(struct client_t *client,
    addr_data_t netaddr)
{
    int i = 0;
    while(i < client->netscount)
    {
        /* Вычисляем адреса сетей и проверяем совпадает ли */
        if((client->netsdata[i].ipaddr &
            client->netsdata[i].netmask) == netaddr)
                return client->netsdata[i].ipaddr;
        i++;
    }
    return 0;
}

addr_data_t client_get_netaddr_by_ipaddr(struct client_t *client,
    addr_data_t ipaddr)
{
    int i = 0;
    while(i < client->netscount)
    {
        /* Опираемся на результат исключающего или для двух адресов,
            затем побитно домножаем на маску, если адреса из одной сети
            в результате получим 0 */
        if(((client->netsdata[i].ipaddr ^ ipaddr) &
            client->netsdata[i].netmask) == 0)
                return client->netsdata[i].ipaddr;
        i++;
    }
    return 0;
}

addr_data_t client_wait_dispatcher_discover_anwer(struct client_t *client)
{
    int sdUDP;
    dg_code_t code;
    struct sockaddr_in sa;
    socklen_t *address_len=NULL;
    fd_set rfds;
    struct timeval tv;
    /* Устанавливаем сообщение в 0, чтобы потом проверить */
    code = 0;
    /* Инициализируем сокет, на который будем ждать ответ */
    sdUDP = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sa.sin_family = PF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(DISPATCHER_PORT);
    /* Связываем сокет с портом */
    if(bind(sdUDP, (struct sockaddr *)&sa, sizeof(struct sockaddr))<0)
    {
        /* Порт диспетчера занят, значит он уже существует 
            и располагается на этом компьютере */
        close(sdUDP);
        /* Сообщаем 0 вместо адреса, т.к. предполагаем, что
            диспетчер есть на этом компьютере */
        return 0;
    }
    /* Инициализируем множество ожидания,
        в данном случае только порт ответа */
    FD_ZERO(&rfds);
    FD_SET(sdUDP, &rfds);
    /* Устанавливаем время ожидания ответа в микросекундах */
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    /* Отсылаем широковещательное сообщение DISPATCHER_DISCOVER */
    dg_dispatcher_discover(client);
    /* Ждем ответ DISPATCHER_IM указанное время */
    select(sdUDP+1, &rfds, NULL, NULL, &tv);
    if(FD_ISSET(sdUDP, &rfds))
    {
        /* Ответ был получен */
        recvfrom(sdUDP, &code, sizeof(dg_code_t), 0,
            (struct sockaddr *)&sa, address_len);
    }
    close(sdUDP);
    /* Проверяем является ли ответ сообщением DISPATCHER_IM */
    if(code == DISPATCHER_IM)
        /* Возвращаем адрес диспетчера, представленный в обычном порядке */
        return ntohl(sa.sin_addr.s_addr);
    /* Во всех прочих случаях считаем что ответа нет */
    return 0;
}

void client_dispatchering_init(struct client_t *client)
{
    int bound;
    int sdUDP, sdTCP;
    struct sockaddr_in sa;
    if(client == NULL)
        return;
    client->dispatcher = NULL;

    /* Инициализируем сокет диспетчера, чтобы слушать TCP */
    sdTCP = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    sa.sin_family = PF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(DISPATCHER_PORT);
    bound = bind(sdTCP, (struct sockaddr *)&sa, sizeof(struct sockaddr));
    if(bound<0)
    {
        /* Порт диспетчера занят, значит он уже существует 
            и располагается на этом компьютере */
        close(sdTCP);
        return;
    }
    /* Очередь ожидания на подключение */
    listen(sdTCP, 1);

    /* Инициализируем сокет диспетчера, чтобы слушать UDP */
    sdUDP = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sa.sin_family = PF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(DISPATCHER_PORT);
    /* Связываем сокет с портом */
    bound = bind(sdUDP, (struct sockaddr *)&sa, sizeof(struct sockaddr));
    if(bound<0)
    {
        /* Порт диспетчера занят, значит он уже существует 
            и располагается на этом компьютере */
        close(sdUDP);
        return;
    }

    /* Создаем структуру диспетчера */
    client->dispatcher =
        (struct dispatcher_t *)malloc(sizeof(struct dispatcher_t));
    /* Указываем, что список диспетчеризации пуст */
    client->dispatcher->units = NULL;
    FD_ZERO(&client->dispatcher->fds);
    /* Инициализируем дескриптор отправки */
    client->dispatcher->socketUDP = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    /* Регистрируем дескрипторы приема */
    client->dispatcher->sdTCP = sdTCP;
    client->dispatcher->sdUDP = sdUDP;
    /* Для одновременного запуска используем классическую схему n+1
        Каждая нить проходит барьер синхронизации тогда, когда его
        проходит главная нить */
    pthread_barrier_init(&(client->dispatcher->starter), 0, 4);
    /* Создаем нити чтения из сетевых сокетов */
    pthread_create(&client->dispatcher->thrdTCP, NULL,
        client_dispatcher_tcp_handler, client);
    pthread_create(&client->dispatcher->thrdacceptor, NULL,
        client_dispatcher_tcp_acceptor, client);
    pthread_create(&client->dispatcher->thrdUDP, NULL,
        client_dispatcher_udp_handler, client);
    /* Спускаем барьер старта всех нитей клиента */
    pthread_barrier_wait(&(client->dispatcher->starter));
}

void client_dispatcher_release(struct client_t *client)
{
    if(client->dispatcher != NULL)
    {
        /* Остановка нитей */
        /* Закрытие сокета отправки */
        close(client->dispatcher->socketUDP);
        /* Освобождение портов диспетчирезации и закрытие
            сокетов приема */
        close(client->dispatcher->sdUDP);
        close(client->dispatcher->sdTCP);
    }
    free(client->dispatcher);
    client->dispatcher = NULL;
}

struct net_data_t *client_prepare_netsdata(unsigned char * count)
{
    int i, idx, n, sock;
    struct net_data_t *result;
    /* Инициализируем контейнеры для ответов ioctl */
    struct ifreq ifr[DEFAULT_NETADDRSCOUNT];
    struct ifconf ifconf_info;
    struct sockaddr_in *sa;
    /* Обнуляем память, выделенную под контейнеры */
    memset(&ifr, 0, sizeof(struct ifreq)*DEFAULT_NETADDRSCOUNT);
    memset(&ifconf_info, 0, sizeof(struct ifconf));
    /* Связываем структуру ifconf с контейнером типа ifreq */
    ifconf_info.ifc_len = sizeof(struct ifreq)*DEFAULT_NETADDRSCOUNT;
    ifconf_info.ifc_req = ifr;

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    /* Опрашиваем ioctl(SIOCGIFCONF), чтобы получить
        имена сетевых интерфейсов */
    ioctl(sock, SIOCGIFCONF, &ifconf_info);

    /* Вычисляем количество сетевых интерфейсов, которые вернул ioctl,
        отнимаем -1, т.к. интерфейс lo: в нашем протоколе не используется
        для сообщений UDP */
    n = ifconf_info.ifc_len/sizeof(struct ifreq)-1;
    /* Если n = 0, то у нас есть только интерфейс lo:, а реальные сети
        отсутствуют, а значит не требуется широковещательный запрос DISPATCHER_DISCOVER */
    if(n < 1)
    {
        /* Освобождаем сокет и покидаем процедуру, возвращая 0
            и сообщая размер массива 0 */
        *count = 0;
        close(sock);
        return NULL;
    }
    /* Инициализируем наш перечень необходимых данных сетевых интерфейсов */
    result = (struct net_data_t *)malloc(sizeof(struct net_data_t)*n);
    /* Для каждого интерфейса сети, кроме lo: */
    idx = i = 0;
    while(idx<n)
    {
        /* Опрашиваем ioctl(SIOCGIFADDR) для получения
            адреса этого компьютера в этой сети */
        ioctl(sock, SIOCGIFADDR, &ifr[i]);
        sa = (struct sockaddr_in*)&ifr[i].ifr_addr;
        /* Если текущий интерфейс не lo: */
        if(sa->sin_addr.s_addr != IPADDR_LOCALHOST)
        {
            /* Записываем адрес компьютера в не локальной сети */
            result[idx].ipaddr = ntohl(sa->sin_addr.s_addr);
            /* Опрашиваем ioctl(SIOCGIFNETMASK) для получения
                адресной маски этой сети */
            ioctl(sock, SIOCGIFNETMASK, &ifr[i]);
            sa = (struct sockaddr_in*)&ifr[i].ifr_netmask;
            result[idx].netmask = ntohl(sa->sin_addr.s_addr);
            /* Опрашиваем ioctl(SIOCGIFBRDADDR) для получения
                широковещательного адреса этой сети */
            ioctl(sock, SIOCGIFBRDADDR, &ifr[i]);
            sa = (struct sockaddr_in*)&ifr[i].ifr_broadaddr;
            result[idx].broadaddr = ntohl(sa->sin_addr.s_addr);
            idx++;
        }
        i++;
    }
    *count = n;
    close(sock);
    return result;
}

unsigned int client_has_free_slot(struct client_t *client)
{
    int i, retval = INVALID_SLOT;
    for(i=0; i<NUMBER_SLOTS; i++)
        /* Если обнаружен признак свободного слота */
        if(client->slots[i].status == SLOT_STATUS_FREE)
        {
            /* Запоминаем идентификатор слота и прерываем цикл */
            retval = i;
            break;
        }
    /* Возвращаем идентификатор слота, если нет свободных - останется
        некорректный слот */
    return retval;
}

void client_connect_to_client(struct client_t *client, unsigned int slotid,
    addr_data_t ipaddr, unsigned short port)
{
    int sock;
    struct sockaddr_in sa;
    /* Обнуляем структуру адреса */
    memset(&sa, 0, sizeof(struct sockaddr_in));
    /* Заполняем адрес и порт в структуре адреса */
    sa.sin_family = PF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = htonl(ipaddr ? ipaddr : INADDR_LOOPBACK);
    /* Создаем сокет и запоминаем дескриптор */
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    /* Выполняем соединение по адрес:порт */
    connect(sock, (struct sockaddr *)&sa, sizeof(struct sockaddr_in));
    client_use_slot(client, slotid, sock, ipaddr, port);
}

int client_gettopsock(struct client_t *client)
{
    int i, max = -1;
    /* Ищем максимальный дескриптор */
    for(i=0; i<NUMBER_SLOTS; i++)
        /* Проверяем только те слоты, которые заняты */
        if(client->slots[i].status != SLOT_STATUS_FREE &&
            client->slots[i].socket>max)
            /* Запоминаем его */
            max = client->slots[i].socket;
    /* Возвращаем максимальный дескриптор увеличенный на 1,
        т.е. если нет дескрипторов, то вернется 0 */
    return max+1;
}

void client_use_slot(struct client_t * client, unsigned int slotid,
    int socket, addr_data_t ipaddr, unsigned short port)
{
    struct slot_t *slot = client->slots + slotid;
    /* Занимаем слот */
    slot->status = SLOT_STATUS_PREPARE;
    slot->socket = socket;
    slot->ipaddr = ipaddr;
    slot->port = port;
    /* Добавляем во множество дескрипторов соседей */
    FD_SET(socket, &client->fds);
    /* Корректируем границу обработки select */
    if(client->topsock - socket < 1)
        client->topsock = socket+1;
}

void client_release_slot(struct client_t *client, unsigned int slotid)
{
    struct slot_t *slot = client->slots + slotid;
    /* Помечаем слот, как свободный */
    slot->status = SLOT_STATUS_FREE;
    /* Корректируем границу дескрипторов для select */
    client->topsock = client_gettopsock(client);
    /* Удаляем из множества дескрипторов сокетов для
        асинхронного чтения */
    FD_CLR(slot->socket, &client->fds);
    /* Закрываем сокет */
    close(slot->socket);
}

void client_slots_swap(struct client_t *client, unsigned int slotid,
    unsigned int newslotid)
{
    struct slot_t slot;
    /* Меняем местами через третью переменную */
    memcpy(&slot, client->slots+slotid, sizeof(struct slot_t));
    memcpy(client->slots+slotid, client->slots+newslotid, sizeof(struct slot_t));
    memcpy(client->slots+newslotid, &slot, sizeof(struct slot_t));
}

void client_slot_ready(struct client_t *client, unsigned int slotid)
{
    client->slots[slotid].status = SLOT_STATUS_READY;
}

void client_dispatcher_add_unit(struct client_t *client, int socket)
{
    /* Так как нам не важен порядок - используем самый простой алгоритм списка -
        стек на односвязном списке */
    struct unit_node_t *ptr;
    /* Выделяем память на новый элемент списка */
    ptr = (struct unit_node_t *)malloc(sizeof(struct unit_node_t));
    /* Запоминаем дескриптор сокета соединения */
    ptr->unit.socket = socket;
    /* Указываем, что у нас нет данных о удалении клиента от диспетчера */
    ptr->unit.distance = INVALID_DISTANCE;
    /* Блокируем мьютекс работы со списком */
    pthread_mutex_lock(&(client->dispatcher->listlocker));
    /* Ссылаемся новым элементом на первый элемент списка */
    ptr->next = client->dispatcher->units;
    /* Делаем новый элемент первым элементом списка */
    client->dispatcher->units = ptr;
    /* Добавляем элемент в множество дескрипторов */
    FD_SET(socket, &client->dispatcher->fds);
    /* Если нужно - обновляем границу сокетов для select */
    if(client->dispatcher->topsock - socket < 1)
        client->dispatcher->topsock = socket + 1;
    /* Снимаем блокировку мьютекса работы со списком */
    pthread_mutex_unlock(&(client->dispatcher->listlocker));
}

void client_dispatcher_remove_unit(struct client_t *client, int socket)
{
    struct unit_node_t *ptr, *prev;
    ptr = NULL;
    /* Ищем элемент, стоящий перед тем, который удаляем */
    prev = client_dispatcher_prev_unit(client->dispatcher, socket);
    /* Если адрес ноль - значит перед нами диспетчер или ничего не найдено */
    if(prev == NULL) 
    {
        /* Проводим дополнительную проверку на то, что это
            не отсутствие совпадений, и удаляется именно диспетчер */
        if(client->dispatcher->units->unit.socket == socket)
        {
            /* Запоминаем старый первый элемент */
            ptr = client->dispatcher->units;
            /* Первым элементом становится следующий
                (или ничего, если следующего нет) */
            client->dispatcher->units = client->dispatcher->units->next;
        }
    }
    else
    {
        /* Запоминаем указатель на следующий за предыдущим
            (т.е. тот, который удаляем) */
        ptr = prev->next;
        /* Связываем список, исключая удаляемый элемент */
        prev->next = ptr->next;
    }
    /* Если какое-то из условий удаления сработало,
        и в ptr находится адрес удаляемого элемента */
    if(ptr != NULL)
    {
        /* То убираем дескриптор сокета из множества */
        FD_CLR(socket, &client->dispatcher->fds);
        /* И освобождаем память */
        free(ptr);
        /* Если дескриптор сокета младше topsock на 1 */
        if(client->dispatcher->topsock - socket == 1)
            /* Это был старший сокет и теперь необходимо найти
                старший из оставшихся */
            client->dispatcher->topsock =
                client_dispatcher_gettopsock(client->dispatcher);
        /* После вывода дескриптора сокета из асинхронной обработки -
            его можно закрыть */
        close(socket);
    }
}

struct unit_node_t *client_dispatcher_prev_unit(struct dispatcher_t *dispatcher, int socket)
{
    struct unit_node_t *ptr, *prev;
    prev = NULL;
    /* Блокируем мьютекс работы со списком */
    pthread_mutex_lock(&(dispatcher->listlocker));
    ptr = dispatcher->units;
    while(ptr!=NULL)
    {
        /* Если дескриптор совпадает, то завершаем цикл, чтобы
            перейти к освобождению мьютекса */
        if(ptr->unit.socket == socket)
            break;
        /* Переходим к следующему элементу */
        prev = ptr;
        ptr = ptr->next;
    }
    /* Снимаем блокировку мьютекса работы со списком */
    pthread_mutex_unlock(&(dispatcher->listlocker));
    return prev;
}

int client_dispatcher_gettopsock(struct dispatcher_t *dispatcher)
{
    struct unit_node_t *ptr;
    int max;
    /* Блокируем мьютекс работы со списком */
    pthread_mutex_lock(&(dispatcher->listlocker));
    ptr = dispatcher->units;
    max = -1;
    while(ptr!=NULL)
    {
        /* Если дескриптор больше большего - теперь он больший */
        if(ptr->unit.socket > max)
            max = ptr->unit.socket;
        /* Переходим к следующему элементу */
        ptr = ptr->next;
    }
    /* Снимаем блокировку мьютекса работы со списком */
    pthread_mutex_unlock(&(dispatcher->listlocker));
    /* Возвращаем старший сокет + 1 для select */
    return max+1;
}

void *client_dispatcher_udp_handler(void *arg)
{
    struct client_t *client = (struct client_t *)arg;
    struct sockaddr_in *sa;
    socklen_t *address_len=NULL;
    dg_code_t code;
    addr_data_t netaddr;
    pthread_barrier_wait(&(client->dispatcher->starter));
    /* Если клиент не инициализирован как диспетчер - уходим */
    if(client == NULL || client->dispatcher == NULL)
        return NULL;
    while(1)
    {
        /* Так как у нас в нити только один описатель соединения,
            то нам не требуется асинхронное чтение */
        recvfrom(client->dispatcher->sdUDP, &code, sizeof(dg_code_t), 0,
            (struct sockaddr *)&sa, address_len);
        netaddr = client_get_netaddr_by_ipaddr(client, sa->sin_addr.s_addr);
        /* Адрес сети еще не выбран, то это первое соединение с внешним клиентом
            устанавливаем подходящий адрес */
        if(client->dispatcher->netaddr == 0)
        {
            client->dispatcher->netaddr = netaddr;
            /* Сигнализируем всем локальным клиентам, которые подключились до
                определения сетевого адреса, по протоколу */
            on_netaddr_setup(client);
        }
        /* Если маска диспетчеризуемой сети подходит обратному адресу */
        else if(client->dispatcher->netaddr == netaddr)
            /* Передаем управление обработчику UDP сообщений от клиентов
                к диспетчеру по протоколу */
            dg_dispatcher_udp_handler(client, sa->sin_addr.s_addr, code);
    }
    return NULL;
}


void *client_dispatcher_tcp_acceptor(void *arg)
{
    struct client_t *client = (struct client_t *)arg;
    int sdNew;
	pthread_barrier_wait(&(client->dispatcher->starter));
    /* Если клиент не инициализирован как диспетчер - уходим */
    if(client == NULL || client->dispatcher == NULL)
        return NULL;
    while(1)
    {
        /* Так как у нас в нити только один описатель соединения,
            то нам не требуется асинхронное чтение */
        /* Принимаем входящее подключение из очереди ожидания */
        sdNew = accept(client->dispatcher->sdTCP, NULL, NULL);
        /* Добавляем подключение в стек клиентов
            и множество дескрипторов */
        client_dispatcher_add_unit(client, sdNew);
        /* Передаем управление обработчику подключений клиентов
            к диспетчеру по протоколу */
        msg_dispatcher_tcp_acceptor(client, sdNew);
    }
    return NULL;
}

void *client_dispatcher_tcp_handler(void *arg)
{
    struct client_t *client = (struct client_t *)arg;
    fd_set rfds;
    struct timeval tv;
    struct unit_node_t *ptr;
    char buffer[TCP_MSG_SIZE];
    ssize_t recvsize;
    unsigned int code;
    ssize_t msgsize;
	pthread_barrier_wait(&(client->dispatcher->starter));
    /* Если клиент не инициализирован как диспетчер - уходим */
    if(client == NULL || client->dispatcher == NULL)
        return NULL;
    /* Устанавливаем время ожидания ответа в микросекундах */
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    while(1)
    {
        /* Клонируем содержимое множества дескрипторов диспетчера, так как
            select изменяет множество, полученное в качестве аргумента */
        memcpy(&rfds, &client->dispatcher->fds, sizeof(fd_set));
        select(client->dispatcher->topsock, &rfds, NULL, NULL, &tv);
        /* Обрабатываем все дескрипторы, принявшие данные */
        ptr = client->dispatcher->units;
        while(ptr != NULL)
        {
            /* Если сокет элемента находится во множестве доступных для чтения */
            if(FD_ISSET(ptr->unit.socket, &rfds))
            {
                /* Принимаем код сообщения */
                recvsize = recv(ptr->unit.socket, &code, sizeof(msg_code_t), MSG_WAITALL);
                if(recvsize > 0)
                {
                    /* Узнаем размер данных, соответствующих этому сообщению */
                    msgsize = size_of_msg_tcp_data(code);
                    /* Если есть данные, которые соответствуют */
                    if(msgsize)
                        /* Принимаем их, считая, что у нас всегда правильный протокол
                            это допущение позволяет использовать MSG_WAITALL, и значительно
                            упрощает прием */
                        recvsize += recv(ptr->unit.socket, buffer, msgsize, MSG_WAITALL);
                    /* Передаем управление обработчику TCP сообщений от клиентов к диспетчеру
                        по протоколу */
                    msg_dispatcher_tcp_handler(client, &ptr->unit, code, buffer, 0);
                }
                /* Если вернуло 0 байт, значит соединение закрылось с той стороны */
                if(!recvsize)
                {
                    /* Удаляем элемент из списка */
                    /* (Можно удалить за O(1) передав адрес предыдущего) */
                    client_dispatcher_remove_unit(client, ptr->unit.socket);
                }
            }
            ptr = ptr->next;
        }
    }
    return NULL;
}

/* Обработчики входящих сообщений для клиента */
/* Обработчик TCP для диалога с диспетчером, в качестве аргумента
    получает указатель на структуру клиента */
void *client_tcp_dialog(void *arg)
{
    struct client_t *client = (struct client_t *)arg;
    size_t msgsize;
    char buffer[TCP_MSG_SIZE];
    msg_code_t code;
    int recvsize;
    pthread_barrier_wait(&(client->starter));
    while(1)
    {
        /* Так как у нас в нити только один описатель соединения,
            то нам не требуется асинхронное чтение */
        /* Принимаем код сообщения */
        recvsize = recv(client->sockTCP, &code, sizeof(msg_code_t), MSG_WAITALL);
        if(recvsize > 0)
        {
            /* Узнаем размер данных, соответствующих этому сообщению */
            msgsize = size_of_msg_tcp_data(code);
            /* Если есть данные, которые соответствуют */
            if(msgsize)
                /* Принимаем их, считая, что у нас всегда правильный протокол
                    это допущение позволяет использовать MSG_WAITALL, и значительно
                    упрощает прием */
                    recvsize += recv(client->sockTCP, buffer, msgsize, MSG_WAITALL);
            /* Передаем управление обработчику TCP сообщений от диспетчера к клиенту
                по протоколу */
            msg_tcp_dialog(client, code, buffer, 0);
        }
        /* Если вернуло 0 байт, значит соединение закрылось с той стороны */
        if(!recvsize)
        {
            /* Вызываем остановку цикла */
            break;
        }
    }
    return NULL;
}

/* TCP приемщик для соседей клиента, в качестве аргумента
    получает указатель на структуру клиента */
void *client_tcp_acceptor(void *arg)
{
    struct client_t *client = (struct client_t *)arg;
    struct sockaddr_in sa;
    socklen_t sa_len = sizeof(struct sockaddr);
    unsigned int slotid;
    int sdNew;
    pthread_barrier_wait(&(client->starter));
    while(1)
    {
        /* Так как у нас в нити только один описатель соединения,
            то нам не требуется асинхронное чтение, кроме того
            заполняем структуру sockaddr, чтобы узнать IP подключения */
        sdNew = accept(client->listenerTCP, (struct sockaddr *)&sa, &sa_len);
        /* Передаем управление обработчику подключений клиентов
            к клиенту по протоколу, вернет слот для подключения */
        slotid = msg_tcp_acceptor(client, sdNew);
        if(slotid != INVALID_SLOT)
        {
            /* Запоминаются реквизиты входящих подключений,
                в данном типе распределения - это тот клиент
                которые предоставляют место в распределении
                текущему клиенту */
            client_use_slot(client, slotid, sdNew,
                ntohl(sa.sin_addr.s_addr), ntohs(sa.sin_port));
        }
        else
            close(sdNew);
    }
    return NULL;
}

/* Обработчик TCP для соседей клиента, в качестве аргумента
    получает указатель на структуру клиента */
void *client_tcp_handler(void *arg)
{
    struct client_t *client = (struct client_t *)arg;
    fd_set rfds;
    struct timeval tv;
    char buffer[TCP_MSG_SIZE];
    msg_code_t code;
    size_t msgsize;
    unsigned int i, recvsize;
    pthread_barrier_wait(&(client->starter));
    /* Устанавливаем время ожидания ответа в микросекундах */
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    while(1)
    {
        /* Клонируем содержимое множества дескрипторов диспетчера, так как
            select изменяет множество, полученное в качестве аргумента */
        memcpy(&rfds, &client->fds, sizeof(fd_set));
        select(client->topsock, &rfds, NULL, NULL, &tv);
        /* Обрабатываем все дескрипторы, принявшие данные */
        for(i=0; i<NUMBER_SLOTS; i++)
        {
            /* Если сокет элемента находится во множестве доступных для чтения */
            if(FD_ISSET(client->slots[i].socket, &rfds))
            {
                /* Принимаем код сообщения */
                recvsize = recv(client->slots[i].socket, &code, sizeof(msg_code_t), MSG_WAITALL);
                if(recvsize > 0)
                {
                    /* Узнаем размер данных, соответствующих этому сообщению */
                    msgsize = size_of_msg_tcp_data(code);
                    /* Если есть данные, которые соответствуют */
                    if(msgsize)
                        /* Принимаем их, считая, что у нас всегда правильный протокол
                            это допущение позволяет использовать MSG_WAITALL, и значительно
                            упрощает прием */
                        recvsize += recv(client->slots[i].socket, buffer, msgsize, MSG_WAITALL);
                    /* Передаем данные обработчику сообщений по протоколу */
                    msg_tcp_handler(client, i, code, buffer, 0);
                }
                /* Если вернуло 0 байт, значит соединение закрылось с той стороны */
                if(!recvsize)
                {
                    client_release_slot(client, i);
                }
            }
        }
    }
    return NULL;
}

#endif /* ifndef CLIENT_C */
