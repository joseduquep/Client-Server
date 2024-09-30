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
void* handle_client(int sock) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[1024];
    int bytes_received;

    // Escuchar mensaje DHCPDISCOVER
    bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
    if (bytes_received > 0) {
        printf("Solicitud DHCPDISCOVER recibida del cliente %s\n", inet_ntoa(client_addr.sin_addr));

        // Asignar IP
        char* assigned_ip = assign_ip();
        if (assigned_ip == NULL) {
            printf("No hay más direcciones IP disponibles\n");
            close(sock);
            return NULL;
        }

        // Responder con DHCPOFFER
        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "DHCPOFFER %s", assigned_ip);
        sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_len);
        printf("Enviando DHCPOFFER con IP: %s al cliente %s\n", assigned_ip, inet_ntoa(client_addr.sin_addr));

        // Recibir DHCPREQUEST
        bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &addr_len);
        if (bytes_received > 0) {
            printf("Solicitud DHCPREQUEST recibida del cliente %s\n", inet_ntoa(client_addr.sin_addr));

            // Confirmar con DHCPACK
            memset(buffer, 0, sizeof(buffer));
            snprintf(buffer, sizeof(buffer), "DHCPACK %s", assigned_ip);
            sendto(sock, buffer, strlen(buffer), 0, (struct sockaddr*)&client_addr, addr_len);
            printf("DHCPACK enviado al cliente %s con IP: %s\n", inet_ntoa(client_addr.sin_addr), assigned_ip);

            // Registrar el cliente
            dhcp_client new_client;
            new_client.client_addr = client_addr;
            strncpy(new_client.assigned_ip, assigned_ip, 16);
            new_client.lease_expiry = time(NULL) + LEASE_TIME;

            clients[client_count++] = new_client;
            printf("Cliente %s registrado con la IP %s\n", inet_ntoa(client_addr.sin_addr), assigned_ip);
        } else {
            printf("No se recibió DHCPREQUEST del cliente %s\n", inet_ntoa(client_addr.sin_addr));
        }
    } else {
        printf("No se recibió DHCPDISCOVER\n");
    }
    
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

    // Bucle principal para manejar solicitudes de clientes
    while (1) {
        handle_client(sock);  // Ya no crea un nuevo hilo, maneja las solicitudes en el mismo proceso
    }

    close(sock);
    return 0;
}
