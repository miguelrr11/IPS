#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "common.h"

#pragma comment(lib, "ws2_32.lib")

struct client {
    struct sockaddr_in addr;
    char nickname[MAX_MSG_LEN];
};

int main(int argc, char *argv[]) {
    if (argc > 1) {
        fprintf(stderr, "Demasiados argumentos\n");
        return EXIT_FAILURE;
    }

    // Inicializar Winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "Error al inicializar Winsock.\n");
        return EXIT_FAILURE;
    }

    int reg_port = 9000;
    int data_port = 9001;

    SOCKET sock_reg = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    SOCKET sock_data = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_reg == INVALID_SOCKET || sock_data == INVALID_SOCKET) {
        fprintf(stderr, "Error al crear socket: %d\n", WSAGetLastError());
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
        fprintf(stderr, "Error en bind(reg): %d\n", WSAGetLastError());
        closesocket(sock_reg);
        closesocket(sock_data);
        WSACleanup();
        return EXIT_FAILURE;
    }
    if (bind(sock_data, (struct sockaddr*)&srv_data, sizeof(srv_data)) == SOCKET_ERROR) {
        fprintf(stderr, "Error en bind(data): %d\n", WSAGetLastError());
        closesocket(sock_reg);
        closesocket(sock_data);
        WSACleanup();
        return EXIT_FAILURE;
    }

    struct client clients[MAX_CLIENTS];
    int client_count = 0;

    printf("Servidor operativo en los puertos %d (reg) y %d (data)\n", reg_port, data_port);

    fd_set readfds;
    SOCKET maxfd = (sock_reg > sock_data ? sock_reg : sock_data);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock_reg, &readfds);
        FD_SET(sock_data, &readfds);

        if (select((int)maxfd+1, &readfds, NULL, NULL, NULL) == SOCKET_ERROR) {
            fprintf(stderr, "Error en select: %d\n", WSAGetLastError());
            break;
        }

        if (FD_ISSET(sock_reg, &readfds)) {
            char buffer[sizeof(reg_pkt_t) + MAX_MSG_LEN];
            struct sockaddr_in cli_addr;
            int addrlen = sizeof(cli_addr);
            int n = recvfrom(sock_reg, buffer, sizeof(buffer), 0,
                             (struct sockaddr*)&cli_addr, &addrlen);

            if (n >= sizeof(reg_pkt_t)) {
                reg_pkt_t *pkt = (reg_pkt_t*)buffer;
                if (ntohs(pkt->type) == MSG_TYPE_REG) {
                    char nickname[MAX_MSG_LEN] = "anon";
                    int nick_len = n - sizeof(reg_pkt_t);
                    if (nick_len > 0 && nick_len < MAX_MSG_LEN) {
                        memcpy(nickname, buffer + sizeof(reg_pkt_t), nick_len);
                        nickname[nick_len] = '\0';

                        // Cortar en el primer espacio, si lo hay
                        char* space = strchr(nickname, ' ');
                        if (space) *space = '\0';
                    }


                    if (client_count < MAX_CLIENTS) {
                        clients[client_count].addr = cli_addr;
                        clients[client_count].addr.sin_port = pkt->port;
                        strncpy(clients[client_count].nickname, nickname, MAX_MSG_LEN);
                        printf("[DEBUG] Usuario registrado %s recv_port=%d send_port=%d como '%s' (total: %d)\n",
                            inet_ntoa(cli_addr.sin_addr),
                            ntohs(pkt->port),
                            ntohs(pkt->port) + 1,
                            nickname,
                            client_count + 1);

                        client_count++;
                    }
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
                uint16_t msg_type = ntohs(chat->type);

                if (msg_type == MSG_TYPE_CHAT) {
                    uint16_t msg_len = ntohs(chat->length);
                    ack_pkt_t ack = { htons(MSG_TYPE_ACK) };
                    sendto(sock_data, (char*)&ack, sizeof(ack), 0,
                           (struct sockaddr*)&src_addr, addrlen);

                    size_t pkt_size = sizeof(broad_pkt_t) + msg_len;
                    char *out_pkt = (char*)malloc(pkt_size);
                    broad_pkt_t *broad = (broad_pkt_t*)out_pkt;
                    broad->type = htons(MSG_TYPE_BROAD);
                    broad->length = htons(msg_len);
                    memcpy(broad->text, chat->text, msg_len);

                    for (int i = 0; i < client_count; i++) {
                        sendto(sock_data, out_pkt, (int)pkt_size, 0,
                               (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
                    }
                    free(out_pkt);
                }

                else if (msg_type == MSG_TYPE_PRIV) {
                    uint16_t msg_len = ntohs(chat->length);
                    ack_pkt_t ack = { htons(MSG_TYPE_ACK) };
                    sendto(sock_data, (char*)&ack, sizeof(ack), 0,
                           (struct sockaddr*)&src_addr, addrlen);

                    // Extraer destino:mensaje
                    const char *body = chat->text;
                    const char *sep = strchr(body, ':');
                    if (!sep || sep == body || (sep - body) >= MAX_MSG_LEN)
                        continue;

                    char dst_nick[MAX_MSG_LEN];
                    int name_len = (int)(sep - body);
                    strncpy(dst_nick, body, name_len);
                    dst_nick[name_len] = '\0';

                    const char *msg_text = sep + 1;
                    size_t final_len = strlen(msg_text);

                    size_t pkt_size = sizeof(broad_pkt_t) + final_len;
                    char *out_pkt = (char*)malloc(pkt_size);
                    broad_pkt_t *broad = (broad_pkt_t*)out_pkt;
                    broad->type = htons(MSG_TYPE_BROAD);
                    broad->length = htons((uint16_t)final_len);
                    memcpy(broad->text, msg_text, final_len);

                    int sent = 0;
                    for (int i = 0; i < client_count; i++) {
                        if (strcmp(clients[i].nickname, dst_nick) == 0) {
                            sendto(sock_data, out_pkt, (int)pkt_size, 0,
                                   (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
                            sent = 1;
                            break;
                        }
                    }

                    if (!sent) {
                        printf("[INFO] Nickname '%s' no encontrado. Mensage descartado.\n", dst_nick);
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
