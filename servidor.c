#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define SERVER_PORT 67
#define MAX_CLIENTS 100
#define MAX_THREADS 50
#define IP_POOL_START "192.168.1.100"
#define LEASE_TIME 3600

#define NETMASK "255.255.255.0"
#define GATEWAY "192.168.1.1"
#define DNS_SERVER "8.8.8.8"

// Estructura para manejar clientes
typedef struct {
    struct sockaddr_in client_addr;
    char assigned_ip[16];
    time_t lease_expiry;
    char client_id[50]; // Añadir campo para CLIENT_ID
} dhcp_client;

// Array de clientes
dhcp_client clients[MAX_CLIENTS];
int client_count = 0;

// Pool de IPs disponibles
int ip_pool_index = 0;
char ip_pool[MAX_CLIENTS][16];
int active_threads = 0;
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t thread_limit_cond = PTHREAD_COND_INITIALIZER;

// Estructura para pasar datos al hilo
typedef struct {
    int sock;
    char buffer[1024];
    int bytes_received;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
} client_data_t;

// Inicializar el pool de IPs
void initialize_ip_pool() {
    struct in_addr addr;
    inet_aton(IP_POOL_START, &addr);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        addr.s_addr = htonl(ntohl(addr.s_addr) + 1);
        snprintf(ip_pool[i], sizeof(ip_pool[i]), "%s", inet_ntoa(addr));
    }
    printf("Pool de IPs inicializado con %d direcciones.\n", MAX_CLIENTS);
}

// Asignar una dirección IP del pool
char* assign_ip() {
    if (ip_pool_index < MAX_CLIENTS) {
        return ip_pool[ip_pool_index++];
    } else {
        printf("No hay más direcciones IP disponibles en el pool.\n");
        return NULL;
    }
}

// Manejar solicitud DHCP
void* handle_client(void* arg) {
    client_data_t* client_data = (client_data_t*)arg;
    int sock = client_data->sock;
    char* buffer = client_data->buffer;
    int bytes_received = client_data->bytes_received;
    struct sockaddr_in client_addr = client_data->client_addr;
    socklen_t addr_len = client_data->addr_len;
    struct sockaddr_in relay_addr;
    char client_id[50] = {0};

    // Extraer CLIENT_ID del mensaje
    char* id_start = strstr(buffer, "CLIENT_ID: ");
    if (id_start != NULL) {
        sscanf(id_start, "CLIENT_ID: %49s", client_id);
    } else {
        // Si no hay CLIENT_ID, usar la dirección IP como identificador (no recomendado)
        strcpy(client_id, inet_ntoa(client_addr.sin_addr));
    }

    // Procesar el paquete recibido
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("Mensaje recibido de %s (ID: %s): %s\n", inet_ntoa(client_addr.sin_addr), client_id, buffer);

        // Revisar si el paquete viene de un relay
        if (client_addr.sin_addr.s_addr == INADDR_ANY) {
            printf("Solicitud desde relay detectada.\n");
            inet_aton(GATEWAY, &relay_addr.sin_addr);
        } else {
            relay_addr = client_addr;
        }

        // Buscar si el cliente ya está registrado
        int client_index = -1;
        for (int i = 0; i < client_count; i++) {
            if (strcmp(clients[i].client_id, client_id) == 0) {
                client_index = i;
                break;
            }
        }

        // Manejar DHCPDISCOVER
        if (strstr(buffer, "DHCPDISCOVER")) {
            printf("Preparando DHCPOFFER para el cliente %s\n", client_id);

            // Asignar IP si no está asignada
            char* assigned_ip;
            if (client_index == -1) {
                assigned_ip = assign_ip();
                if (assigned_ip == NULL) {
                    printf("No hay más direcciones IP disponibles\n");
                    close(sock);
                    pthread_mutex_lock(&thread_count_mutex);
                    active_threads--;
                    pthread_cond_signal(&thread_limit_cond);
                    pthread_mutex_unlock(&thread_count_mutex);
                    free(client_data);
                    return NULL;
                }

                // Registrar el cliente
                dhcp_client new_client;
                new_client.client_addr = client_addr;
                strncpy(new_client.assigned_ip, assigned_ip, 16);
                new_client.lease_expiry = time(NULL) + LEASE_TIME;
                strncpy(new_client.client_id, client_id, 50);

                clients[client_count++] = new_client;
                client_index = client_count - 1;
            } else {
                // El cliente ya tiene una IP asignada
                assigned_ip = clients[client_index].assigned_ip;
            }

            // Crear y enviar DHCPOFFER
            memset(buffer, 0, sizeof(client_data->buffer));
            snprintf(buffer, sizeof(client_data->buffer),
                     "DHCPOFFER IP: %s NETMASK: %s GATEWAY: %s DNS: %s LEASE: %d",
                     assigned_ip, NETMASK, GATEWAY, DNS_SERVER, LEASE_TIME);

            int send_status = sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&relay_addr, addr_len);
            if (send_status == -1) {
                perror("Error al enviar DHCPOFFER");
            } else {
                printf("DHCPOFFER enviado con IP: %s al cliente %s\n", assigned_ip, client_id);
            }
        }

        // Manejar DHCPREQUEST
        else if (strstr(buffer, "DHCPREQUEST")) {
            printf("Procesando DHCPREQUEST del cliente %s\n", client_id);

            // Extraer la IP solicitada
            char requested_ip[16] = {0};
            sscanf(buffer, "DHCPREQUEST IP: %15s", requested_ip);

            if (client_index != -1) {
                char* assigned_ip = clients[client_index].assigned_ip;
                // Verificar que la IP solicitada coincide con la asignada
                if (strcmp(requested_ip, assigned_ip) == 0) {
                    memset(buffer, 0, sizeof(client_data->buffer));
                    snprintf(buffer, sizeof(client_data->buffer),
                             "DHCPACK IP: %s NETMASK: %s GATEWAY: %s DNS: %s LEASE: %d",
                             assigned_ip, NETMASK, GATEWAY, DNS_SERVER, LEASE_TIME);

                    sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&relay_addr, addr_len);
                    printf("DHCPACK enviado con IP: %s al cliente %s\n", assigned_ip, client_id);
                } else {
                    printf("Error: IP solicitada (%s) no coincide con la asignada (%s).\n", requested_ip, assigned_ip);
                    // Enviar DHCPNAK opcionalmente
                    memset(buffer, 0, sizeof(client_data->buffer));
                    snprintf(buffer, sizeof(client_data->buffer), "DHCPNAK");
                    sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&relay_addr, addr_len);
                }
            } else {
                printf("Error: Cliente %s no encontrado para el DHCPREQUEST.\n", client_id);
                // Enviar DHCPNAK opcionalmente
                memset(buffer, 0, sizeof(client_data->buffer));
                snprintf(buffer, sizeof(client_data->buffer), "DHCPNAK");
                sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&relay_addr, addr_len);
            }
        }
    } else {
        perror("No se recibió ningún mensaje");
    }

    // Liberar el hilo al finalizar
    pthread_mutex_lock(&thread_count_mutex);
    active_threads--;
    pthread_cond_signal(&thread_limit_cond);
    pthread_mutex_unlock(&thread_count_mutex);

    free(client_data);
    return NULL;
}

int main() {
    int sock;
    struct sockaddr_in server_addr;
    int opt = 1;

    // Inicializar pool de IPs
    initialize_ip_pool();

    // Crear socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Configurar opciones del socket
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error al configurar opciones del socket");
        close(sock);
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        perror("Error al configurar SO_BROADCAST");
        close(sock);
        exit(EXIT_FAILURE);
    }

    // Configurar dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Escuchar en todas las interfaces
    server_addr.sin_port = htons(SERVER_PORT);

    // Enlazar socket
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al enlazar");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Servidor DHCP esperando solicitudes en el puerto %d...\n", SERVER_PORT);

    // Ciclo para manejar clientes
    while (1) {
        char buffer[1024];
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int bytes_received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client_addr, &addr_len);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Paquete recibido: %s\n", buffer);

            pthread_mutex_lock(&thread_count_mutex);
            while (active_threads >= MAX_THREADS) {
                pthread_cond_wait(&thread_limit_cond, &thread_count_mutex);
            }
            pthread_mutex_unlock(&thread_count_mutex);

            // Preparar datos para el hilo
            client_data_t* client_data = malloc(sizeof(client_data_t));
            client_data->sock = sock;
            memcpy(client_data->buffer, buffer, bytes_received);
            client_data->bytes_received = bytes_received;
            client_data->client_addr = client_addr;
            client_data->addr_len = addr_len;

            pthread_t thread_id;

            if (pthread_create(&thread_id, NULL, handle_client, (void*)client_data) == 0) {
                pthread_mutex_lock(&thread_count_mutex);
                active_threads++;
                pthread_mutex_unlock(&thread_count_mutex);
                pthread_detach(thread_id);
            } else {
                perror("Error al crear el hilo");
                free(client_data);
            }
        } else {
            perror("Error al recibir paquete");
        }
    }

    close(sock);
    pthread_mutex_destroy(&thread_count_mutex);

    return 0;
}
