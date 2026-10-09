/**
 * @file server.c
 * @brief Linux epoll 非阻塞 Reactor 事件迴圈與 Connection 記憶體管理實作
 * 
 * 核心功能：
 * 1. Socket Programming: 非阻塞 TCP Socket、監聽、綁定、接受連線
 * 2. 異步 I/O 多路復用 (Async Event Loop): epoll_wait 事件驅動
 * 3. 嚴密記憶體管理: 動態擴充緩衝區、Partial Reads/Writes、EAGAIN 處理
 * 4. 路由分派與回應回傳: 完全無阻塞運作
 */

#define _GNU_SOURCE
#include "server.h"
#include "http_parser.h"
#include "response.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* 全域指標用於訊號處理 (SIGINT / SIGTERM 優雅關閉) */
static fastapi_app_t *g_active_app = NULL;

static void handle_signal(int sig) {
    (void)sig;
    if (g_active_app) {
        g_active_app->running = false;
    }
}

/* 將 File Descriptor 設定為非阻塞模式 (O_NONBLOCK) */
static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* 建立與初始化 client 連線記憶體結構 */
static client_conn_t *conn_create(fastapi_app_t *app, int client_fd) {
    client_conn_t *conn = (client_conn_t *)malloc(sizeof(client_conn_t));
    if (!conn) return NULL;

    conn->fd = client_fd;
    conn->read_cap = FASTAPI_INITIAL_BUFFER_SIZE;
    conn->read_len = 0;
    conn->read_buf = (char *)malloc(conn->read_cap);

    conn->write_buf = NULL;
    conn->write_len = 0;
    conn->write_sent = 0;
    conn->keep_alive = true;
    conn->last_active = time(NULL);

    if (!conn->read_buf) {
        free(conn);
        return NULL;
    }

    /* 加入雙向串列管理 */
    conn->prev = NULL;
    conn->next = app->conn_head;
    if (app->conn_head) {
        app->conn_head->prev = conn;
    }
    app->conn_head = conn;
    app->active_connections++;

    return conn;
}

/* 關閉連線並完全釋放記憶體 (保證無 Memory Leak) */
static void conn_close(fastapi_app_t *app, client_conn_t *conn) {
    if (!conn) return;

    /* 從 epoll 移除監聽 */
    epoll_ctl(app->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
    close(conn->fd);

    /* 釋放接收與發送緩衝區 */
    if (conn->read_buf) {
        free(conn->read_buf);
        conn->read_buf = NULL;
    }
    if (conn->write_buf) {
        free(conn->write_buf);
        conn->write_buf = NULL;
    }

    /* 從串列移除 */
    if (conn->prev) {
        conn->prev->next = conn->next;
    } else {
        app->conn_head = conn->next;
    }
    if (conn->next) {
        conn->next->prev = conn->prev;
    }

    app->active_connections--;
    free(conn);
}

/* 更新連線在 epoll 中的監聽事件 (例如切換 EPOLLIN <-> EPOLLOUT) */
static int conn_modify_epoll(fastapi_app_t *app, client_conn_t *conn, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.ptr = conn;
    return epoll_ctl(app->epoll_fd, EPOLL_CTL_MOD, conn->fd, &ev);
}

/* 非阻塞接收客戶端資料 */
static void handle_client_read(fastapi_app_t *app, client_conn_t *conn) {
    conn->last_active = time(NULL);

    while (1) {
        /* 若緩衝區剩餘空間不足，以倍增策略動態擴展 */
        if (conn->read_cap - conn->read_len < 1024) {
            size_t new_cap = conn->read_cap * 2;
            if (new_cap > FASTAPI_MAX_BUFFER_SIZE) {
                /* 超過最大記憶體保護上限，防範 DoS 攻擊 */
                conn_close(app, conn);
                return;
            }
            char *new_buf = (char *)realloc(conn->read_buf, new_cap);
            if (!new_buf) {
                conn_close(app, conn);
                return;
            }
            conn->read_buf = new_buf;
            conn->read_cap = new_cap;
        }

        ssize_t bytes_read = recv(conn->fd, 
                                  conn->read_buf + conn->read_len, 
                                  conn->read_cap - conn->read_len - 1, 
                                  0);

        if (bytes_read > 0) {
            conn->read_len += (size_t)bytes_read;
            conn->read_buf[conn->read_len] = '\0';
        } else if (bytes_read == 0) {
            /* 客戶端關閉連線 (EOF) */
            conn_close(app, conn);
            return;
        } else {
            /* 讀取完畢或發生錯誤 */
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* 當前無更多資料可讀，退出循環等待下一次 EPOLLIN 事件 (非阻塞核心) */
                break;
            } else if (errno == EINTR) {
                continue; /* 被訊號中斷，重試 */
            } else {
                /* 連線異常，安全釋放 */
                conn_close(app, conn);
                return;
            }
        }
    }

    /* 嘗試解析 HTTP 請求 */
    while (conn->read_len > 0) {
        http_request_t req;
        size_t consumed_bytes = 0;
        http_parse_status_t status = http_parse_request(conn->read_buf, conn->read_len, 
                                                        &req, &consumed_bytes);

        if (status == HTTP_PARSE_INCOMPLETE) {
            /* 資料尚未齊全，維持連線並等待後續 socket 資料 */
            break;
        }

        if (status == HTTP_PARSE_ERROR) {
            /* 語法錯誤，回傳 400 Bad Request 並關閉連線 */
            http_response_t err_res;
            fastapi_response_init(&err_res);
            fastapi_error_response(&err_res, 400, "Bad HTTP Request syntax");
            
            size_t err_len = 0;
            char *err_packet = response_serialize(&err_res, false, &err_len);
            if (err_packet) {
                send(conn->fd, err_packet, err_len, 0);
                free(err_packet);
            }
            response_cleanup(&err_res);
            conn_close(app, conn);
            return;
        }

        /* 成功解析 HTTP 請求！記錄連線設定 */
        conn->keep_alive = req.keep_alive;
        req.client_conn = conn;

        /* 初始化回應結構 */
        http_response_t res;
        fastapi_response_init(&res);

        /* 路由匹配與分派 (呼叫使用者定義之 API 路由) */
        int dispatch_result = router_dispatch(&app->router, &req, &res);
        if (dispatch_result == 404) {
            fastapi_error_response(&res, 404, "Endpoint not found");
        } else if (dispatch_result == 405) {
            fastapi_error_response(&res, 405, "Method not allowed for this route");
        }

        /* 將回應物件序列化為網路傳輸封包 */
        size_t resp_packet_len = 0;
        char *resp_packet = response_serialize(&res, conn->keep_alive, &resp_packet_len);

        /* 釋放 Request 與 Response 結構 */
        http_request_free(&req);
        response_cleanup(&res);

        if (!resp_packet) {
            conn_close(app, conn);
            return;
        }

        /* 移除非阻塞接收緩衝區中已消耗的請求資料 (支援 HTTP Pipelining) */
        if (consumed_bytes < conn->read_len) {
            memmove(conn->read_buf, conn->read_buf + consumed_bytes, conn->read_len - consumed_bytes);
            conn->read_len -= consumed_bytes;
            conn->read_buf[conn->read_len] = '\0';
        } else {
            conn->read_len = 0;
            conn->read_buf[0] = '\0';
        }

        /* 嘗試立即非阻塞發送回應 */
        ssize_t sent = send(conn->fd, resp_packet, resp_packet_len, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                sent = 0;
            } else {
                free(resp_packet);
                conn_close(app, conn);
                return;
            }
        }

        if ((size_t)sent < resp_packet_len) {
            /* 網路緩衝區已滿 (背壓)：將剩餘位元組存入 write_buf 並監聽 EPOLLOUT */
            conn->write_buf = resp_packet;
            conn->write_len = resp_packet_len;
            conn->write_sent = (size_t)sent;
            conn_modify_epoll(app, conn, EPOLLOUT | EPOLLHUP | EPOLLERR);
            return;
        } else {
            /* 完整發送完畢 */
            free(resp_packet);
            if (!conn->keep_alive) {
                conn_close(app, conn);
                return;
            }
        }
    }
}

/* 非阻塞發送緩衝區資料 (處理 EPOLLOUT 事件) */
static void handle_client_write(fastapi_app_t *app, client_conn_t *conn) {
    if (!conn->write_buf || conn->write_sent >= conn->write_len) {
        /* 無資料需發送，恢復為 EPOLLIN 監聽模式 */
        conn_modify_epoll(app, conn, EPOLLIN | EPOLLHUP | EPOLLERR);
        return;
    }

    conn->last_active = time(NULL);

    while (conn->write_sent < conn->write_len) {
        ssize_t sent = send(conn->fd, 
                            conn->write_buf + conn->write_sent, 
                            conn->write_len - conn->write_sent, 
                            MSG_NOSIGNAL);
        if (sent > 0) {
            conn->write_sent += (size_t)sent;
        } else if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* 核心緩衝區暫時無法寫入，繼續保持 EPOLLOUT 監聽 */
                return;
            } else if (errno == EINTR) {
                continue;
            } else {
                /* 連線異常，釋放資源 */
                conn_close(app, conn);
                return;
            }
        }
    }

    /* 發送完畢，釋放發送緩衝區 */
    free(conn->write_buf);
    conn->write_buf = NULL;
    conn->write_len = 0;
    conn->write_sent = 0;

    if (!conn->keep_alive) {
        conn_close(app, conn);
    } else {
        /* 切換回讀取模式 */
        conn_modify_epoll(app, conn, EPOLLIN | EPOLLHUP | EPOLLERR);
    }
}

/* 接受新的 TCP 連線 (非阻塞 Accept 迴圈) */
static void handle_accept(fastapi_app_t *app) {
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(app->server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* 所有連線均已接收完畢 */
                break;
            } else if (errno == EINTR) {
                continue;
            } else {
                perror("accept error");
                break;
            }
        }

        /* 設為非阻塞 */
        if (set_nonblocking(client_fd) < 0) {
            close(client_fd);
            continue;
        }

        /* 建立連線結構 */
        client_conn_t *conn = conn_create(app, client_fd);
        if (!conn) {
            close(client_fd);
            continue;
        }

        /* 加入 epoll 監聽 */
        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLHUP | EPOLLERR;
        ev.data.ptr = conn;
        if (epoll_ctl(app->epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
            conn_close(app, conn);
            continue;
        }
    }
}

/* 應用生命週期實作 */
fastapi_app_t *fastapi_create(void) {
    fastapi_app_t *app = (fastapi_app_t *)malloc(sizeof(fastapi_app_t));
    if (!app) return NULL;

    memset(app, 0, sizeof(fastapi_app_t));
    app->server_fd = -1;
    app->epoll_fd = -1;
    app->running = false;
    app->conn_head = NULL;
    app->active_connections = 0;

    router_init(&app->router);
    return app;
}

void fastapi_destroy(fastapi_app_t *app) {
    if (!app) return;

    /* 關閉所有連線並釋放記憶體 */
    client_conn_t *curr = app->conn_head;
    while (curr) {
        client_conn_t *next = curr->next;
        conn_close(app, curr);
        curr = next;
    }

    if (app->server_fd >= 0) close(app->server_fd);
    if (app->epoll_fd >= 0) close(app->epoll_fd);

    router_cleanup(&app->router);
    free(app);
}

int fastapi_route(fastapi_app_t *app, http_method_t method, const char *path_pattern, route_handler_t handler) {
    if (!app) return -1;
    return router_add(&app->router, method, path_pattern, handler);
}

void fastapi_stop(fastapi_app_t *app) {
    if (app) {
        app->running = false;
    }
}

/* 啟動 epoll 事件驅動伺服器 */
int fastapi_run(fastapi_app_t *app, const char *host, int port) {
    if (!app) return -1;

    /* 忽略 SIGPIPE 避免 client 提前斷線導致伺服器崩潰 */
    signal(SIGPIPE, SIG_IGN);
    
    /* 註冊優雅關閉訊號 */
    g_active_app = app;
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* 1. 建立 TCP Socket */
    app->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (app->server_fd < 0) {
        perror("socket creation failed");
        return -1;
    }

    /* 2. 設定 Socket 重複使用 Port 與非阻塞屬性 */
    int opt = 1;
    setsockopt(app->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(app->server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
    if (set_nonblocking(app->server_fd) < 0) {
        perror("set nonblocking failed");
        close(app->server_fd);
        return -1;
    }

    /* 3. 綁定監聽 IP 與 Port */
    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons((uint16_t)port);
    if (!host || strcmp(host, "0.0.0.0") == 0) {
        srv_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, host, &srv_addr.sin_addr);
    }

    if (bind(app->server_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("bind failed");
        close(app->server_fd);
        return -1;
    }

    /* 4. 監聽連線 */
    if (listen(app->server_fd, 4096) < 0) {
        perror("listen failed");
        close(app->server_fd);
        return -1;
    }

    /* 5. 建立 Linux epoll 實例 */
    app->epoll_fd = epoll_create1(0);
    if (app->epoll_fd < 0) {
        perror("epoll_create1 failed");
        close(app->server_fd);
        return -1;
    }

    /* 監聽伺服器 listen socket 的新連線事件 */
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.ptr = NULL; /* NULL 代表 server listen socket */
    if (epoll_ctl(app->epoll_fd, EPOLL_CTL_ADD, app->server_fd, &ev) < 0) {
        perror("epoll_ctl listen fd failed");
        close(app->server_fd);
        close(app->epoll_fd);
        return -1;
    }

    printf("\n=======================================================\n");
    printf("  🚀 C-FastAPI Asynchronous Server Running!\n");
    printf("  📍 Listening on: http://%s:%d\n", host ? host : "0.0.0.0", port);
    printf("  ⚡ Reactor Engine: Linux epoll (Non-blocking I/O)\n");
    printf("  🛡️  Zero-Dependency Pure C11 Framework\n");
    printf("=======================================================\n\n");

    app->running = true;
    struct epoll_event events[FASTAPI_MAX_EPOLL_EVENTS];

    /* 6. 非阻塞 Reactor 事件迴圈 (Event Loop) */
    while (app->running) {
        /* epoll_wait 等待事件發生 (逾時 1000ms 便於定期巡檢與檢查關閉旗標) */
        int nfds = epoll_wait(app->epoll_fd, events, FASTAPI_MAX_EPOLL_EVENTS, 1000);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait error");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.ptr == NULL) {
                /* 伺服器監聽 Socket 事件：接收新連線 */
                handle_accept(app);
            } else {
                /* 客戶端連線 I/O 事件 */
                client_conn_t *conn = (client_conn_t *)events[i].data.ptr;

                /* 異常與斷線檢查 */
                if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                    conn_close(app, conn);
                    continue;
                }

                /* 可讀事件：接收 HTTP 請求位元組 */
                if (events[i].events & EPOLLIN) {
                    handle_client_read(app, conn);
                }

                /* 可寫事件：發送排隊中的回應位元組 (處理非阻塞網路背壓) */
                if (events[i].events & EPOLLOUT) {
                    handle_client_write(app, conn);
                }
            }
        }
    }

    printf("\n🛑 Stopping C-FastAPI server and cleaning up resources...\n");
    fastapi_destroy(app);
    g_active_app = NULL;
    printf("✅ Server shutdown cleanly.\n");
    return 0;
}
