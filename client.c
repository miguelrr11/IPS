#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "common.h"

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char *argv[]) {
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [nickname]\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Inicializar Winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "Winsock init failed.\n");
        return EXIT_FAILURE;
    }

    char nickname[MAX_MSG_LEN];
    if (argc == 2) {
        strncpy(nickname, argv[1], MAX_MSG_LEN-1);
        nickname[MAX_MSG_LEN-1] = '\0';
    } else {
        snprintf(nickname, MAX_MSG_LEN, "usuario_%lu", (unsigned long)GetCurrentProcessId());
    }

    const char* server_ip = "127.0.0.1";
    int reg_port = 9000;
    int data_port = 9001;

    // Crear socket de recepción y dejar que SO elija puerto
    SOCKET sock_recv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_recv == INVALID_SOCKET) { fprintf(stderr, "socket recv failed\n"); return EXIT_FAILURE; }
    struct sockaddr_in cli_recv = {0};
    cli_recv.sin_family = AF_INET;
    cli_recv.sin_addr.s_addr = INADDR_ANY;
    cli_recv.sin_port = htons(0);
    if (bind(sock_recv, (struct sockaddr*)&cli_recv, sizeof(cli_recv)) == SOCKET_ERROR) {
        fprintf(stderr, "bind recv failed\n"); return EXIT_FAILURE;
    }
    int len = sizeof(cli_recv);
    if (getsockname(sock_recv, (struct sockaddr*)&cli_recv, &len) == SOCKET_ERROR) {
        fprintf(stderr, "getsockname failed\n"); return EXIT_FAILURE;
    }
    int recv_port = ntohs(cli_recv.sin_port);

    // Crear socket de envío en puerto consecutivo
    SOCKET sock_send = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_send == INVALID_SOCKET) { fprintf(stderr, "socket send failed\n"); return EXIT_FAILURE; }
    struct sockaddr_in cli_send = cli_recv;
    cli_send.sin_port = htons(recv_port+1);
    if (bind(sock_send, (struct sockaddr*)&cli_send, sizeof(cli_send)) == SOCKET_ERROR) {
        fprintf(stderr, "bind send failed\n"); return EXIT_FAILURE;
    }

    printf("[%s] Using ports recv=%d, send=%d\n", nickname, recv_port, recv_port+1);

    // Registrarse
    struct sockaddr_in srv_reg = {0};
    srv_reg.sin_family = AF_INET;
    srv_reg.sin_port = htons(reg_port);
    inet_pton(AF_INET, server_ip, &srv_reg.sin_addr);
    reg_pkt_t regp = {htons(MSG_TYPE_REG), htons(recv_port)};
    sendto(sock_send, (char*)&regp, sizeof(regp), 0,
           (struct sockaddr*)&srv_reg, sizeof(srv_reg));
    printf("[%s] Registered on server %s:%d\n", nickname, server_ip, reg_port);

    // Preparar dirección de datos
    struct sockaddr_in srv_data = {0};
    srv_data.sin_family = AF_INET;
    srv_data.sin_port = htons(data_port);
    inet_pton(AF_INET, server_ip, &srv_data.sin_addr);

    DWORD WINAPI recv_thread(LPVOID arg) {
    SOCKET sock = *(SOCKET*)arg;
    char buffer[sizeof(broad_pkt_t)+MAX_MSG_LEN];
    while (1) {
        struct sockaddr_in src;
        int addrlen = sizeof(src);
        int n = recvfrom(sock, buffer, sizeof(buffer), 0,
                         (struct sockaddr*)&src, &addrlen);
        if (n >= sizeof(broad_pkt_t)) {
            broad_pkt_t *b = (broad_pkt_t*)buffer;
            if (ntohs(b->type)==MSG_TYPE_BROAD) {
                int msg_len = ntohs(b->length);
                printf("%.*s\n", msg_len, b->text);
            }
        }
    }
    return 0;
}


    // Hilo para recibir mensajes
    CreateThread(NULL, 0, recv_thread, &sock_recv, 0, NULL);

    char msg[MAX_MSG_LEN];
    while (fgets(msg, sizeof(msg), stdin)) {
        size_t l = strnlen(msg, sizeof(msg));
        if (l>0 && msg[l-1]=='\n') msg[--l]='\0';
        char full[MAX_MSG_LEN];
        int flen = snprintf(full, MAX_MSG_LEN, "[%s] %s", nickname, msg);
        chat_pkt_t *chat = (chat_pkt_t*)malloc(sizeof(chat_pkt_t)+flen);
        chat->type = htons(MSG_TYPE_CHAT);
        chat->length = htons(flen);
        memcpy(chat->text, full, flen);
        sendto(sock_send, (char*)chat, sizeof(chat_pkt_t)+flen, 0,
               (struct sockaddr*)&srv_data, sizeof(srv_data));
        free(chat);
        ack_pkt_t ack;
        recvfrom(sock_send, (char*)&ack, sizeof(ack), 0, NULL, NULL);
    }

    // Cleanup
    closesocket(sock_send);
    closesocket(sock_recv);
    WSACleanup();
    return EXIT_SUCCESS;
}
