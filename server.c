#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include "common.h"

struct client {
    struct sockaddr_in addr;
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <reg_port> <data_port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int reg_port = atoi(argv[1]);
    int data_port = atoi(argv[2]);

    int sock_reg = socket(AF_INET, SOCK_DGRAM, 0);
    int sock_data = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_reg < 0 || sock_data < 0) {
        perror("socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in srv_reg = {0}, srv_data = {0};
    srv_reg.sin_family = AF_INET;
    srv_reg.sin_addr.s_addr = INADDR_ANY;
    srv_reg.sin_port = htons(reg_port);

    srv_data.sin_family = AF_INET;
    srv_data.sin_addr.s_addr = INADDR_ANY;
    srv_data.sin_port = htons(data_port);

    if (bind(sock_reg, (struct sockaddr*)&srv_reg, sizeof(srv_reg)) < 0) {
        perror("bind reg");
        return EXIT_FAILURE;
    }
    if (bind(sock_data, (struct sockaddr*)&srv_data, sizeof(srv_data)) < 0) {
        perror("bind data");
        return EXIT_FAILURE;
    }

    struct client clients[MAX_CLIENTS];
    int client_count = 0;

    printf("Server running: reg_port=%d, data_port=%d\n", reg_port, data_port);

    fd_set readfds;
    int maxfd = sock_reg > sock_data ? sock_reg : sock_data;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock_reg, &readfds);
        FD_SET(sock_data, &readfds);

        if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(sock_reg, &readfds)) {
            reg_pkt_t pkt;
            struct sockaddr_in cli_addr;
            socklen_t addrlen = sizeof(cli_addr);
            ssize_t n = recvfrom(sock_reg, &pkt, sizeof(pkt), 0,
                                 (struct sockaddr*)&cli_addr, &addrlen);
            if (n == sizeof(pkt) && ntohs(pkt.type) == MSG_TYPE_REG) {
                if (client_count < MAX_CLIENTS) {
                    clients[client_count].addr = cli_addr;
                    clients[client_count].addr.sin_port = pkt.port;
                    printf("[DEBUG] Registered client %s:%d (total: %d)\n",
                           inet_ntoa(cli_addr.sin_addr), ntohs(pkt.port), client_count + 1);
                    client_count++;
                }
            }
        }

        if (FD_ISSET(sock_data, &readfds)) {
            char buffer[sizeof(chat_pkt_t) + MAX_MSG_LEN];
            struct sockaddr_in src_addr;
            socklen_t addrlen = sizeof(src_addr);
            ssize_t n = recvfrom(sock_data, buffer, sizeof(buffer), 0,
                                 (struct sockaddr*)&src_addr, &addrlen);
            if (n >= sizeof(chat_pkt_t)) {
                chat_pkt_t *chat = (chat_pkt_t*)buffer;
                if (ntohs(chat->type) == MSG_TYPE_CHAT) {
                    uint16_t msg_len = ntohs(chat->length);
                    ack_pkt_t ack;
                    ack.type = htons(MSG_TYPE_ACK);
                    sendto(sock_data, &ack, sizeof(ack), 0,
                           (struct sockaddr*)&src_addr, addrlen);

                    size_t pkt_size = sizeof(broad_pkt_t) + msg_len;
                    char *out_pkt = malloc(pkt_size);
                    broad_pkt_t *broad = (broad_pkt_t*)out_pkt;
                    broad->type = htons(MSG_TYPE_BROAD);
                    broad->length = htons(msg_len);
                    memcpy(broad->text, chat->text, msg_len);

                    for (int i = 0; i < client_count; i++) {
                        sendto(sock_data, out_pkt, pkt_size, 0,
                               (struct sockaddr*)&clients[i].addr,
                               sizeof(clients[i].addr));
                    }
                    free(out_pkt);
                }
            }
        }
    }

    close(sock_reg);
    close(sock_data);
    return EXIT_SUCCESS;
}
