import socket
from colorama import init, Fore
init()

# IP y puerto del servidor DHCP
SERVER_IP = "172.22.116.245"  # Cambia esto si el servidor está en otra máquina
SERVER_PORT = 67
CLIENT_PORT = 68

CLIENT_ID = "Client8"  # Identificador único del cliente

# Función para crear el socket de cliente DHCP
def create_dhcp_socket():
    print(Fore.MAGENTA + "Creando socket DHCP cliente...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # UDP
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(('', CLIENT_PORT))  # Vincular al puerto 68
    except PermissionError:
        print(Fore.RED + "Error: Permiso denegado al intentar vincular al puerto 68. Ejecuta el cliente con permisos adecuados.")
        exit(1)
    sock.settimeout(10)  # Timeout de 10 segundos para recibir datos
    print(Fore.MAGENTA + "Socket creado y configurado con timeout de 10 segundos.")
    return sock

# Función para enviar un mensaje DHCPDISCOVER
def send_discover(sock):
    print(Fore.MAGENTA + "Preparando para enviar DHCPDISCOVER...")
    message = f"DHCPDISCOVER CLIENT_ID: {CLIENT_ID}"
    print(Fore.BLUE + f"Mensaje DHCPDISCOVER a enviar: '{message}'")
    sock.sendto(message.encode(), (SERVER_IP, SERVER_PORT))
    print(Fore.YELLOW + "DHCPDISCOVER enviado al servidor.")

# Función para recibir la oferta (DHCPOFFER) del servidor
def receive_offer(sock):
    print(Fore.MAGENTA + "Esperando DHCPOFFER del servidor...")
    try:
        response, _ = sock.recvfrom(1024)
        print(Fore.MAGENTA + "Respuesta recibida del servidor.")
        response_decoded = response.decode()
        print(Fore.BLUE + f"Respuesta decodificada: '{response_decoded}'")

        # Parsing más robusto
        if response_decoded.startswith("DHCPOFFER IP:"):
            parts = response_decoded.split()
            if len(parts) >= 3 and parts[0] == "DHCPOFFER" and parts[1] == "IP:":
                offered_ip = parts[2]
                print(Fore.GREEN + f"IP ofrecida por el servidor: {offered_ip}")
                return offered_ip
            else:
                print(Fore.RED + "Error: Formato de respuesta DHCPOFFER inesperado.")
                return None
        else:
            print(Fore.RED + "Error: Respuesta no es DHCPOFFER.")
            return None

    except socket.timeout:
        print(Fore.RED + "Error: No se recibió ninguna respuesta del servidor.")
        return None

# Función para enviar un mensaje DHCPREQUEST
def send_request(sock, offered_ip):
    print(Fore.MAGENTA + "Preparando para enviar DHCPREQUEST...")
    message = f"DHCPREQUEST IP: {offered_ip} CLIENT_ID: {CLIENT_ID}"
    print(Fore.BLUE + f"Mensaje DHCPREQUEST a enviar: '{message}'")
    sock.sendto(message.encode(), (SERVER_IP, SERVER_PORT))
    print(Fore.YELLOW + f"DHCPREQUEST enviado para la IP ofrecida: {offered_ip}")

# Función para manejar la respuesta DHCPACK o DHCPNAK
def handle_ack_or_nak(response):
    print(Fore.MAGENTA + "Procesando respuesta del servidor (ACK o NAK)...")
    if "DHCPNAK" in response:
        print(Fore.RED + "Recibido DHCPNAK, la solicitud de IP fue rechazada.")
    elif "DHCPACK" in response:
        ack_response = response.split()
        if len(ack_response) >= 3 and ack_response[1] == "IP:":
            assigned_ip = ack_response[2]
            print(Fore.GREEN + f"Recibido DHCPACK, la IP {assigned_ip} fue asignada exitosamente.")
        else:
            print(Fore.RED + "Error: DHCPACK recibido sin una IP válida.")
    else:
        print(Fore.RED + "Error: Respuesta inesperada del servidor.")

# Flujo principal de ejecución del cliente DHCP
def main():
    print(Fore.MAGENTA + "Iniciando cliente DHCP...")
    sock = create_dhcp_socket()
    send_discover(sock)
    
    offered_ip = receive_offer(sock)
    print(Fore.CYAN + "Verificación después de recibir DHCPOFFER.")
    print(Fore.CYAN + f"IP ofrecida recibida: {offered_ip}")

    print("Ahora sigue enviar la solicitud DHCPREQUEST.")
    if offered_ip:
        send_request(sock, offered_ip)
        # Después de enviar el DHCPREQUEST, agrega mensajes de depuración:
        print(Fore.CYAN + "Esperando respuesta del servidor (DHCPACK o DHCPNAK)...")

        try:
            response, _ = sock.recvfrom(1024)
            print(Fore.MAGENTA + "Respuesta recibida del servidor.")
            response = response.decode()  # Recibe y decodifica la respuesta
            print(f"Respuesta recibida del servidor: '{response}'")
            handle_ack_or_nak(response)
        except socket.timeout:
            print(Fore.RED + "Error: No se recibió ninguna respuesta del servidor después de enviar DHCPREQUEST.")
    else:
        print(Fore.RED + "No se recibió ninguna IP ofrecida. No se envió el DHCPREQUEST.")

    sock.close()
    print(Fore.MAGENTA + "Socket cerrado. Cliente DHCP finalizado.")

if __name__ == "__main__":
    main()
