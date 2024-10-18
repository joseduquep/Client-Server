# DHCP-Server
## Índice
- [Introducción](#introducción)
- [Desarrollo](#desarrollo)
- [Aspectos Logrados y No logrados](#aspectos-logrados-y-no-logrados)
- [Conclusiones](#conclusiones)
- [Referencias](#referencias)

## Introducción
Este proyecto tiene como objetivo la creación de un servidor y un cliente DHCP funcionales utilizando la API de Berkeley para el manejo de sockets. El servidor está implementado en C y es capaz de gestionar la asignación dinámica de direcciones IP a múltiples clientes dentro de una red local o remota. El cliente está implementado en Python y puede solicitar una dirección IP, recibir la oferta, confirmar la asignación y manejar la concesión de la misma (lease).

El proyecto incluye funcionalidades esenciales del protocolo DHCP, como el manejo de solicitudes DHCPDISCOVER, DHCPOFFER, DHCPREQUEST y DHCPACK, además de ofrecer soporte para múltiples clientes concurrentes a través del uso de hilos en el servidor.

## Desarrollo
### Funcionalidades del Servidor
  1. Inicialización del pool de IPs: El servidor gestiona un conjunto de direcciones IP configuradas a partir de la dirección inicial 192.168.1.100, generando hasta 100 direcciones únicas.

  2. Gestión de solicitudes DHCP:
  - DHCPDISCOVER: El servidor escucha en el puerto 67 y, al recibir una solicitud DHCPDISCOVER, asigna una dirección IP disponible del pool y envía una oferta DHCPOFFER al cliente.
  - DHCPREQUEST: Al recibir una solicitud DHCPREQUEST, el servidor verifica que la IP solicitada corresponda a la previamente ofrecida y, de ser así, envía una confirmación DHCPACK.
  - Gestión de IPs concurrentes: Utilizando hilos, el servidor puede manejar múltiples clientes simultáneamente, lo que permite atender varias solicitudes al mismo tiempo.
  
  3. Asignación de direcciones IP: Las direcciones IP se asignan dinámicamente desde el pool, y cada cliente tiene una dirección asociada hasta que el tiempo de arrendamiento expira o se libera la IP.

  4. Tabla de IPs asignadas: Después de procesar cada solicitud, el servidor muestra una tabla con todas las direcciones IP asignadas y los detalles correspondientes.

  5. Manejo de errores: El servidor es capaz de manejar solicitudes inválidas, devolviendo un mensaje DHCPNAK en caso de que una solicitud DHCPREQUEST no pueda ser procesada o la IP solicitada no esté disponible.

### Funcionalidades del Cliente
  1. Solicitud de dirección IP:
    - El cliente envía un mensaje DHCPDISCOVER al servidor especificando un CLIENT_ID único.
    - Después de recibir un DHCPOFFER, el cliente envía un DHCPREQUEST solicitando la IP ofrecida.
  
  2. Recepción de parámetros de red:
    - El cliente recibe la IP, máscara de subred, puerta de enlace, servidor DNS y otros parámetros del servidor tras la confirmación con DHCPACK.
    - En caso de que el servidor rechace la solicitud, se maneja el mensaje DHCPNAK.
  
  3. Manejo de tiempo de espera: El cliente está configurado con un timeout de 10 segundos para esperar respuestas del servidor, manejando adecuadamente la situación en caso de que no se reciba respuesta.

  4. Cierre y liberación de recursos: Una vez que el cliente ha terminado de interactuar con el servidor, cierra el socket y finaliza la ejecución.

### Diagrama del Flujo de Trabajo
  1. El cliente envía un DHCPDISCOVER al servidor.
  2. El servidor responde con un DHCPOFFER ofreciendo una dirección IP.
  3. El cliente envía un DHCPREQUEST para solicitar la IP ofrecida.
  4. El servidor confirma la asignación enviando un DHCPACK o rechaza la solicitud con un DHCPNAK.
  5. El cliente maneja la IP asignada y gestiona su uso mientras sea válida.

### Hilos en el Servidor
Para manejar las solicitudes de múltiples clientes al mismo tiempo, el servidor utiliza hilos. Cada vez que se recibe una solicitud, se crea un nuevo hilo para procesarla sin bloquear la ejecución del servidor. Se implementan mecanismos de protección con mutexes para garantizar que los datos compartidos, como la tabla de clientes y el pool de IPs, sean accedidos de manera segura.

### Pruebas y Despliegue
El servidor y el cliente fueron probados tanto en entornos locales como en la nube (usando AWS EC2). Se realizaron pruebas para verificar la asignación de IPs, la gestión del tiempo de arrendamiento y el manejo concurrente de múltiples clientes. El sistema funcionó correctamente bajo diferentes condiciones de red, y se verificó que el servidor pudiera manejar situaciones de errores y tiempos de espera.

## Funcionamiento e implementación del código
### Instalación de WSL
1. Habilitar WSL
- Abre el Símbolo del sistema o PowerShell como administrador y ejecuta el siguiente comando para habilitar WSL

`wsl --install`

- Esto instalará WSL y descargará Ubuntu.

2. Reiniciar el Sistema
Una vez que tu sistema se reinicie, abre la aplicación de Ubuntu. Se te pedirá que configures un nombre de usuario y una contraseña para tu entorno de Linux.

### Librerías y Elementos a Instalar
1. Instalar el Compilador de C
-Abrir la terminal de WSL y ejecutar:

`sudo apt-get update`
`sudo apt-get install build-essential`

2. Instalar Python
- En la terminal de WSL, instalar Python y pip ejecutando

`sudo apt-get install python3 python3-pip`

### Pasos Generales para Ejecutar
1. Compilar el Servidor

`gcc servidor.c -o serverexe`

2. Ejecutar el Servidor

`sudo ./servidor`

3. Ejecutar el Cliente

`python3 cliente.py`

## Diagrama

![Servidor - Cliente Dhcp drawio](https://github.com/user-attachments/assets/9f49e5c1-0605-44bc-b6d6-87612bd05ab8)


## Aspectos Logrados y No Logrados
### Aspectos Logrados
  - Implementación completa del servidor DHCP en C que gestiona solicitudes de múltiples clientes.
  - Correcta gestión de direcciones IP, tiempos de arrendamiento y liberación de IPs.
  - Cliente DHCP funcional en Python, capaz de solicitar y recibir direcciones IP.
  - Soporte para múltiples clientes concurrentes con manejo seguro de datos compartidos usando hilos y mutexes.
  - Manejo adecuado de errores como la recepción de solicitudes incorrectas o la falta de IPs disponibles.
### Aspectos No Logrados
  - No se implementaron mecanismos de seguridad como autenticación o cifrado en la comunicación.
  - No se incluyeron políticas para liberar automáticamente las IPs inactivas o extender el pool en caso de agotamiento.
  - A pesar de contar con el relay, no se probó el manejo de configuraciones de red más complejas como múltiples gateways.
  - Los eventos solo se registran en consola, sin un sistema de logs persistentes para auditoría.
  - No se implementó una renovación automática de las IPs cuando el lease está por expirar.
  - No se realizaron pruebas exhaustivas de escalabilidad bajo una alta carga de clientes.
  
## Conclusiones
Este proyecto permitió comprender y aplicar el protocolo DHCP, así como utilizar sockets UDP para la comunicación eficiente entre cliente y servidor. La implementación concurrente del servidor mediante hilos resultó eficaz para manejar múltiples solicitudes simultáneas, garantizando un buen rendimiento y evitando bloqueos. El uso de mutexes fue clave para la sincronización en el acceso a recursos compartidos, como el pool de direcciones IP.

El cliente en Python interactuó correctamente con el servidor, gestionando la solicitud, recepción y liberación de direcciones IP de manera fluida. Las pruebas confirmaron el correcto funcionamiento tanto en entornos locales como en la nube. Aunque se cumplieron los objetivos principales, queda espacio para mejorar en aspectos como la seguridad y la gestión de configuraciones de red más avanzadas.

## Referencias
# Recursos importantes

- [API de Berkeley Sockets](https://en.wikipedia.org/wiki/Berkeley_sockets)
- [Documentación de Microsoft sobre el servidor DHCP](https://docs.microsoft.com/en-us/windows-server/networking/technologies/dhcp/dhcp-top)
- [Fundamentos de DHCP](https://www.networkworld.com/article/3239896/what-is-dhcp-and-how-does-it-work.html)
- [RFC 3046: Agente de Relay DHCP](https://datatracker.ietf.org/doc/html/rfc3046)
- [Chat GPT](https://chat.openai.com)

