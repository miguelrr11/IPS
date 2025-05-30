// client_gui.c
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "common.h"

#pragma comment(lib, "ws2_32.lib")

#define ID_NICK     101
#define ID_BTN_REG  102
#define ID_MSG_IN   103
#define ID_BTN_SEND 104
#define ID_CHAT_OUT 105

SOCKET sock_recv, sock_send;
struct sockaddr_in srv_data;
char nickname[MAX_MSG_LEN] = "Usuario";

WNDPROC orig_nick_proc, orig_msg_proc;

void append_text(HWND hwnd, const char* msg) {
    int len = GetWindowTextLength(hwnd);
    SendMessage(hwnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(hwnd, EM_REPLACESEL, 0, (LPARAM)msg);
}

LRESULT CALLBACK NickProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        char text[MAX_MSG_LEN];
        GetWindowText(hwnd, text, MAX_MSG_LEN);
        text[strcspn(text, "\r\n")] = 0;
        SetWindowText(hwnd, text);
        SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(ID_BTN_REG, BN_CLICKED), 0);
        return 0; // evitar el beep
    }
    return CallWindowProc(orig_nick_proc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        char text[MAX_MSG_LEN];
        GetWindowText(hwnd, text, MAX_MSG_LEN);
        text[strcspn(text, "\r\n")] = 0;
        SetWindowText(hwnd, text);
        SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(ID_BTN_SEND, BN_CLICKED), 0);
        return 0;
    }
    return CallWindowProc(orig_msg_proc, hwnd, msg, wParam, lParam);
}


DWORD WINAPI recv_thread(LPVOID arg) {
    char buffer[sizeof(broad_pkt_t)+MAX_MSG_LEN];
    while (1) {
        struct sockaddr_in src;
        int addrlen = sizeof(src);
        int n = recvfrom(sock_recv, buffer, sizeof(buffer), 0,
                         (struct sockaddr*)&src, &addrlen);
        if (n >= sizeof(broad_pkt_t)) {
            broad_pkt_t *b = (broad_pkt_t*)buffer;
            if (ntohs(b->type)==MSG_TYPE_BROAD) {
                int msg_len = ntohs(b->length);
                char out[MAX_MSG_LEN + 2];
                snprintf(out, sizeof(out), "%.*s\r\n", msg_len, b->text);
                HWND hwndOut = GetDlgItem((HWND)arg, ID_CHAT_OUT);
                append_text(hwndOut, out);
            }
        }
    }
    return 0;
}

void iniciar_conexion(HWND hwnd) {
    const char* server_ip = "127.0.0.1";
    int reg_port = 9000;
    int data_port = 9001;

    sock_recv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_recv == INVALID_SOCKET) {
        MessageBox(hwnd, "Socket recv failed", "Error", MB_ICONERROR);
        return;
    }

    struct sockaddr_in cli_recv = {0};
    cli_recv.sin_family = AF_INET;
    cli_recv.sin_addr.s_addr = INADDR_ANY;
    cli_recv.sin_port = htons(0);
    if (bind(sock_recv, (struct sockaddr*)&cli_recv, sizeof(cli_recv)) == SOCKET_ERROR) {
        MessageBox(hwnd, "Bind recv failed", "Error", MB_ICONERROR);
        return;
    }

    int len = sizeof(cli_recv);
    getsockname(sock_recv, (struct sockaddr*)&cli_recv, &len);
    int recv_port = ntohs(cli_recv.sin_port);

    sock_send = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_send == INVALID_SOCKET) {
        MessageBox(hwnd, "Socket send failed", "Error", MB_ICONERROR);
        return;
    }

    struct sockaddr_in cli_send = cli_recv;
    cli_send.sin_port = htons(recv_port+1);
    if (bind(sock_send, (struct sockaddr*)&cli_send, sizeof(cli_send)) == SOCKET_ERROR) {
        MessageBox(hwnd, "Bind send failed", "Error", MB_ICONERROR);
        return;
    }

    struct sockaddr_in srv_reg = {0};
    srv_reg.sin_family = AF_INET;
    srv_reg.sin_port = htons(reg_port);
    inet_pton(AF_INET, server_ip, &srv_reg.sin_addr);
    reg_pkt_t regp = {htons(MSG_TYPE_REG), htons(recv_port)};
    sendto(sock_send, (char*)&regp, sizeof(regp), 0,
           (struct sockaddr*)&srv_reg, sizeof(srv_reg));

    srv_data.sin_family = AF_INET;
    srv_data.sin_port = htons(data_port);
    inet_pton(AF_INET, server_ip, &srv_data.sin_addr);

    CreateThread(NULL, 0, recv_thread, hwnd, 0, NULL);
}

void enviar_mensaje(HWND hwnd) {
    HWND hwndMsg = GetDlgItem(hwnd, ID_MSG_IN);
    int len = GetWindowTextLength(hwndMsg);
    if (len == 0) return;

    char msg[MAX_MSG_LEN] = {0};
    GetWindowText(hwndMsg, msg, MAX_MSG_LEN);
    char full[MAX_MSG_LEN];
    int flen = snprintf(full, MAX_MSG_LEN, "[%s] %s", nickname, msg);
    chat_pkt_t *chat = (chat_pkt_t*)malloc(sizeof(chat_pkt_t)+flen);
    chat->type = htons(MSG_TYPE_CHAT);
    chat->length = htons(flen);
    memcpy(chat->text, full, flen);
    sendto(sock_send, (char*)chat, sizeof(chat_pkt_t)+flen, 0,
           (struct sockaddr*)&srv_data, sizeof(srv_data));
    free(chat);
    SetWindowText(hwndMsg, "");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_CREATE: {
            CreateWindow("edit", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 10, 200, 25,
                         hwnd, (HMENU)ID_NICK, NULL, NULL);
            CreateWindow("button", "Registrar", WS_CHILD | WS_VISIBLE, 220, 10, 100, 25,
                         hwnd, (HMENU)ID_BTN_REG, NULL, NULL);
            CreateWindow("edit", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE |
                         ES_AUTOVSCROLL | ES_READONLY, 10, 50, 310, 300,
                         hwnd, (HMENU)ID_CHAT_OUT, NULL, NULL);
            CreateWindow("edit", "", WS_CHILD | WS_VISIBLE | WS_BORDER, 10, 360, 240, 25,
                         hwnd, (HMENU)ID_MSG_IN, NULL, NULL);
            CreateWindow("button", "Enviar", WS_CHILD | WS_VISIBLE, 260, 360, 60, 25,
                         hwnd, (HMENU)ID_BTN_SEND, NULL, NULL);

            // Subclasificar los campos de entrada
            HWND hwndNick = GetDlgItem(hwnd, ID_NICK);
            HWND hwndMsg  = GetDlgItem(hwnd, ID_MSG_IN);
            orig_nick_proc = (WNDPROC)SetWindowLongPtr(hwndNick, GWLP_WNDPROC, (LONG_PTR)NickProc);
            orig_msg_proc  = (WNDPROC)SetWindowLongPtr(hwndMsg,  GWLP_WNDPROC, (LONG_PTR)MsgProc);
            break;
        }

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BTN_REG) {
                GetWindowText(GetDlgItem(hwnd, ID_NICK), nickname, MAX_MSG_LEN);
                iniciar_conexion(hwnd);
                EnableWindow(GetDlgItem(hwnd, ID_BTN_REG), FALSE);
            } else if (LOWORD(wParam) == ID_BTN_SEND) {
                enviar_mensaje(hwnd);
            }
            break;

        case WM_DESTROY:
            closesocket(sock_send);
            closesocket(sock_recv);
            WSACleanup();
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR args, int nCmdShow) {
    WNDCLASS wc = {0};
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInst;
    wc.lpszClassName = "ChatClient";
    wc.lpfnWndProc = WndProc;

    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    RegisterClass(&wc);
    HWND hwnd = CreateWindow("ChatClient", "Chat Global", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             100, 100, 350, 450, NULL, NULL, NULL, NULL);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}