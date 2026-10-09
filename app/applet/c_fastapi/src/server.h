/**
 * @file server.h
 * @brief Linux epoll 非阻塞 Reactor 事件迴圈與 Connection 記憶體管理核心
 */

#ifndef SERVER_H
#define SERVER_H

#include "../include/fastapi.h"
#include "router.h"
#include <sys/epoll.h>

#define FASTAPI_INITIAL_BUFFER_SIZE 4096
#define FASTAPI_MAX_BUFFER_SIZE (1024 * 1024 * 16) /* 16MB 請求上限保護 */
#define FASTAPI_MAX_EPOLL_EVENTS 1024
#define FASTAPI_CONN_TIMEOUT_SEC 60

/* 單一連線狀態機與記憶體管理結構 */
typedef struct client_conn {
    int fd;
    
    /* 接收緩衝區 (動態擴展) */
    char *read_buf;
    size_t read_cap;
    size_t read_len;

    /* 發送緩衝區 (處理非阻塞網路背壓 Partial Writes) */
    char *write_buf;
    size_t write_len;
    size_t write_sent;

    /* 連線管理與時間標記 */
    bool keep_alive;
    time_t last_active;
    
    struct client_conn *prev;
    struct client_conn *next;
} client_conn_t;

/* C-FastAPI 應用內部結構實作 */
struct fastapi_app {
    int server_fd;
    int epoll_fd;
    volatile bool running;
    
    router_t router;

    /* 活躍連線雙向鏈結串列 (便於安全巡檢與優雅釋放) */
    client_conn_t *conn_head;
    size_t active_connections;
};

#endif /* SERVER_H */
