// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include "common.h"

int sock_send, sock_recv;
struct sockaddr_in srv_data_addr;
char nickname[MAX_MSG_LEN];

void* recv_thread(void* arg) {
    (void)arg;
    char buffer[sizeof(broad_pkt_t) + MAX_MSG_LEN];
    while (1) {
        struct sockaddr_in src;
        socklen_t addrlen = sizeof(src);
        ssize_t n = recvfrom(sock_recv, buffer, sizeof(buffer), 0,
                             (struct sockaddr*)&src, &addrlen);
        if (n >= sizeof(broad_pkt_t)) {
            broad_pkt_t *b = (broad_pkt_t*)buffer;
            if (ntohs(b->type) == MSG_TYPE_BROAD) {
                uint16_t len = ntohs(b->length);
                printf("%.*s\n", len, b->text);
            }
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [nickname]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        strncpy(nickname, argv[1], MAX_MSG_LEN-1);
        nickname[MAX_MSG_LEN-1] = '\0';
    } else {
        snprintf(nickname, MAX_MSG_LEN, "usuario_%d", getpid());
    }
    
    const char* server_ip = "127.0.0.1";
    int reg_port = 9000;
    int data_port = 9001;

    // Crear socket de recepción y permitir que el SO elija el puerto
    sock_recv = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_recv < 0) { perror("socket recv"); return EXIT_FAILURE; }
    struct sockaddr_in cli_recv = {0};
    cli_recv.sin_family = AF_INET;
    cli_recv.sin_addr.s_addr = INADDR_ANY;
    cli_recv.sin_port = htons(0);  // puerto dinámico básico
    if (bind(sock_recv, (struct sockaddr*)&cli_recv, sizeof(cli_recv)) < 0) {
        perror("bind recv"); return EXIT_FAILURE;
    }
    socklen_t len = sizeof(cli_recv);
    if (getsockname(sock_recv, (struct sockaddr*)&cli_recv, &len) < 0) {
        perror("getsockname"); return EXIT_FAILURE;
    }
    int recv_port = ntohs(cli_recv.sin_port);

    // Crear socket de envío y bind al puerto consecutivo
    sock_send = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_send < 0) { perror("socket send"); return EXIT_FAILURE; }
    struct sockaddr_in cli_send = cli_recv;
    cli_send.sin_port = htons(recv_port + 1);
    if (bind(sock_send, (struct sockaddr*)&cli_send, sizeof(cli_send)) < 0) {
        perror("bind send"); return EXIT_FAILURE;
    }
    printf("[%s] Using ports recv=%d, send=%d\n", nickname, recv_port, recv_port+1);

    // Registro automático usando puerto de recepción
    struct sockaddr_in srv_reg = {0};
    srv_reg.sin_family = AF_INET;
    srv_reg.sin_port = htons(reg_port);
    inet_pton(AF_INET, server_ip, &srv_reg.sin_addr);

    reg_pkt_t regp;
    regp.type = htons(MSG_TYPE_REG);
    regp.port = htons(recv_port);
    sendto(sock_send, &regp, sizeof(regp), 0,
           (struct sockaddr*)&srv_reg, sizeof(srv_reg));
    printf("[%s] Registered on server %s:%d\n", nickname, server_ip, reg_port);

    // Preparar dirección de datos
    srv_data_addr = (struct sockaddr_in){0};
    srv_data_addr.sin_family = AF_INET;
    srv_data_addr.sin_port = htons(data_port);
    inet_pton(AF_INET, server_ip, &srv_data_addr.sin_addr);

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, NULL);

    char msg[MAX_MSG_LEN];
    while (fgets(msg, sizeof(msg), stdin)) {
        size_t msglen = strnlen(msg, sizeof(msg));
        if (msglen > 0 && msg[msglen-1] == '\n') msg[--msglen] = '\0';

        char fullmsg[MAX_MSG_LEN];
        int prefix_len = snprintf(fullmsg, MAX_MSG_LEN, "[%s] %s", nickname, msg);
        if (prefix_len < 0) continue;

        size_t pkt_size = sizeof(chat_pkt_t) + prefix_len;
        char *out_pkt = malloc(pkt_size);
        chat_pkt_t *chat = (chat_pkt_t*)out_pkt;
        chat->type = htons(MSG_TYPE_CHAT);
        chat->length = htons(prefix_len);
        memcpy(chat->text, fullmsg, prefix_len);

        sendto(sock_send, out_pkt, pkt_size, 0,
               (struct sockaddr*)&srv_data_addr, sizeof(srv_data_addr));
        free(out_pkt);

        ack_pkt_t ack;
        recvfrom(sock_send, &ack, sizeof(ack), 0, NULL, NULL);
    }

    close(sock_send);
    close(sock_recv);
    return EXIT_SUCCESS;
}