#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>

#define SERVER_PORT 1067
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

// Mutexes y condiciones
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t thread_limit_cond = PTHREAD_COND_INITIALIZER;

// Nuevo mutex para proteger acceso a clients[] y client_count
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Estructura para pasar datos al hilo
typedef struct {
    int sock;
    char buffer[1024];
    int bytes_received;
    struct sockaddr_in client_addr;
    socklen_t addr_len;
} client_data_t;

// Declaración de la función mostrar_tabla_ips
void mostrar_tabla_ips();

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
    char recv_buffer[1024];
    int bytes_received = client_data->bytes_received;
    struct sockaddr_in client_addr = client_data->client_addr;
    socklen_t addr_len = client_data->addr_len;
    char send_buffer[1024] = {0};
    char client_id[50] = {0};
    char requested_ip[16] = {0};

    // Copiar el mensaje recibido al recv_buffer
    strncpy(recv_buffer, client_data->buffer, bytes_received);
    recv_buffer[bytes_received] = '\0';

    // Depuración: Mostrar el contenido completo del recv_buffer
    printf("Buffer content: '%s'\n", recv_buffer);

    // Extraer DHCPREQUEST o DHCPDISCOVER
    if (strstr(recv_buffer, "DHCPREQUEST") != NULL) {
        // Parsing DHCPREQUEST
        // Expected format: DHCPREQUEST IP: <IP> CLIENT_ID: <ClientID>
        int parsed = sscanf(recv_buffer, "DHCPREQUEST IP: %15s CLIENT_ID: %49s", requested_ip, client_id);
        if (parsed != 2) {
            printf("Error: Formato de DHCPREQUEST inválido.\n");
            // Enviar DHCPNAK opcionalmente
            snprintf(send_buffer, sizeof(send_buffer), "DHCPNAK");
            sendto(sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&client_addr, addr_len);
            goto cleanup;
        }
    }
    else if (strstr(recv_buffer, "DHCPDISCOVER") != NULL) {
        // Parsing DHCPDISCOVER
        // Expected format: DHCPDISCOVER CLIENT_ID: <ClientID>
        char* id_start = strstr(recv_buffer, "CLIENT_ID: ");
        if (id_start != NULL) {
            sscanf(id_start, "CLIENT_ID: %49s", client_id);
        } else {
            // Si no hay CLIENT_ID, usar la dirección IP como identificador (no recomendado)
            strcpy(client_id, inet_ntoa(client_addr.sin_addr));
        }
    }
    else {
        printf("Error: Tipo de mensaje desconocido.\n");
        goto cleanup;
    }

    // Depuración: Mostrar el CLIENT_ID extraído
    printf("Extracted CLIENT_ID: '%s'\n", client_id);

    // Revisar si el paquete viene de un relay
    struct sockaddr_in relay_addr;
    if (client_addr.sin_addr.s_addr == INADDR_ANY) {
        printf("Solicitud desde relay detectada.\n");
        memset(&relay_addr, 0, sizeof(relay_addr));
        relay_addr.sin_family = AF_INET;
        inet_aton(GATEWAY, &relay_addr.sin_addr);
        relay_addr.sin_port = htons(SERVER_PORT); // Asegurar que el puerto está establecido
    } else {
        relay_addr = client_addr;
    }

    // Buscar si el cliente ya está registrado
    int client_index = -1;

    // Bloquear el mutex antes de acceder a clients[] y client_count
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].client_id, client_id) == 0) {
            client_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex); // Desbloquear el mutex

    // Manejar DHCPDISCOVER
    if (strstr(recv_buffer, "DHCPDISCOVER")) {
        printf("Preparando DHCPOFFER para el cliente %s\n", client_id);

        // Asignar IP si no está asignada
        char* assigned_ip;
        if (client_index == -1) {
            assigned_ip = assign_ip();
            if (assigned_ip == NULL) {
                printf("No hay más direcciones IP disponibles\n");
                // Enviar DHCPNAK opcionalmente
                snprintf(send_buffer, sizeof(send_buffer), "DHCPNAK");
                sendto(sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&relay_addr, addr_len);
                goto cleanup;
            }

            // Registrar el cliente
            dhcp_client new_client;
            new_client.client_addr = client_addr;
            strncpy(new_client.assigned_ip, assigned_ip, 16);
            new_client.lease_expiry = time(NULL) + LEASE_TIME;
            strncpy(new_client.client_id, client_id, 50);

            // Bloquear el mutex antes de modificar clients[] y client_count
            pthread_mutex_lock(&clients_mutex);
            clients[client_count++] = new_client;
            client_index = client_count - 1;
            pthread_mutex_unlock(&clients_mutex); // Desbloquear el mutex
        } else {
            // El cliente ya tiene una IP asignada
            assigned_ip = clients[client_index].assigned_ip;
        }

        // Crear y enviar DHCPOFFER
        snprintf(send_buffer, sizeof(send_buffer),
                 "DHCPOFFER IP: %s NETMASK: %s GATEWAY: %s DNS: %s LEASE: %d",
                 assigned_ip, NETMASK, GATEWAY, DNS_SERVER, LEASE_TIME);

        int send_status = sendto(sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&relay_addr, addr_len);
        if (send_status == -1) {
            perror("Error al enviar DHCPOFFER");
        } else {
            printf("DHCPOFFER enviado con IP: %s al cliente %s\n", assigned_ip, client_id);
        }
    }

    // Manejar DHCPREQUEST
    else if (strstr(recv_buffer, "DHCPREQUEST")) {
        printf("Procesando DHCPREQUEST del cliente %s\n", client_id);
        printf("Requested IP: '%s'\n", requested_ip);

        if (client_index != -1) {
            char* assigned_ip;

            // Bloquear el mutex antes de acceder a clients[]
            pthread_mutex_lock(&clients_mutex);
            assigned_ip = clients[client_index].assigned_ip;
            pthread_mutex_unlock(&clients_mutex); // Desbloquear el mutex

            // Verificar que la IP solicitada coincide con la asignada
            if (strcmp(requested_ip, assigned_ip) == 0) {
                snprintf(send_buffer, sizeof(send_buffer),
                         "DHCPACK IP: %s NETMASK: %s GATEWAY: %s DNS: %s LEASE: %d",
                         assigned_ip, NETMASK, GATEWAY, DNS_SERVER, LEASE_TIME);

                int send_status = sendto(sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&relay_addr, addr_len);
                if (send_status == -1) {
                    perror("Error al enviar DHCPACK");
                } else {
                    printf("DHCPACK enviado con IP: %s al cliente %s\n", assigned_ip, client_id);
                }
            } else {
                printf("Error: IP solicitada (%s) no coincide con la asignada (%s).\n", requested_ip, assigned_ip);
                // Enviar DHCPNAK opcionalmente
                snprintf(send_buffer, sizeof(send_buffer), "DHCPNAK");
                sendto(sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&relay_addr, addr_len);
            }
        } else {
            printf("Error: Cliente %s no encontrado para el DHCPREQUEST.\n", client_id);
            // Enviar DHCPNAK opcionalmente
            snprintf(send_buffer, sizeof(send_buffer), "DHCPNAK");
            sendto(sock, send_buffer, strlen(send_buffer), 0, (struct sockaddr*)&relay_addr, addr_len);
        }
    }

    // Mostrar la tabla de IPs asignadas después de procesar cada solicitud
    mostrar_tabla_ips();

cleanup:
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
            if (client_data == NULL) {
                perror("Error al asignar memoria para client_data");
                continue;
            }
            strncpy(client_data->buffer, buffer, bytes_received);
            client_data->buffer[bytes_received] = '\0';
            client_data->bytes_received = bytes_received;
            client_data->client_addr = client_addr;
            client_data->addr_len = addr_len;
            client_data->sock = sock;

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
    pthread_mutex_destroy(&clients_mutex);

    return 0;
}

// Tabla de IPs Asignadas y Direcciones MAC
void mostrar_tabla_ips() {
    printf("\nIPs Asignadas\n");
    printf("------------------\n");
    for (int i = 0; i < client_count; i++) {
        printf("%s\n", clients[i].assigned_ip);
    }
}
