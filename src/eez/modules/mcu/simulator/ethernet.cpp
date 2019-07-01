/*
 * EEZ Middleware
 * Copyright (C) 2015-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if OPTION_ETHERNET

#include <stdint.h>

#include <eez/system.h>

#include <eez/modules/mcu/ethernet.h>

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
#undef INPUT
#undef OUTPUT

#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock2.h>

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define DebugTrace printf

namespace eez {
namespace mcu {
namespace ethernet {

bool onSystemStateChanged() {
    return true;
}

bool bind(int port);
bool client_available();
bool connected();
int available();
int read(char *buffer, int buffer_size);
int write(const char *buffer, int buffer_size);
void stop();

#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
static SOCKET listen_socket = INVALID_SOCKET;
static SOCKET client_socket = INVALID_SOCKET;
#else
static int listen_socket = -1;
static int client_socket = -1;

bool enable_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    flags = flags | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        return false;
    }
    return true;
}

#endif

bool bind(int port) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    WSADATA wsaData;
    int iResult;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        DebugTrace("EHTERNET: WSAStartup failed with error %d\n", iResult);
        return false;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    char port_str[16];
#ifdef _MSC_VER
    _itoa(port, port_str, 10);
#else
    itoa(port, port_str, 10);
#endif
    iResult = getaddrinfo(NULL, port_str, &hints, &result);
    if (iResult != 0) {
        DebugTrace("EHTERNET: getaddrinfo failed with error %d\n", iResult);
        return false;
    }

    // Create a SOCKET for connecting to server
    listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (listen_socket == INVALID_SOCKET) {
        DebugTrace("EHTERNET: socket failed with error %d\n", WSAGetLastError());
        freeaddrinfo(result);
        return false;
    }

    u_long iMode = 1;
    iResult = ioctlsocket(listen_socket, FIONBIO, &iMode);
    if (iResult != NO_ERROR) {
        DebugTrace("EHTERNET: ioctlsocket failed with error %d\n", iResult);
        freeaddrinfo(result);
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
        return false;
    }

    // Setup the TCP listening socket
    iResult = ::bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        DebugTrace("EHTERNET: bind failed with error %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
        return false;
    }

    freeaddrinfo(result);

    iResult = listen(listen_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        DebugTrace("EHTERNET listen failed with error %d\n", WSAGetLastError());
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
        return false;
    }

    return true;
#else
    sockaddr_in serv_addr;
    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0) {
        DebugTrace("EHTERNET: socket failed with error %d", errno);
        return false;
    }

    if (!enable_non_blocking(listen_socket)) {
        DebugTrace("EHTERNET: ioctl on listen socket failed with error %d", errno);
        close(listen_socket);
        listen_socket = -1;
        return false;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    if (::bind(listen_socket, (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        DebugTrace("EHTERNET: bind failed with error %d", errno);
        close(listen_socket);
        listen_socket = -1;
        return false;
    }

    if (listen(listen_socket, 5) < 0) {
        DebugTrace("EHTERNET: listen failed with error %d", errno);
        close(listen_socket);
        listen_socket = -1;
        return false;
    }

    return true;
#endif    
}

bool client_available() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (connected())
        return true;

    if (listen_socket == INVALID_SOCKET) {
        return false;
    }

    // Accept a client socket
    client_socket = accept(listen_socket, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
        if (WSAGetLastError() == WSAEWOULDBLOCK) {
            return false;
        }

        DebugTrace("EHTERNET accept failed with error %d\n", WSAGetLastError());
        closesocket(listen_socket);
        listen_socket = INVALID_SOCKET;
        return false;
    }

    return true;
#else
    if (connected())
        return true;

    if (listen_socket == -1) {
        return 0;
    }

    sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    client_socket = accept(listen_socket, (sockaddr *)&cli_addr, &clilen);
    if (client_socket < 0) {
        if (errno == EWOULDBLOCK) {
            return false;
        }

        DebugTrace("EHTERNET: accept failed with error %d", errno);
        close(listen_socket);
        listen_socket = -1;
        return false;
    }

    if (!enable_non_blocking(client_socket)) {
        DebugTrace("EHTERNET: ioctl on client socket failed with error %d", errno);
        close(client_socket);
        listen_socket = -1;
        return false;
    }

    return true;
#endif    
}

bool connected() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    return client_socket != INVALID_SOCKET;
#else
    return client_socket != -1;
#endif    
}

int available() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (client_socket == INVALID_SOCKET)
        return 0;

    char buffer[1000];
    int iResult = ::recv(client_socket, buffer, 1000, MSG_PEEK);
    if (iResult > 0) {
        return iResult;
    }

    if (iResult < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
        return 0;
    }

    stop();

    return 0;
#else
    if (client_socket == -1)
        return 0;

    char buffer[1000];
    int iResult = ::recv(client_socket, buffer, 1000, MSG_PEEK);
    if (iResult > 0) {
        return iResult;
    }

    if (iResult < 0 && errno == EWOULDBLOCK) {
        return 0;
    }

    stop();

    return 0;
#endif        
}

int read(char *buffer, int buffer_size) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    int iResult = ::recv(client_socket, buffer, buffer_size, 0);
    if (iResult > 0) {
        return iResult;
    }

    if (iResult < 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
        return 0;
    }

    stop();

    return 0;
#else
    int n = ::read(client_socket, buffer, buffer_size);
    if (n > 0) {
        return n;
    }

    if (n < 0 && errno == EWOULDBLOCK) {
        return 0;
    }

    stop();

    return 0;
#endif    
}

int write(const char *buffer, int buffer_size) {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    int iSendResult;

    if (client_socket != INVALID_SOCKET) {
        // Echo the buffer back to the sender
        iSendResult = ::send(client_socket, buffer, buffer_size, 0);
        if (iSendResult == SOCKET_ERROR) {
            DebugTrace("send failed with error: %d\n", WSAGetLastError());
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
            return 0;
        }
        return iSendResult;
    }

    return 0;
#else
    if (client_socket != -1) {
        int n = ::write(client_socket, buffer, buffer_size);
        if (n < 0) {
            close(client_socket);
            client_socket = -1;
            return 0;
        }
        return n;
    }

    return 0;
#endif    
}

void stop() {
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32
    if (client_socket != INVALID_SOCKET) {
        int iResult = ::shutdown(client_socket, SD_SEND);
        if (iResult == SOCKET_ERROR) {
            DebugTrace("EHTERNET shutdown failed with error %d\n", WSAGetLastError());
        }
        closesocket(client_socket);
        client_socket = INVALID_SOCKET;
    }
#else
    int result = ::shutdown(client_socket, SHUT_WR);
    if (result < 0) {
        DebugTrace("ETHERNET shutdown failed with error %d\n", errno);
    }
    close(client_socket);
    client_socket = -1;
#endif    
}

////////////////////////////////////////////////////////////////////////////////

EthernetModule Ethernet;

bool EthernetModule::begin(uint8_t *mac, uint8_t *, uint8_t *, uint8_t *, uint8_t *) {
    delay(500);
    return true;
}

uint8_t EthernetModule::maintain() {
    return 0;
}

IPAddress EthernetModule::localIP() {
    return IPAddress();
}

IPAddress EthernetModule::subnetMask() {
    return IPAddress();
}

IPAddress EthernetModule::gatewayIP() {
    return IPAddress();
}

IPAddress EthernetModule::dnsServerIP() {
    return IPAddress();
}

////////////////////////////////////////////////////////////////////////////////

void EthernetServer::init(int port_) {
    port = port_;
    client = true;
}

void EthernetServer::begin() {
    bind_result = mcu::ethernet::bind(port);
}

EthernetClient EthernetServer::available() {
    if (!bind_result)
        return EthernetClient();
    return client;
}

////////////////////////////////////////////////////////////////////////////////

EthernetClient::EthernetClient() : valid(false) {
}

EthernetClient::EthernetClient(bool valid_) : valid(valid_) {
}

bool EthernetClient::connected() {
    return mcu::ethernet::connected();
}

EthernetClient::operator bool() {
    return valid && mcu::ethernet::client_available();
}

size_t EthernetClient::available() {
    return mcu::ethernet::available();
}

size_t EthernetClient::read(uint8_t *buffer, size_t buffer_size) {
    return mcu::ethernet::read((char *)buffer, (int)buffer_size);
}

size_t EthernetClient::write(const char *buffer, size_t buffer_size) {
    return mcu::ethernet::write(buffer, (int)buffer_size);
}

void EthernetClient::flush() {
}

void EthernetClient::stop() {
    mcu::ethernet::stop();
}

////////////////////////////////////////////////////////////////////////////////

uint8_t EthernetUDP::begin(uint16_t port) {
    return 0;
}

void EthernetUDP::stop() {
}

int EthernetUDP::beginPacket(const char *host, uint16_t port) {
    return 0;
}

size_t EthernetUDP::write(const uint8_t *buffer, size_t size) {
    return 0;
}

int EthernetUDP::endPacket() {
    return 0;
}

int EthernetUDP::read(unsigned char *buffer, size_t len) {
    return 0;
}

int EthernetUDP::parsePacket() {
    return 0;
}

} // namespace ethernet
} // namespace mcu
} // namespace eez

#endif