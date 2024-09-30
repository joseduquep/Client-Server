#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_PORT 67
#define MAX_CLIENTS 100
#define IP_POOL_START "192.168.1.100"
#define IP_POOL_END "192.168.1.150"
#define LEASE_TIME 3600 // 1 hora en segundos

// Estructura para manejar clientes
typedef struct {
    struct sockaddr_in client_addr;
    char assigned_ip[16];
    time_t lease_expiry;
} dhcp_client;

// Array de clientes
dhcp_client clients[MAX_CLIENTS];
int client_count = 0;

// Pool de IPs disponibles
int ip_pool_index = 0;
char ip_pool[MAX_CLIENTS][16];

// Inicializar el pool de IPs
void initialize_ip_pool() {
    struct in_addr addr;
    inet_aton(IP_POOL_START, &addr);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        snprintf(ip_pool[i], sizeof(ip_pool[i]), "%s", inet_ntoa(addr));
        addr.s_addr = htonl(ntohl(addr.s_addr) + 1);  // Incrementar la IP
    }
}

// Asignar una dirección IP del pool
char* assign_ip() {
    if (ip_pool_index < MAX_CLIENTS) {
        return ip_pool[ip_pool_index++];
    } else {
        return NULL; // No más IPs disponibles
    }
}

// Hilo para manejar cada cliente
void* handle_client(void* arg) {
    int sock = *((int*) arg);
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[1024];
    int bytes_received;

    // Escuchar mensaje DHCPDISCOVER
    bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
    if (bytes_received > 0) {
        printf("Solicitud DHCPDISCOVER recibida\n");

        // Asignar IP
        char* assigned_ip = assign_ip();
        if (assigned_ip == NULL) {
            printf("No hay más direcciones IP disponibles\n");
            close(sock);
            return NULL;
        }

        // Responder con DHCPOFFER
        printf("Enviando DHCPOFFER con IP: %s\n", assigned_ip);
        snprintf(buffer, sizeof(buffer), "DHCPOFFER %s", assigned_ip);
        sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_len);

        // Recibir DHCPREQUEST
        bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (bytes_received > 0) {
            printf("Solicitud DHCPREQUEST recibida\n");

            // Confirmar con DHCPACK
            snprintf(buffer, sizeof(buffer), "DHCPACK %s", assigned_ip);
            sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_len);
            printf("Asignada IP: %s\n", assigned_ip);

            // Registrar el cliente
            dhcp_client new_client;
            new_client.client_addr = client_addr;
            strncpy(new_client.assigned_ip, assigned_ip, 16);
            new_client.lease_expiry = time(NULL) + LEASE_TIME;

            clients[client_count++] = new_client;
        }
    }

    close(sock);
    return NULL;
}

int main() {
    int sock;
    struct sockaddr_in server_addr;

    // Crear socket UDP
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Error al crear socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Enlazar el socket a la dirección y puerto
    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al enlazar");
        close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Servidor DHCP escuchando en el puerto %d...\n", SERVER_PORT);

    // Inicializar el pool de IPs
    initialize_ip_pool();

    while (1) {
        pthread_t thread_id;
        int client_sock = sock;

        // Crear un nuevo hilo para manejar cada solicitud
        pthread_create(&thread_id, NULL, handle_client, &client_sock);
    }

    close(sock);
    return 0;
}
