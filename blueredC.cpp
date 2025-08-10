#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <future>
#include <chrono>
#include <cstring>
#include <sstream>
#include <fstream>
#include <algorithm>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <net/if.h>

#define PORT 9000
#define BUFFER_SIZE 8192
#define TIMEOUT_MS 300

std::mutex mtx;
std::vector<std::string> dispositivosEncontrados;

// Obtener IP local y máscara para definir rango
bool obtenerIPyMascara(std::string& ipLocal, std::string& maskLocal) {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) return false;

    bool ok = false;
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            char addr[INET_ADDRSTRLEN];
            void* ptr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, ptr, addr, INET_ADDRSTRLEN);

            char mask[INET_ADDRSTRLEN];
            void* ptrm = &((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
            inet_ntop(AF_INET, ptrm, mask, INET_ADDRSTRLEN);

            // Ignorar loopback
            if (std::string(addr) != "127.0.0.1") {
                ipLocal = std::string(addr);
                maskLocal = std::string(mask);
                ok = true;
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
    return ok;
}

// Convertir IP string a uint32_t
uint32_t ipToInt(const std::string& ip) {
    struct in_addr inaddr;
    inet_pton(AF_INET, ip.c_str(), &inaddr);
    return ntohl(inaddr.s_addr);
}

// Convertir uint32_t a IP string
std::string intToIP(uint32_t ipInt) {
    struct in_addr inaddr;
    inaddr.s_addr = htonl(ipInt);
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &inaddr, buf, INET_ADDRSTRLEN);
    return std::string(buf);
}

// Calcular rango de IPs basado en IP y máscara
void calcularRangoIPs(const std::string& ipLocal, const std::string& maskLocal, uint32_t& startIP, uint32_t& endIP) {
    uint32_t ipInt = ipToInt(ipLocal);
    uint32_t maskInt = ipToInt(maskLocal);

    startIP = (ipInt & maskInt) + 1;
    endIP = (ipInt | (~maskInt)) - 1;
}

// Intentar conectar a IP:PORT para ver si servidor responde
bool probarConexion(const std::string& ip) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    struct sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr);

    // Setear timeout corto
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = TIMEOUT_MS * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    bool conectado = (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0);
    close(sock);
    return conectado;
}

// Escaneo paralelo IPs en rango para detectar servidores
void escanearRango(uint32_t startIP, uint32_t endIP) {
    std::vector<std::future<void>> futures;

    for (uint32_t ip = startIP; ip <= endIP; ++ip) {
        futures.push_back(std::async(std::launch::async, [ip]() {
            std::string ipStr = intToIP(ip);
            if (probarConexion(ipStr)) {
                std::lock_guard<std::mutex> lock(mtx);
                dispositivosEncontrados.push_back(ipStr);
                std::cout << "📡 Dispositivo encontrado: " << ipStr << "\n";
            }
        }));
    }

    for (auto& f : futures) {
        f.get();
    }
}

bool enviarTodo(int sockfd, const char* buffer, size_t length) {
    size_t total = 0;
    ssize_t sent;
    while (total < length) {
        sent = send(sockfd, buffer + total, length - total, 0);
        if (sent == -1) return false;
        total += sent;
    }
    return true;
}

bool enviarString(int sockfd, const std::string& str) {
    uint32_t len = htonl(str.size());
    if (!enviarTodo(sockfd, (char*)&len, sizeof(len))) return false;
    if (!enviarTodo(sockfd, str.c_str(), str.size())) return false;
    return true;
}

bool enviarUint64(int sockfd, uint64_t valor) {
    uint64_t val_net = htobe64(valor);
    return enviarTodo(sockfd, (char*)&val_net, sizeof(val_net));
}

bool enviarArchivo(int sockfd, const std::string& rutaArchivo) {
    std::ifstream archivo(rutaArchivo, std::ios::binary);
    if (!archivo.is_open()) {
        std::cerr << "❌ No se puede abrir el archivo: " << rutaArchivo << "\n";
        return false;
    }

    archivo.seekg(0, std::ios::end);
    size_t file_size = archivo.tellg();
    archivo.seekg(0, std::ios::beg);

    if (!enviarUint64(sockfd, file_size)) {
        std::cerr << "❌ Error al enviar tamaño archivo\n";
        archivo.close();
        return false;
    }

    char buffer[BUFFER_SIZE];
    size_t totalEnviado = 0;

    while (!archivo.eof()) {
        archivo.read(buffer, BUFFER_SIZE);
        int bytesLeidos = archivo.gcount();

        if (!enviarTodo(sockfd, buffer, bytesLeidos)) {
            std::cerr << "❌ Error al enviar datos\n";
            archivo.close();
            return false;
        }

        totalEnviado += bytesLeidos;
        float progreso = (totalEnviado * 100.0f) / file_size;
        std::cout << "\r📤 Enviando archivo... " << progreso << "% (" << totalEnviado << "/" << file_size << " bytes)";
        std::cout.flush();
    }

    std::cout << "\n✅ Archivo enviado: " << rutaArchivo << "\n";
    archivo.close();
    return true;
}

int main() {
    std::string ipLocal, maskLocal;
    if (!obtenerIPyMascara(ipLocal, maskLocal)) {
        std::cerr << "❌ No se pudo obtener IP y máscara local\n";
        return 1;
    }

    uint32_t startIP, endIP;
    calcularRangoIPs(ipLocal, maskLocal, startIP, endIP);

    std::cout << "🔎 Escaneando red local desde " << intToIP(startIP) << " hasta " << intToIP(endIP) << "\n";

    escanearRango(startIP, endIP);

    if (dispositivosEncontrados.empty()) {
        std::cout << "❌ No se encontraron dispositivos con servidor activo en puerto " << PORT << "\n";
        return 1;
    }

    std::cout << "\nDispositivos disponibles para enviar archivos:\n";
    for (size_t i = 0; i < dispositivosEncontrados.size(); ++i) {
        std::cout << " [" << (i+1) << "] " << dispositivosEncontrados[i] << "\n";
    }

    int elegido = 0;
    while (true) {
        std::cout << "Elegí dispositivo (1-" << dispositivosEncontrados.size() << "): ";
        std::cin >> elegido;
        if (elegido >= 1 && elegido <= (int)dispositivosEncontrados.size()) break;
        std::cout << "Número inválido, probá de nuevo.\n";
    }

    std::string ipDestino = dispositivosEncontrados[elegido - 1];

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "❌ Error al crear socket\n";
        return 1;
    }

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ipDestino.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "❌ IP inválida\n";
        close(sock);
        return 1;
    }

    std::cout << "Conectando a " << ipDestino << "...\n";
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "❌ Conexión fallida\n";
        close(sock);
        return 1;
    }

    std::cout << "Ingresá nombres de archivos a enviar (separados por espacios):\n";
    std::cin.ignore();
    std::string linea;
    std::getline(std::cin, linea);

    std::istringstream iss(linea);
    std::vector<std::string> archivosEnviar;
    std::string token;
    while (iss >> token) archivosEnviar.push_back(token);

    if (archivosEnviar.empty()) {
        std::cerr << "⚠️ No ingresaste archivos.\n";
        close(sock);
        return 1;
    }

    uint32_t cantArchivos = archivosEnviar.size();
    uint32_t cantNet = htonl(cantArchivos);
    if (!enviarTodo(sock, (char*)&cantNet, sizeof(cantNet))) {
        std::cerr << "❌ Error al enviar cantidad de archivos\n";
        close(sock);
        return 1;
    }

    for (const auto& archivo : archivosEnviar) {
        std::cout << "🔄 Enviando archivo: " << archivo << "\n";
        if (!enviarString(sock, archivo)) {
            std::cerr << "❌ Error al enviar nombre de archivo\n";
            close(sock);
            return 1;
        }
        if (!enviarArchivo(sock, archivo)) {
            std::cerr << "❌ Error al enviar archivo: " << archivo << "\n";
            close(sock);
            return 1;
        }
    }

    std::cout << "✅ Todos los archivos enviados.\n";
    close(sock);
    return 0;
}
