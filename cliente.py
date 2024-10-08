import socket
from colorama import init, Fore, Back, Style
init()

# IP y puerto del servidor DHCP
SERVER_IP = "127.0.0.1"  # Cambia esto si el servidor está en otra máquina
SERVER_PORT = 67

# Función para crear el socket de cliente DHCP
def create_dhcp_socket():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)  # UDP
    sock.settimeout(10)  # Timeout de 5 segundos para recibir datos
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
        print(Fore.BLUE + "Respuesta recibida:", response)  # Imprime la respuesta recibida

        if len(response) < 2:
            print("Error: Respuesta incompleta")
            return None, None, None, None

        offered_ip = response[1]
        if offered_ip:
            print(Fore.GREEN +f"IP ofrecida: {offered_ip}")
            send_request(sock, offered_ip)
        else:
            print("No se recibió una oferta válida")
        # Si faltan otros campos, los llenamos con valores por defecto o vacíos
        mask = response[2] if len(response) > 2 else "255.255.255.0"  # Valor por defecto
        gateway = response[3] if len(response) > 3 else "192.168.1.1"  # Valor por defecto
        dns = response[4] if len(response) > 4 else "8.8.8.8"  # Valor por defecto
        return offered_ip, mask, gateway, dns

    except socket.timeout:
        print("Error: No se recibió ninguna respuesta del servidor.")
        return None, None, None, None

# Función para enviar un mensaje DHCPREQUEST
def send_request(sock, offered_ip):
    message = Fore.YELLOW + f"DHCPREQUEST {offered_ip}"
    sock.sendto(message.encode(), (SERVER_IP, SERVER_PORT))
    print(Fore.RED + f"DHCPREQUEST enviado para la IP: {offered_ip}")


# Función para recibir la confirmación (DHCPACK) del servidor
def receive_ack(sock):
    try:
        response, _ = sock.recvfrom(1024)
        print(Fore.BLUE + f"Respuesta DHCPACK recibida (raw): {response}")  # Agregar este print
        response = response.decode().split()
        print(f"Respuesta DHCPACK decodificada: {response}")  # Agregar este print
        if response[0] == "DHCPACK":
            assigned_ip = response[1]
            print(Fore.GREEN + f"DHCPACK recibido: {assigned_ip}")
            return assigned_ip
        else:
            print("Error: No se recibió un DHCPACK válido.")
            return None
    except socket.timeout:
        print("Error: No se recibió una respuesta DHCPACK.")
        return None


# Función para enviar un mensaje DHCPRELEASE
def dhcp_release(sock, assigned_ip):
    message = f"DHCPRELEASE {assigned_ip}"
    sock.sendto(message.encode(), (SERVER_IP, SERVER_PORT))
    print(f"Enviado DHCPRELEASE para la IP: {assigned_ip}")

# Función principal
def main():
    sock = create_dhcp_socket()
    send_discover(sock)

    try:
        offered_ip, mask, gateway, dns = receive_offer(sock)
        if not offered_ip:  # Si la oferta no es válida
            print("No se recibió una oferta válida.")
            return
        
        print(Fore.RED + f"IP Ofrecida: {offered_ip}, Máscara: {mask}, Gateway: {gateway}, DNS: {dns}")

        send_request(sock, offered_ip)
        assigned_ip = receive_ack(sock)

        if assigned_ip:
            print(Fore.GREEN +f"Dirección IP asignada: {assigned_ip}")
        
    except Exception as e:
        print(f"Error durante la comunicación DHCP: {e}")
    
    finally:
        # Asegúrate de liberar la IP solo si fue asignada
        if 'assigned_ip' in locals() and assigned_ip:
            dhcp_release(sock, assigned_ip)
        sock.close()

if __name__ == "__main__":
    main()
