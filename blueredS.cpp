#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>

#define PORT 9000
#define BUFFER_SIZE 8192

bool recibirTodo(int sockfd, char* buffer, size_t length) {
    size_t total = 0;
    ssize_t recvd;
    while (total < length) {
        recvd = recv(sockfd, buffer + total, length - total, 0);
        if (recvd <= 0) return false;
        total += recvd;
    }
    return true;
}

bool recibirString(int sockfd, std::string& str) {
    uint32_t len_net;
    if (!recibirTodo(sockfd, (char*)&len_net, sizeof(len_net))) return false;

    uint32_t len = ntohl(len_net);
    if (len == 0) {
        str.clear();
        return true;
    }

    std::vector<char> buffer(len);
    if (!recibirTodo(sockfd, buffer.data(), len)) return false;

    str.assign(buffer.begin(), buffer.end());
    return true;
}

bool recibirUint64(int sockfd, uint64_t& valor) {
    uint64_t val_net;
    if (!recibirTodo(sockfd, (char*)&val_net, sizeof(val_net))) return false;
    valor = be64toh(val_net);
    return true;
}

bool recibirArchivo(int sockfd, const std::string& nombreArchivo) {
    uint64_t tamaño;
    if (!recibirUint64(sockfd, tamaño)) {
        std::cerr << "❌ Error al recibir tamaño de archivo\n";
        return false;
    }

    std::cout << "📥 Recibiendo archivo de " << tamaño << " bytes\n";

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        std::cout << "Guardando archivo en: " << cwd << "/" << nombreArchivo << std::endl;
    } else {
        std::cerr << "No pude obtener el directorio actual\n";
    }

    std::string nombreArchivoUnico = nombreArchivo + "_" + std::to_string(time(NULL));

    std::ofstream archivo(nombreArchivoUnico, std::ios::binary);
    if (!archivo.is_open()) {
        std::cerr << "❌ No puedo abrir el archivo para guardar: " << nombreArchivo << "\n";
        return false;
    }

    char buffer[BUFFER_SIZE];
    uint64_t totalRecibido = 0;
    while (totalRecibido < tamaño) {
        size_t aRecibir = (tamaño - totalRecibido) > BUFFER_SIZE ? BUFFER_SIZE : (tamaño - totalRecibido);
        ssize_t bytes = recv(sockfd, buffer, aRecibir, 0);
        if (bytes <= 0) {
            std::cerr << "❌ Error en recv o conexión cerrada\n";
            archivo.close();
            return false;
        }
        archivo.write(buffer, bytes);
        totalRecibido += bytes;
        
        float progreso = (totalRecibido * 100.0f) / tamaño;
std::cout << "\r📥 Recibiendo archivo... " << int(progreso) << "% (" << totalRecibido << "/" << tamaño << " bytes)" << std::flush;
    }

    archivo.close();
    std::cout << "✅ Archivo guardado: " << nombreArchivo << "\n";
    return true;
}

// Función que recibe archivos con nombre y contenido
bool recibirArchivoCompleto(int sockfd) {
    std::string nombreArchivo;
    if (!recibirString(sockfd, nombreArchivo)) {
        std::cerr << "❌ Error al recibir nombre de archivo\n";
        return false;
    }

    std::cout << "📥 Recibiendo archivo: " << nombreArchivo << "\n";

    uint64_t tamaño;
    if (!recibirUint64(sockfd, tamaño)) {
        std::cerr << "❌ Error al recibir tamaño de archivo\n";
        return false;
    }

    std::ofstream archivo(nombreArchivo, std::ios::binary);
    if (!archivo.is_open()) {
        std::cerr << "❌ No se pudo abrir archivo para guardar\n";
        // Leer el contenido pero descartar para no romper protocolo
        char buffer[BUFFER_SIZE];
        uint64_t totalDescartado = 0;
        while (totalDescartado < tamaño) {
            size_t aRecibir = (tamaño - totalDescartado) > BUFFER_SIZE ? BUFFER_SIZE : (tamaño - totalDescartado);
            ssize_t bytes = recv(sockfd, buffer, aRecibir, 0);
            if (bytes <= 0) break;
            totalDescartado += bytes;
        }
        return false;
    }

    char buffer[BUFFER_SIZE];
    uint64_t totalRecibido = 0;

    while (totalRecibido < tamaño) {
        size_t aRecibir = (tamaño - totalRecibido) > BUFFER_SIZE ? BUFFER_SIZE : (tamaño - totalRecibido);
        ssize_t bytes = recv(sockfd, buffer, aRecibir, 0);
        if (bytes <= 0) {
            std::cerr << "❌ Error en recv o conexión cerrada\n";
            archivo.close();
            return false;
        }
        archivo.write(buffer, bytes);
        totalRecibido += bytes;

        float progreso = (totalRecibido * 100.0f) / tamaño;
        std::cout << "\r📥 Recibiendo archivo... " << progreso << "% (" << totalRecibido << "/" << tamaño << " bytes)";
        std::cout.flush();
    }
    archivo.close();
    std::cout << "\n✅ Archivo guardado: " << nombreArchivo << "\n";
    return true;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address{};
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "❌ Error al crear socket\n";
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "❌ Error al bindear socket\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 3) < 0) {
        std::cerr << "❌ Error al escuchar en socket\n";
        close(server_fd);
        return 1;
    }

    std::cout << "🚀 Servidor escuchando en puerto " << PORT << "\n";

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            std::cerr << "❌ Error en accept\n";
            continue;
        }

        std::cout << "🤝 Cliente conectado\n";

        // Recibir cantidad de archivos (uint32_t)
        uint32_t cantArchivos_net;
        ssize_t recibido = recv(new_socket, &cantArchivos_net, sizeof(cantArchivos_net), MSG_WAITALL);
        if (recibido != sizeof(cantArchivos_net)) {
            std::cerr << "❌ Error al recibir cantidad de archivos\n";
            close(new_socket);
            continue;
        }
        uint32_t cantArchivos = ntohl(cantArchivos_net);
        std::cout << "📁 Cantidad de archivos a recibir: " << cantArchivos << "\n";

        bool ok = true;
        for (uint32_t i = 0; i < cantArchivos; i++) {
            if (!recibirArchivoCompleto(new_socket)) {
                std::cerr << "❌ Error al recibir archivo\n";
                ok = false;
                break;
            }
        }

        if (ok) {
            std::cout << "✅ Todos los archivos recibidos\n";
        }

        close(new_socket);
    }

    close(server_fd);
    return 0;
}
