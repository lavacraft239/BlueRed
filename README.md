# BlueRed

BlueRed es un programa en C++ para compartir archivos entre dispositivos conectados en la misma red WiFi, sin necesidad de permisos root ni configuraciones complicadas.  
Funciona con un servidor y un cliente que se comunican para enviar y recibir archivos de forma simple y rápida.

---

## ¿Qué hace BlueRed?

- Escanea la red local para detectar dispositivos disponibles.  
- Permite elegir un dispositivo para enviar archivos.  
- Envía uno o varios archivos al dispositivo seleccionado.  
- El servidor recibe los archivos y los guarda en la carpeta donde se ejecuta.  
- No necesita root ni permisos especiales.  
- Funciona en Termux, Linux, y dispositivos Android con soporte C++.

---

## Archivos principales

- `server` (o `blueredS.cpp`): programa servidor que recibe archivos.  
- `client` (o `blueredC.cpp`): programa cliente que escanea la red y envía archivos.

---

## Cómo compilar

Usa g++ con los siguientes flags para ambos archivos:

```bash
g++ blueredS.cpp -o server -pthread
g++ blueredC.cpp -o client -pthread
```
Se quedará escuchando en el puerto 9000 esperando conexiones entrantes.

Cliente

Ejecutar el cliente en el dispositivo que enviará archivos:
```bash
./client
```
El cliente escanea la red local y muestra los dispositivos encontrados.
Elegí el dispositivo destino escribiendo el número correspondiente.
Luego ingresá los nombres de los archivos que querés enviar, separados por espacios.

Ejemplo:

Ingresá nombres de archivos a enviar (separados por espacios): archivo1.txt imagen.png

El cliente enviará cada archivo y el servidor los recibirá y guardará en la carpeta donde está corriendo.


---

Estado actual

Importante: Actualmente hay problemas para guardar los archivos correctamente en el servidor.
Algunos archivos se reciben pero no se guardan bien o se pierden.
Se recomienda revisar el código para corregir este problema.


---

Detalles técnicos

Comunicación basada en TCP por el puerto 9000.

El cliente primero envía la cantidad de archivos, luego el nombre y tamaño de cada uno, seguido por el contenido.

El servidor recibe esa información y guarda cada archivo con su nombre original.

El escaneo de red se hace con pings para detectar dispositivos activos (solo en la misma red local).

Los archivos se envían en modo binario para evitar corrupción.

La transferencia incluye mensajes de progreso y confirmación para cada archivo.



---

Problemas comunes

Asegurate que tanto cliente como servidor estén en la misma red WiFi.

Si el archivo se envía pero no aparece, revisá que el servidor esté guardando en la carpeta correcta.

Evitar enviar archivos vacíos (0 bytes).

Verificar que no haya firewall bloqueando el puerto 9000.



---

Futuras mejoras

Soporte para transferencia multihilo y más rápida.

Transferencia segura con cifrado.

Interfaz gráfica para facilitar el uso.



---

Autor

BlueRed fue creado por Santiago Manzolillo, con ganas de facilitar el intercambio de archivos en redes locales sin complicaciones.


---

Licencia

MIT License - Usalo libremente para aprender y compartir.


---

¡Gracias por usar BlueRed!
Si querés ayudar o sugerir mejoras, podés contactarme o hacer un fork en GitHub.

Querés que te ayude con un script o instrucciones para hacer que guarde mejor los archivos?

