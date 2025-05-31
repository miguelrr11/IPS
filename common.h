#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define MAX_MSG_LEN 512
#define MAX_CLIENTS 100

// Código de mensaje
enum {
    MSG_TYPE_REG   = 1,
    MSG_TYPE_CHAT  = 2,
    MSG_TYPE_BROAD = 3,
    MSG_TYPE_ACK   = 4,
    MSG_TYPE_PRIV  = 5
};

#pragma pack(push,1)
// Paquete de registro
typedef struct {
    uint16_t type;   // = MSG_TYPE_REG
    uint16_t port;   // puerto de recepción del cliente (net-order)
} reg_pkt_t;

// Paquete de chat (cliente → servidor)
typedef struct {
    uint16_t type;       // = MSG_TYPE_CHAT
    uint16_t length;     // longitud del texto (net-order)
    char     text[];     // datos UTF-8
} chat_pkt_t;

// Paquete de difusión (servidor → cliente)
typedef struct {
    uint16_t type;       // = MSG_TYPE_BROAD
    uint16_t length;     // longitud del texto (net-order)
    char     text[];     // datos UTF-8
} broad_pkt_t;

// Paquete de confirmación (ACK)
typedef struct {
    uint16_t type;       // = MSG_TYPE_ACK
} ack_pkt_t;
#pragma pack(pop)

#endif // COMMON_H