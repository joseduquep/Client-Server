import socket
from colorama import init, Fore
init()

# IP y puerto del servidor DHCP
SERVER_IP = "127.0.0.1"  # Cambia esto si el servidor está en otra máquina
SERVER_PORT = 67

# Función para crear el socket de cliente DHCP
def create_dhcp_socket():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # UDP
    sock.settimeout(10)  # Timeout de 10 segundos para recibir datos
    return sock

# Función para enviar un mensaje DHCPDISCOVER
def send_discover(sock):
    message = "DHCPDISCOVER"
    sock.sendto(message.encode(), (SERVER_IP, SERVER_PORT))
    print(Fore.YELLOW + "Enviado DHCPDISCOVER")

# Función para recibir la oferta (DHCPOFFER) del servidor
def receive_offer(sock):
    try:
        response, _ = sock.recvfrom(1024)
        response = response.decode().split()
        print(Fore.BLUE + "Respuesta recibida:", response)

        if len(response) < 2:
            print("Error: Respuesta incompleta")
            return None, None, None, None

        offered_ip = response[1]
        if offered_ip:
            print(Fore.GREEN + f"IP ofrecida: {offered_ip}")
        else:
            print("No se recibió una oferta válida")
        
        mask = response[2] if len(response) > 2 else "255.255.255.0"
        gateway = response[3] if len(response) > 3 else "192.168.1.1"
        dns = response[4] if len(response) > 4 else "8.8.8.8"
        return offered_ip, mask, gateway, dns

    except socket.timeout:
        print("Error: No se recibió ninguna respuesta del servidor.")
        return None, None, None, None

# Función para enviar un mensaje DHCPREQUEST
def send_request(sock, offered_ip):
    message = f"DHCPREQUEST {offered_ip}"
    sock.sendto(message.encode(), (SERVER_IP, SERVER_PORT))
    print(Fore.YELLOW + f"Enviado DHCPREQUEST para la IP: {offered_ip}")

# Función para enviar un mensaje DHCPDECLINE
def send_decline(sock, declined_ip):
    message = f"DHCPDECLINE {declined_ip}"
    sock.sendto(message.encode(), (SERVER_IP, SERVER_PORT))
    print(Fore.RED + f"Enviado DHCPDECLINE para la IP: {declined_ip}")

# Función para enviar un mensaje DHCPRELEASE
def send_release(sock, leased_ip):
    message = f"DHCPRELEASE {leased_ip}"
    sock.sendto(message.encode(), (SERVER_IP, SERVER_PORT))
    print(Fore.CYAN + f"Enviado DHCPRELEASE para la IP: {leased_ip}")

# Función para manejar la respuesta DHCPACK o DHCPNAK
def handle_ack_or_nak(response):
    if "DHCPNAK" in response:
        print(Fore.RED + "Recibido DHCPNAK, la solicitud de IP fue rechazada.")
    elif "DHCPACK" in response:
        print(Fore.GREEN + f"Recibido DHCPACK, la IP: {response[8:len(response)]} fue asignada exitosamente.")

# Flujo principal de ejecución del cliente DHCP
def main():
    sock = create_dhcp_socket()
    send_discover(sock)
    offered_ip, mask, gateway, dns = receive_offer(sock)

    if offered_ip:
        send_request(sock, offered_ip)
        try:
            response, _ = sock.recvfrom(1024)
            response = response.decode()
            handle_ack_or_nak(response)
            parameters = f"""Los parametros asignados fueron Response: {response}, Mask: {mask}, Gateway: {gateway}, DNS: {dns}"""
            print(Fore.YELLOW + parameters)
        except socket.timeout:
            print("Error: No se recibió ninguna respuesta del servidor.")

    # Liberar la IP (opcional)
    # send_release(sock, offered_ip)

    sock.close()

if __name__ == "__main__":
    main()
