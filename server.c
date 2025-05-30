#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "common.h"

#pragma comment(lib, "ws2_32.lib")

struct client {
    struct sockaddr_in addr;
};

int main(int argc, char *argv[]) {
    if (argc > 1) {
        fprintf(stderr, "No arguments expected\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Inicializar Winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "Failed to initialize Winsock.\n");
        return EXIT_FAILURE;
    }

    int reg_port = 9000;
    int data_port = 9001;

    SOCKET sock_reg = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SOCKET sock_data = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_reg == INVALID_SOCKET || sock_data == INVALID_SOCKET) {
        fprintf(stderr, "Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return EXIT_FAILURE;
    }

    struct sockaddr_in srv_reg = {0}, srv_data = {0};
    srv_reg.sin_family = AF_INET;
    srv_reg.sin_addr.s_addr = INADDR_ANY;
    srv_reg.sin_port = htons(reg_port);

    srv_data.sin_family = AF_INET;
    srv_data.sin_addr.s_addr = INADDR_ANY;
    srv_data.sin_port = htons(data_port);

    if (bind(sock_reg, (struct sockaddr*)&srv_reg, sizeof(srv_reg)) == SOCKET_ERROR) {
        fprintf(stderr, "bind(reg) failed: %d\n", WSAGetLastError());
        closesocket(sock_reg);
        closesocket(sock_data);
        WSACleanup();
        return EXIT_FAILURE;
    }
    if (bind(sock_data, (struct sockaddr*)&srv_data, sizeof(srv_data)) == SOCKET_ERROR) {
        fprintf(stderr, "bind(data) failed: %d\n", WSAGetLastError());
        closesocket(sock_reg);
        closesocket(sock_data);
        WSACleanup();
        return EXIT_FAILURE;
    }

    struct client clients[MAX_CLIENTS];
    int client_count = 0;

    printf("Server running on ports %d (reg) and %d (data)\n", reg_port, data_port);

    fd_set readfds;
    SOCKET maxfd = (sock_reg > sock_data ? sock_reg : sock_data);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock_reg, &readfds);
        FD_SET(sock_data, &readfds);

        if (select((int)maxfd+1, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) {
            fprintf(stderr, "select failed: %d\n", WSAGetLastError());
            break;
        }

        if (FD_ISSET(sock_reg, &readfds)) {
            reg_pkt_t pkt;
            struct sockaddr_in cli_addr;
            int addrlen = sizeof(cli_addr);
            int n = recvfrom(sock_reg, (char*)&pkt, sizeof(pkt), 0,
                             (struct sockaddr*)&cli_addr, &addrlen);
            if (n == sizeof(pkt) && ntohs(pkt.type) == MSG_TYPE_REG) {
                if (client_count < MAX_CLIENTS) {
                    clients[client_count].addr = cli_addr;
                    clients[client_count].addr.sin_port = pkt.port;
                    printf("[DEBUG] Registered client %s:%d (total: %d)\n",
                           inet_ntoa(cli_addr.sin_addr), ntohs(pkt.port), client_count+1);
                    client_count++;
                }
            }
        }

        if (FD_ISSET(sock_data, &readfds)) {
            char buffer[sizeof(chat_pkt_t)+MAX_MSG_LEN];
            struct sockaddr_in src_addr;
            int addrlen = sizeof(src_addr);
            int n = recvfrom(sock_data, buffer, sizeof(buffer), 0,
                             (struct sockaddr*)&src_addr, &addrlen);
            if (n >= sizeof(chat_pkt_t)) {
                chat_pkt_t *chat = (chat_pkt_t*)buffer;
                if (ntohs(chat->type) == MSG_TYPE_CHAT) {
                    uint16_t msg_len = ntohs(chat->length);
                    ack_pkt_t ack;
                    ack.type = htons(MSG_TYPE_ACK);
                    sendto(sock_data, (char*)&ack, sizeof(ack), 0,
                           (struct sockaddr*)&src_addr, addrlen);

                    size_t pkt_size = sizeof(broad_pkt_t)+msg_len;
                    char *out_pkt = (char*)malloc(pkt_size);
                    broad_pkt_t *broad = (broad_pkt_t*)out_pkt;
                    broad->type = htons(MSG_TYPE_BROAD);
                    broad->length = htons(msg_len);
                    memcpy(broad->text, chat->text, msg_len);

                    for (int i=0; i<client_count; i++) {
                        sendto(sock_data, out_pkt, (int)pkt_size, 0,
                               (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
                    }
                    free(out_pkt);
                }
            }
        }
    }

    // Cerrar sockets y Winsock
    closesocket(sock_reg);
    closesocket(sock_data);
    WSACleanup();
    return EXIT_SUCCESS;
}