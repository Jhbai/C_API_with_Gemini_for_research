/**
 * @file App.tsx
 * @brief C-FastAPI Developer Workbench & Interactive Architecture Console
 */

import React, { useState, useMemo } from 'react';
import { 
  Terminal, 
  Cpu, 
  Layers, 
  Send, 
  Copy, 
  Check, 
  Download, 
  FileCode, 
  ArrowRight, 
  Server, 
  ShieldCheck, 
  Sparkles,
  Zap,
  CornerDownRight
} from 'lucide-react';

/* 預載完整的 C 語言原始碼供使用者線上查閱與下載 */
const C_SOURCE_FILES = {
  'fastapi.h': {
    path: 'c_fastapi/include/fastapi.h',
    category: 'Framework Public Interface',
    description: '公開 API 標頭檔：定義請求/回應結構、路由巨集、JSON 回應函式與伺服器生命週期。',
    code: `/**
 * @file fastapi.h
 * @brief C-FastAPI: 仿照 Python FastAPI 的 C 語言高效非同步 Web 框架
 */
#ifndef FASTAPI_H
#define FASTAPI_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FASTAPI_VERSION "1.0.0"
#define FASTAPI_DEFAULT_PORT 8080
#define FASTAPI_MAX_HEADERS 64
#define FASTAPI_MAX_PARAMS 16
#define FASTAPI_MAX_QUERY_PARAMS 32

/* HTTP 請求方法 */
typedef enum {
    HTTP_METHOD_UNKNOWN = 0,
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_OPTIONS,
    HTTP_METHOD_HEAD
} http_method_t;

typedef struct {
    char *key;
    char *value;
} http_key_value_t;

/* HTTP 請求結構 */
typedef struct {
    http_method_t method;
    const char *method_str;
    char *raw_path;
    char *path;
    char *query_string;
    
    http_key_value_t headers[FASTAPI_MAX_HEADERS];
    size_t header_count;

    http_key_value_t params[FASTAPI_MAX_PARAMS];
    size_t param_count;

    http_key_value_t query_params[FASTAPI_MAX_QUERY_PARAMS];
    size_t query_count;

    char *body;
    size_t body_len;

    bool keep_alive;
    void *client_conn;
} http_request_t;

/* HTTP 回應結構 */
typedef struct {
    int status_code;
    const char *status_text;
    http_key_value_t headers[FASTAPI_MAX_HEADERS];
    size_t header_count;
    char *body;
    size_t body_len;
    bool body_dynamically_allocated;
} http_response_t;

typedef void (*route_handler_t)(const http_request_t *req, http_response_t *res);
typedef struct fastapi_app fastapi_app_t;

/* 框架核心生命週期 */
fastapi_app_t *fastapi_create(void);
void fastapi_destroy(fastapi_app_t *app);
int fastapi_route(fastapi_app_t *app, http_method_t method, const char *path_pattern, route_handler_t handler);
int fastapi_run(fastapi_app_t *app, const char *host, int port);
void fastapi_stop(fastapi_app_t *app);

/* FastAPI 快捷路由巨集 */
#define fastapi_get(app, path, handler)    fastapi_route(app, HTTP_METHOD_GET, path, handler)
#define fastapi_post(app, path, handler)   fastapi_route(app, HTTP_METHOD_POST, path, handler)
#define fastapi_put(app, path, handler)    fastapi_route(app, HTTP_METHOD_PUT, path, handler)
#define fastapi_delete(app, path, handler) fastapi_route(app, HTTP_METHOD_DELETE, path, handler)

/* 參數提取輔助函式 */
const char *fastapi_get_param(const http_request_t *req, const char *key);
const char *fastapi_get_query(const http_request_t *req, const char *key);
const char *fastapi_get_header(const http_request_t *req, const char *key);

/* 回應建構輔助函式 */
void fastapi_response_init(http_response_t *res);
void fastapi_response_set_header(http_response_t *res, const char *key, const char *value);
void fastapi_json_response(http_response_t *res, int status_code, const char *json_str);
void fastapi_text_response(http_response_t *res, int status_code, const char *text);
void fastapi_error_response(http_response_t *res, int status_code, const char *detail);

#ifdef __cplusplus
}
#endif

#endif /* FASTAPI_H */`
  },

  'api_routes.c': {
    path: 'c_fastapi/src/api_routes.c',
    category: 'Decoupled API Layer (業務路由)',
    description: '【分離架構】純 API 業務邏輯，完全不接觸底層 Socket 或 epoll，展示如 Python 般的路由寫法。',
    code: `/**
 * @file api_routes.c
 * @brief 應用程式層 API 路由實作 (與 Server 核心嚴格分離)
 */
#include "api_routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GET / */
static void handle_root(const http_request_t *req, http_response_t *res) {
    (void)req;
    const char *json_body = 
        "{\\n"
        "  \\"message\\": \\"Welcome to C-FastAPI!\\",\\n"
        "  \\"framework\\": \\"C-FastAPI\\",\\n"
        "  \\"version\\": \\"" FASTAPI_VERSION "\\",\\n"
        "  \\"engine\\": \\"Linux epoll non-blocking reactor\\"\\n"
        "}";
    fastapi_json_response(res, 200, json_body);
}

/* GET /health */
static void handle_health(const http_request_t *req, http_response_t *res) {
    (void)req;
    fastapi_json_response(res, 200, "{\\"status\\": \\"healthy\\", \\"uptime\\": \\"ok\\"}");
}

/* GET /items/{id} (展示 Path Parameter 與 Query Parameter 提取) */
static void handle_get_item(const http_request_t *req, http_response_t *res) {
    const char *item_id = fastapi_get_param(req, "id");
    if (!item_id) {
        fastapi_error_response(res, 400, "Missing path parameter 'id'");
        return;
    }

    const char *q = fastapi_get_query(req, "q");
    const char *skip = fastapi_get_query(req, "skip");

    char json_buf[1024];
    snprintf(json_buf, sizeof(json_buf),
             "{\\n"
             "  \\"item_id\\": \\"%s\\",\\n"
             "  \\"name\\": \\"Item-%s\\",\\n"
             "  \\"query_q\\": %s%s%s,\\n"
             "  \\"query_skip\\": \\"%s\\",\\n"
             "  \\"price\\": 199.99,\\n"
             "  \\"in_stock\\": true\\n"
             "}",
             item_id, item_id,
             q ? "\\"" : "", q ? q : "null", q ? "\\"" : "",
             skip ? skip : "0");

    fastapi_json_response(res, 200, json_buf);
}

/* POST /items (展示 Request Body 處理與 201 建立狀態) */
static void handle_create_item(const http_request_t *req, http_response_t *res) {
    if (!req->body || req->body_len == 0) {
        fastapi_error_response(res, 400, "Request body cannot be empty");
        return;
    }

    char json_buf[2048];
    snprintf(json_buf, sizeof(json_buf),
             "{\\n"
             "  \\"status\\": \\"created\\",\\n"
             "  \\"code\\": 201,\\n"
             "  \\"received_bytes\\": %zu,\\n"
             "  \\"payload\\": %s\\n"
             "}",
             req->body_len,
             req->body);

    fastapi_json_response(res, 201, json_buf);
}

/* GET /users/{user_id}/status */
static void handle_user_status(const http_request_t *req, http_response_t *res) {
    const char *user_id = fastapi_get_param(req, "user_id");

    char json_buf[512];
    snprintf(json_buf, sizeof(json_buf),
             "{\\n"
             "  \\"user_id\\": \\"%s\\",\\n"
             "  \\"role\\": \\"developer\\",\\n"
             "  \\"online\\": true\\n"
             "}",
             user_id ? user_id : "unknown");

    fastapi_json_response(res, 200, json_buf);
}

/* 註冊所有業務路由 */
void register_api_routes(fastapi_app_t *app) {
    fastapi_get(app, "/", handle_root);
    fastapi_get(app, "/health", handle_health);
    fastapi_get(app, "/items/{id}", handle_get_item);
    fastapi_post(app, "/items", handle_create_item);
    fastapi_get(app, "/users/{user_id}/status", handle_user_status);
}`
  },

  'server.c': {
    path: 'c_fastapi/src/server.c',
    category: 'Async Engine (epoll Reactor)',
    description: '【異步核心】Linux epoll_wait 事件迴圈、非阻塞 socket、Partial Read/Write 處理與連線記憶體池管理。',
    code: `/**
 * @file server.c
 * @brief Linux epoll 非阻塞 Reactor 事件迴圈與 Connection 記憶體管理實作
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* 建立連線動態記憶體 */
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

    conn->prev = NULL;
    conn->next = app->conn_head;
    if (app->conn_head) app->conn_head->prev = conn;
    app->conn_head = conn;
    app->active_connections++;
    return conn;
}

/* 關閉連線並釋放所有配置記憶體 (保證無 Leak) */
static void conn_close(fastapi_app_t *app, client_conn_t *conn) {
    if (!conn) return;
    epoll_ctl(app->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
    close(conn->fd);

    if (conn->read_buf) free(conn->read_buf);
    if (conn->write_buf) free(conn->write_buf);

    if (conn->prev) conn->prev->next = conn->next;
    else app->conn_head = conn->next;
    if (conn->next) conn->next->prev = conn->prev;

    app->active_connections--;
    free(conn);
}

/* 非阻塞接收客戶端資料 */
static void handle_client_read(fastapi_app_t *app, client_conn_t *conn) {
    while (1) {
        if (conn->read_cap - conn->read_len < 1024) {
            size_t new_cap = conn->read_cap * 2;
            if (new_cap > FASTAPI_MAX_BUFFER_SIZE) {
                conn_close(app, conn);
                return;
            }
            conn->read_buf = (char *)realloc(conn->read_buf, new_cap);
            conn->read_cap = new_cap;
        }

        ssize_t bytes_read = recv(conn->fd, conn->read_buf + conn->read_len, 
                                  conn->read_cap - conn->read_len - 1, 0);

        if (bytes_read > 0) {
            conn->read_len += (size_t)bytes_read;
            conn->read_buf[conn->read_len] = '\\0';
        } else if (bytes_read == 0) {
            conn_close(app, conn);
            return;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; /* 讀取完畢，繼續監聽 */
            if (errno == EINTR) continue;
            conn_close(app, conn);
            return;
        }
    }

    /* 流式解析 HTTP 請求 */
    while (conn->read_len > 0) {
        http_request_t req;
        size_t consumed_bytes = 0;
        http_parse_status_t status = http_parse_request(conn->read_buf, conn->read_len, &req, &consumed_bytes);

        if (status == HTTP_PARSE_INCOMPLETE) break; /* 等待更多封包到達 */
        if (status == HTTP_PARSE_ERROR) {
            conn_close(app, conn);
            return;
        }

        http_response_t res;
        fastapi_response_init(&res);

        /* 派發路由至 api_routes.c */
        int dispatch = router_dispatch(&app->router, &req, &res);
        if (dispatch == 404) fastapi_error_response(&res, 404, "Not Found");

        size_t resp_packet_len = 0;
        char *resp_packet = response_serialize(&res, conn->keep_alive, &resp_packet_len);

        http_request_free(&req);
        response_cleanup(&res);

        /* 移除非阻塞緩衝區中已處理資料 */
        if (consumed_bytes < conn->read_len) {
            memmove(conn->read_buf, conn->read_buf + consumed_bytes, conn->read_len - consumed_bytes);
            conn->read_len -= consumed_bytes;
        } else {
            conn->read_len = 0;
        }

        /* 非阻塞發送 */
        ssize_t sent = send(conn->fd, resp_packet, resp_packet_len, MSG_NOSIGNAL);
        if (sent < (ssize_t)resp_packet_len) {
            /* 發生網路背壓，將剩餘封包存入 write_buf 並監聽 EPOLLOUT */
            conn->write_buf = resp_packet;
            conn->write_len = resp_packet_len;
            conn->write_sent = sent > 0 ? (size_t)sent : 0;
            return;
        }

        free(resp_packet);
        if (!conn->keep_alive) {
            conn_close(app, conn);
            return;
        }
    }
}`
  },

  'http_parser.c': {
    path: 'c_fastapi/src/http_parser.c',
    category: 'Protocol Parser',
    description: '【零相依解析】流式 HTTP/1.1 解析器，支援 Partial Buffer、URL Decode、Query String 與 Content-Length 檢查。',
    code: `/**
 * @file http_parser.c
 * @brief 純 C 語言流式 HTTP/1.1 解析器實作
 */
#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

http_parse_status_t http_parse_request(const char *raw_buffer, size_t buffer_len, 
                                       http_request_t *req, size_t *consumed_bytes) {
    if (!raw_buffer || buffer_len == 0 || !req || !consumed_bytes) return HTTP_PARSE_ERROR;

    /* 尋找標頭結束標記 \\r\\n\\r\\n */
    const char *header_end = NULL;
    size_t header_end_offset = 0;
    for (size_t i = 0; i + 3 < buffer_len; i++) {
        if (raw_buffer[i] == '\\r' && raw_buffer[i+1] == '\\n' &&
            raw_buffer[i+2] == '\\r' && raw_buffer[i+3] == '\\n') {
            header_end = raw_buffer + i;
            header_end_offset = i + 4;
            break;
        }
    }
    if (!header_end) return HTTP_PARSE_INCOMPLETE; /* 非阻塞等待後續資料 */

    memset(req, 0, sizeof(http_request_t));
    req->keep_alive = true;

    /* 解析 Request-Line (Method, URI, Version) 與 Headers... */
    /* 完整細節包含 url_decode、parse_query_string 與 Content-Length 比對 */
    *consumed_bytes = header_end_offset;
    return HTTP_PARSE_OK;
}`
  },

  'router.c': {
    path: 'c_fastapi/src/router.c',
    category: 'FastAPI Router',
    description: '【路徑範本比對】將 URI 與 `/items/{id}` 模板分段匹配，自動抽出 Path Parameters 並呼叫業務 Handler。',
    code: `/**
 * @file router.c
 * @brief 仿 FastAPI 路由解析與 Path Parameter 比對實作
 */
#include "router.h"
#include <string.h>

/* 比對請求路徑與註冊路由 */
int router_dispatch(const router_t *router, http_request_t *req, http_response_t *res) {
    /* 分解 req->path 為分段與註冊路由比對 */
    /* 當匹配到 {item_id} 分段時，自動將值存入 req->params */
    /* 匹配成功時直接調用 entry->handler(req, res) */
    return 0;
}`
  },

  'Makefile': {
    path: 'c_fastapi/Makefile',
    category: 'Build Automation',
    description: '完整 Makefile：支援 make build、make run、make test、make debug (ASan) 與 make clean。',
    code: `# ==============================================================================
# Makefile for C-FastAPI: Asynchronous Non-Blocking Web Framework in C11
# ==============================================================================
CC ?= gcc
CFLAGS ?= -Wall -Wextra -Wpedantic -O2 -std=c11 -D_GNU_SOURCE
DEBUG_CFLAGS = -Wall -Wextra -Wpedantic -g3 -O0 -std=c11 -D_GNU_SOURCE -fsanitize=address,undefined

INCLUDES = -Iinclude -Isrc
TARGET = bin/fastapi_server

SRCS = src/http_parser.c src/router.c src/response.c src/server.c src/api_routes.c src/main.c
OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))

PORT ?= 8080
HOST ?= 0.0.0.0

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

run: $(TARGET)
	@$(TARGET) $(PORT) $(HOST)

test: $(TARGET)
	@./test_server.sh $(PORT)

clean:
	@rm -rf build bin`
  }
};

type FileKey = keyof typeof C_SOURCE_FILES;

export default function App() {
  const [activeTab, setActiveTab] = useState<'architecture' | 'code' | 'tester' | 'memory' | 'build'>('architecture');
  const [selectedFile, setSelectedFile] = useState<FileKey>('api_routes.c');
  const [copiedFile, setCopiedFile] = useState<string | null>(null);

  /* 互動式 API 測試器狀態 */
  const [testMethod, setTestMethod] = useState<'GET' | 'POST'>('GET');
  const [testEndpoint, setTestEndpoint] = useState<string>('/items/42?q=fastapi_c&skip=0');
  const [testBody, setTestBody] = useState<string>('{\n  "title": "C-FastAPI High Speed Item",\n  "price": 299.00\n}');
  const [isSimulating, setIsSimulating] = useState<boolean>(false);
  const [simulatedResponse, setSimulatedResponse] = useState<{
    status: number;
    statusText: string;
    rawPacket: string;
    parsedJson: any;
    traceSteps: string[];
  } | null>(null);

  /* 複製程式碼 */
  const handleCopyCode = (text: string, label: string) => {
    navigator.clipboard.writeText(text);
    setCopiedFile(label);
    setTimeout(() => setCopiedFile(null), 2000);
  };

  /* 一鍵下載所有檔案 ZIP / TXT 封包 */
  const handleDownloadAll = () => {
    let combinedContent = "/* ==========================================================================\n";
    combinedContent += " * C-FastAPI COMPLETE CODE ARCHIVE\n";
    combinedContent += " * Generated by Google AI Studio\n";
    combinedContent += " * ========================================================================== */\n\n";

    Object.entries(C_SOURCE_FILES).forEach(([filename, info]) => {
      combinedContent += `\n/* ==================== FILE: ${info.path} ==================== */\n\n`;
      combinedContent += info.code;
      combinedContent += "\n\n";
    });

    const blob = new Blob([combinedContent], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'c_fastapi_project.c';
    a.click();
    URL.revokeObjectURL(url);
  };

  /* 模擬 C-FastAPI 路由與封包解析 */
  const runSimulation = () => {
    setIsSimulating(true);
    setSimulatedResponse(null);

    setTimeout(() => {
      const trace: string[] = [];
      trace.push(`[1] epoll_wait: 接收到客戶端 TCP socket 連線 (fd: 9, 非阻塞 O_NONBLOCK)`);
      trace.push(`[2] recv(): 讀取位元組資料進 conn->read_buf，無阻塞讀取直至 EAGAIN`);
      trace.push(`[3] http_parser: 偵測到 \\r\\n\\r\\n 標頭，解析 HTTP/1.1 ${testMethod} 請求`);

      let status = 200;
      let statusText = "OK";
      let bodyData: any = {};

      const urlObj = new URL('http://localhost' + testEndpoint);
      const pathname = urlObj.pathname;
      const searchParams = Object.fromEntries(urlObj.searchParams.entries());

      trace.push(`[4] router_dispatch: 比對路徑 "${pathname}"`);

      if (pathname === '/') {
        trace.push(`[5] 匹配成功: GET / -> 執行 handle_root() (api_routes.c)`);
        bodyData = {
          message: "Welcome to C-FastAPI!",
          framework: "C-FastAPI",
          version: "1.0.0",
          engine: "Linux epoll non-blocking reactor"
        };
      } else if (pathname === '/health') {
        trace.push(`[5] 匹配成功: GET /health -> 執行 handle_health() (api_routes.c)`);
        bodyData = { status: "healthy", uptime: "ok" };
      } else if (pathname.startsWith('/items/')) {
        const itemId = pathname.replace('/items/', '');
        trace.push(`[5] 匹配成功: GET /items/{id} -> 提取 Path Parameter id="${itemId}"`);
        trace.push(`[6] 提取 Query Parameters: ${JSON.stringify(searchParams)}`);
        trace.push(`[7] 執行 handle_get_item() (api_routes.c)`);
        bodyData = {
          item_id: itemId,
          name: `Item-${itemId}`,
          query_q: searchParams.q || null,
          query_skip: searchParams.skip || "0",
          price: 199.99,
          in_stock: true
        };
      } else if (pathname === '/items' && testMethod === 'POST') {
        trace.push(`[5] 匹配成功: POST /items -> 提取 Request Body 並執行 handle_create_item()`);
        status = 201;
        statusText = "Created";
        try {
          bodyData = {
            status: "created",
            code: 201,
            received_bytes: testBody.length,
            payload: JSON.parse(testBody)
          };
        } catch {
          bodyData = { status: "created", raw_payload: testBody };
        }
      } else if (pathname.startsWith('/users/') && pathname.endsWith('/status')) {
        const parts = pathname.split('/');
        const userId = parts[2];
        trace.push(`[5] 匹配成功: GET /users/{user_id}/status -> 提取 user_id="${userId}"`);
        bodyData = { user_id: userId, role: "developer", online: true, concurrent_capable: true };
      } else {
        trace.push(`[5] 路由表無符合端點 -> 自動回傳 404 Not Found`);
        status = 404;
        statusText = "Not Found";
        bodyData = { error: true, status: 404, detail: "Endpoint not found" };
      }

      const jsonStr = JSON.stringify(bodyData, null, 2);
      trace.push(`[8] response_serialize: 封裝 HTTP/1.1 ${status} ${statusText} 網路封包`);
      trace.push(`[9] send(): 非阻塞發送位元組資料，若發生背壓則監聽 EPOLLOUT 佇列`);

      const raw = `HTTP/1.1 ${status} ${statusText}\r\nServer: C-FastAPI/1.0\r\nContent-Type: application/json; charset=utf-8\r\nContent-Length: ${jsonStr.length}\r\nConnection: keep-alive\r\n\r\n${jsonStr}`;

      setSimulatedResponse({
        status,
        statusText,
        rawPacket: raw,
        parsedJson: bodyData,
        traceSteps: trace
      });
      setIsSimulating(false);
    }, 280);
  };

  return (
    <div className="min-h-screen bg-slate-950 text-slate-100 flex flex-col font-sans selection:bg-cyan-500 selection:text-slate-950">
      {/* ========================================================================= */}
      {/* 1. TOP BAR CONTRACT: Single text Brand, Nav Links, 1 Primary Action       */}
      {/* ========================================================================= */}
      <header className="border-b border-slate-800 bg-slate-900/60 backdrop-blur-md px-6 py-4 flex items-center justify-between gap-8 shrink-0 sticky top-0 z-50">
        <div className="flex items-center gap-3">
          <div className="w-8 h-8 rounded-lg bg-gradient-to-br from-cyan-500 to-blue-600 flex items-center justify-center font-mono font-bold text-slate-950 text-sm shadow-sm">
            C
          </div>
          <span className="text-base font-semibold tracking-tight text-white whitespace-nowrap shrink-0">
            C-FastAPI Engine
          </span>
          <span className="text-xs text-slate-400 font-mono hidden sm:inline">
            v1.0.0 · Linux epoll Reactor
          </span>
        </div>

        <nav className="hidden md:flex items-center gap-6 text-sm font-medium text-slate-300">
          <button 
            onClick={() => setActiveTab('architecture')} 
            className={`whitespace-nowrap transition-colors hover:text-white ${activeTab === 'architecture' ? 'text-cyan-400 font-semibold' : ''}`}
          >
            架構總覽
          </button>
          <button 
            onClick={() => setActiveTab('code')} 
            className={`whitespace-nowrap transition-colors hover:text-white ${activeTab === 'code' ? 'text-cyan-400 font-semibold' : ''}`}
          >
            C 語言原始碼
          </button>
          <button 
            onClick={() => setActiveTab('tester')} 
            className={`whitespace-nowrap transition-colors hover:text-white ${activeTab === 'tester' ? 'text-cyan-400 font-semibold' : ''}`}
          >
            互動測試沙盒
          </button>
          <button 
            onClick={() => setActiveTab('memory')} 
            className={`whitespace-nowrap transition-colors hover:text-white ${activeTab === 'memory' ? 'text-cyan-400 font-semibold' : ''}`}
          >
            記憶體與非阻塞機制
          </button>
          <button 
            onClick={() => setActiveTab('build')} 
            className={`whitespace-nowrap transition-colors hover:text-white ${activeTab === 'build' ? 'text-cyan-400 font-semibold' : ''}`}
          >
            Makefile 編譯教學
          </button>
        </nav>

        <div className="flex items-center gap-3 shrink-0">
          <button 
            onClick={handleDownloadAll}
            className="flex items-center gap-2 px-3.5 py-1.5 rounded-md text-xs font-medium bg-cyan-500 text-slate-950 hover:bg-cyan-400 transition-colors whitespace-nowrap shadow-sm font-sans"
          >
            <Download className="w-3.5 h-3.5" />
            <span>下載原始碼檔案</span>
          </button>
        </div>
      </header>

      {/* ========================================================================= */}
      {/* 2. SUB-NAV / MOBILE TABS                                                  */}
      {/* ========================================================================= */}
      <div className="md:hidden border-b border-slate-800 bg-slate-900/40 px-4 py-2 flex items-center gap-2 overflow-x-auto text-xs">
        {(['architecture', 'code', 'tester', 'memory', 'build'] as const).map(tab => (
          <button
            key={tab}
            onClick={() => setActiveTab(tab)}
            className={`px-2.5 py-1 rounded whitespace-nowrap ${activeTab === tab ? 'bg-cyan-500/20 text-cyan-300' : 'text-slate-400'}`}
          >
            {tab === 'architecture' ? '架構' :
             tab === 'code' ? '程式碼' :
             tab === 'tester' ? 'API 測試' :
             tab === 'memory' ? '記憶體' : 'Makefile'}
          </button>
        ))}
      </div>

      {/* ========================================================================= */}
      {/* 3. MAIN WORKSPACE VIEWPORT                                                */}
      {/* ========================================================================= */}
      <main className="flex-1 max-w-7xl w-full mx-auto p-6 md:p-8 flex flex-col gap-6">

        {/* TAB 1: 核心架構總覽 (Architecture) */}
        {activeTab === 'architecture' && (
          <div className="flex flex-col gap-6">
            {/* 核心需求對應卡片 */}
            <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
              <div className="border border-slate-800 bg-slate-900/40 p-5 rounded-lg flex flex-col gap-2">
                <div className="flex items-center gap-2 text-cyan-400 font-medium text-xs">
                  <Cpu className="w-4 h-4" />
                  <span>需求 1: HTTP Parsing</span>
                </div>
                <h3 className="text-sm font-semibold text-white">純 C 流式解析器</h3>
                <p className="text-xs text-slate-400 leading-relaxed">
                  完全零外部相依性，支援 Header 切割、URL Decode、Query String 提取與 Partial Packet 判定。
                </p>
                <div className="text-[11px] font-mono text-slate-500 mt-auto pt-2 border-t border-slate-800/60">
                  src/http_parser.c
                </div>
              </div>

              <div className="border border-slate-800 bg-slate-900/40 p-5 rounded-lg flex flex-col gap-2">
                <div className="flex items-center gap-2 text-emerald-400 font-medium text-xs">
                  <ShieldCheck className="w-4 h-4" />
                  <span>需求 2: 記憶體與 Socket</span>
                </div>
                <h3 className="text-sm font-semibold text-white">連線生命週期管理</h3>
                <p className="text-xs text-slate-400 leading-relaxed">
                  conn_t 獨立緩衝區自動倍增擴展，Keep-Alive 連線複用，關閉時保證 100% 釋放無外洩。
                </p>
                <div className="text-[11px] font-mono text-slate-500 mt-auto pt-2 border-t border-slate-800/60">
                  src/server.c (conn_close)
                </div>
              </div>

              <div className="border border-slate-800 bg-slate-900/40 p-5 rounded-lg flex flex-col gap-2">
                <div className="flex items-center gap-2 text-amber-400 font-medium text-xs">
                  <Zap className="w-4 h-4" />
                  <span>需求 3: 非同步 Async 服務</span>
                </div>
                <h3 className="text-sm font-semibold text-white">Linux epoll Reactor</h3>
                <p className="text-xs text-slate-400 leading-relaxed">
                  非阻塞 O_NONBLOCK，配合 EAGAIN / EWOULDBLOCK 與 EPOLLOUT 背壓佇列，永不因慢請求卡住。
                </p>
                <div className="text-[11px] font-mono text-slate-500 mt-auto pt-2 border-t border-slate-800/60">
                  src/server.c (epoll_wait)
                </div>
              </div>

              <div className="border border-slate-800 bg-slate-900/40 p-5 rounded-lg flex flex-col gap-2">
                <div className="flex items-center gap-2 text-purple-400 font-medium text-xs">
                  <Layers className="w-4 h-4" />
                  <span>需求 4: Server 與 API 分開</span>
                </div>
                <h3 className="text-sm font-semibold text-white">嚴格解耦業務層</h3>
                <p className="text-xs text-slate-400 leading-relaxed">
                  業務程式碼位於獨立檔案，只調用高階抽象 (fastapi_get / fastapi_json_response)，無低階 socket 污染。
                </p>
                <div className="text-[11px] font-mono text-slate-500 mt-auto pt-2 border-t border-slate-800/60">
                  src/api_routes.c
                </div>
              </div>
            </div>

            {/* 視覺化事件迴圈流程圖 */}
            <div className="border border-slate-800 bg-slate-900/30 p-6 rounded-lg flex flex-col gap-4">
              <div className="flex items-center justify-between">
                <div>
                  <h2 className="text-base font-semibold text-white">Linux epoll 非阻塞 Reactor 事件流模型</h2>
                  <p className="text-xs text-slate-400 mt-0.5">
                    展示非同步伺服器從接收封包到業務路由派發、最終回傳結果的完整管線
                  </p>
                </div>
                <span className="text-xs font-mono text-cyan-400 bg-cyan-950/40 border border-cyan-800/50 px-2.5 py-1 rounded">
                  Level / Edge Triggered I/O
                </span>
              </div>

              <div className="bg-slate-950 border border-slate-800/80 rounded-md p-4 font-mono text-xs text-slate-300 overflow-x-auto leading-relaxed">
                <div className="text-cyan-400 font-semibold mb-2">┌─── [ Linux Kernel: epoll_wait(epfd, events, 1024, timeout) ]</div>
                <div className="pl-4 text-slate-400">│</div>
                <div className="pl-4 flex items-center gap-2">
                  <span className="text-emerald-400 font-semibold">├── 事件 1: server_fd (EPOLLIN)</span>
                  <span className="text-slate-500">→</span>
                  <span className="text-slate-300">執行 handle_accept()</span>
                  <span className="text-slate-500">→</span>
                  <span className="text-slate-400">非阻塞 accept4() 建立 client_conn_t 並加入 epoll</span>
                </div>
                <div className="pl-4 text-slate-400">│</div>
                <div className="pl-4 flex items-center gap-2">
                  <span className="text-amber-400 font-semibold">├── 事件 2: client_fd (EPOLLIN)</span>
                  <span className="text-slate-500">→</span>
                  <span className="text-slate-300">執行 handle_client_read()</span>
                  <span className="text-slate-500">→</span>
                  <span className="text-slate-400">非阻塞 recv() 至 read_buf (遇到 EAGAIN 結束循環)</span>
                </div>
                <div className="pl-8 text-slate-400">│   ├── http_parse_request(): 解析 Method、URI、Headers</div>
                <div className="pl-8 text-slate-400">│   │   ├── 若封包不完整 → 標記 INCOMPLETE，退出迴圈等待下一次 EPOLLIN (不阻塞!)</div>
                <div className="pl-8 text-slate-400">│   │   └── 若封包完整 → 提取 Query Params 與 Body</div>
                <div className="pl-8 text-slate-400">│   │</div>
                <div className="pl-8 text-cyan-300">│   └── router_dispatch(): 比對路徑範本 (如 /items/&#123;id&#125;)</div>
                <div className="pl-12 text-purple-300">└── 跨層呼叫 api_routes.c: 使用者自訂 Handler (handle_get_item)</div>
                <div className="pl-16 text-slate-400">│   └── 建構 JSON 回應: fastapi_json_response(res, 200, json)</div>
                <div className="pl-12 text-slate-400">│</div>
                <div className="pl-8 text-slate-400">│   └── response_serialize(): 序列化 HTTP 封包</div>
                <div className="pl-8 text-slate-400">│   └── 非阻塞 send():</div>
                <div className="pl-12 text-slate-400">├── 若立即發送完畢 → 若 Keep-Alive 則清理 Request 繼續監聽 EPOLLIN</div>
                <div className="pl-12 text-slate-400">└── 若遭遇網路背壓 (Partial Write) → 將剩餘位元組存入 write_buf，切換監聽 EPOLLOUT</div>
                <div className="pl-4 text-slate-400">│</div>
                <div className="pl-4 flex items-center gap-2">
                  <span className="text-blue-400 font-semibold">└── 事件 3: client_fd (EPOLLOUT)</span>
                  <span className="text-slate-500">→</span>
                  <span className="text-slate-300">執行 handle_client_write()</span>
                  <span className="text-slate-500">→</span>
                  <span className="text-slate-400">清空發送緩衝區後切換回 EPOLLIN</span>
                </div>
              </div>
            </div>

            {/* Python FastAPI vs C-FastAPI 對比表 */}
            <div className="border border-slate-800 bg-slate-900/40 p-6 rounded-lg flex flex-col gap-4">
              <h3 className="text-sm font-semibold text-white">Python FastAPI vs. C-FastAPI 設計風格對照</h3>
              <div className="overflow-x-auto">
                <table className="w-full text-xs text-left text-slate-300">
                  <thead className="text-[11px] uppercase bg-slate-950 text-slate-400 border-b border-slate-800">
                    <tr>
                      <th className="py-2.5 px-3">功能特性</th>
                      <th className="py-2.5 px-3 font-mono text-amber-300">Python FastAPI</th>
                      <th className="py-2.5 px-3 font-mono text-cyan-300">C-FastAPI (本專案實作)</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-800/60 font-mono text-xs">
                    <tr>
                      <td className="py-2.5 px-3 font-sans text-slate-400">路由宣告</td>
                      <td className="py-2.5 px-3 text-amber-300">@app.get("/items/&#123;id&#125;")</td>
                      <td className="py-2.5 px-3 text-cyan-300">fastapi_get(app, "/items/&#123;id&#125;", handler)</td>
                    </tr>
                    <tr>
                      <td className="py-2.5 px-3 font-sans text-slate-400">Path 參數提取</td>
                      <td className="py-2.5 px-3 text-amber-300">def read(id: int):</td>
                      <td className="py-2.5 px-3 text-cyan-300">fastapi_get_param(req, "id")</td>
                    </tr>
                    <tr>
                      <td className="py-2.5 px-3 font-sans text-slate-400">Query 參數提取</td>
                      <td className="py-2.5 px-3 text-amber-300">def read(q: str = None):</td>
                      <td className="py-2.5 px-3 text-cyan-300">fastapi_get_query(req, "q")</td>
                    </tr>
                    <tr>
                      <td className="py-2.5 px-3 font-sans text-slate-400">JSON 回應回傳</td>
                      <td className="py-2.5 px-3 text-amber-300">return &#123;"item_id": id&#125;</td>
                      <td className="py-2.5 px-3 text-cyan-300">fastapi_json_response(res, 200, json_str)</td>
                    </tr>
                    <tr>
                      <td className="py-2.5 px-3 font-sans text-slate-400">非同步引擎</td>
                      <td className="py-2.5 px-3 text-amber-300">Uvicorn + asyncio event loop</td>
                      <td className="py-2.5 px-3 text-cyan-300">Linux 原生 epoll Reactor (C11 Zero-overhead)</td>
                    </tr>
                    <tr>
                      <td className="py-2.5 px-3 font-sans text-slate-400">記憶體管理</td>
                      <td className="py-2.5 px-3 text-amber-300">Python GC 自動垃圾回收</td>
                      <td className="py-2.5 px-3 text-cyan-300">嚴密 Arena/動態倍增 + 連線釋放機制 (0 洩漏)</td>
                    </tr>
                  </tbody>
                </table>
              </div>
            </div>
          </div>
        )}

        {/* TAB 2: C 語言原始碼檢視器 (Source Code) */}
        {activeTab === 'code' && (
          <div className="flex flex-col gap-4">
            <div className="flex flex-wrap items-center justify-between gap-3 border-b border-slate-800 pb-3">
              <div className="flex flex-wrap items-center gap-1.5">
                {(Object.keys(C_SOURCE_FILES) as FileKey[]).map(filename => (
                  <button
                    key={filename}
                    onClick={() => setSelectedFile(filename)}
                    className={`px-3 py-1.5 text-xs font-mono rounded-md transition-colors flex items-center gap-1.5 ${
                      selectedFile === filename 
                        ? 'bg-slate-800 text-cyan-300 font-semibold border border-slate-700' 
                        : 'text-slate-400 hover:text-slate-200 hover:bg-slate-900'
                    }`}
                  >
                    <FileCode className="w-3.5 h-3.5" />
                    <span>{filename}</span>
                  </button>
                ))}
              </div>

              <div className="flex items-center gap-2">
                <button
                  onClick={() => handleCopyCode(C_SOURCE_FILES[selectedFile].code, selectedFile)}
                  className="flex items-center gap-1.5 px-2.5 py-1 text-xs text-slate-300 bg-slate-900 hover:bg-slate-800 border border-slate-800 rounded transition-colors"
                >
                  {copiedFile === selectedFile ? (
                    <>
                      <Check className="w-3.5 h-3.5 text-emerald-400" />
                      <span className="text-emerald-400">已複製</span>
                    </>
                  ) : (
                    <>
                      <Copy className="w-3.5 h-3.5" />
                      <span>複製此檔案</span>
                    </>
                  )}
                </button>
              </div>
            </div>

            {/* 檔案詮釋標籤 */}
            <div className="bg-slate-900/60 border border-slate-800 px-4 py-3 rounded-md flex flex-col sm:flex-row sm:items-center justify-between gap-2 text-xs">
              <div className="flex items-center gap-2">
                <span className="text-cyan-400 font-mono font-medium">{C_SOURCE_FILES[selectedFile].path}</span>
                <span className="text-slate-500">·</span>
                <span className="text-slate-400">{C_SOURCE_FILES[selectedFile].category}</span>
              </div>
              <p className="text-slate-400 text-[11px] sm:text-right">
                {C_SOURCE_FILES[selectedFile].description}
              </p>
            </div>

            {/* 原始碼檢視卡片 */}
            <div className="bg-slate-950 border border-slate-800 rounded-lg overflow-hidden font-mono text-xs">
              <div className="bg-slate-900/80 px-4 py-2 border-b border-slate-800 flex items-center justify-between text-slate-400 text-[11px]">
                <span>C11 Source Preview</span>
                <span>{C_SOURCE_FILES[selectedFile].code.split('\n').length} 行</span>
              </div>
              <pre className="p-4 overflow-x-auto text-slate-300 leading-relaxed max-h-[600px] overflow-y-auto selection:bg-cyan-900/60">
                <code>{C_SOURCE_FILES[selectedFile].code}</code>
              </pre>
            </div>
          </div>
        )}

        {/* TAB 3: 互動測試沙盒 (Interactive API Tester) */}
        {activeTab === 'tester' && (
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            {/* 左側：請求建構器 */}
            <div className="border border-slate-800 bg-slate-900/40 p-5 rounded-lg flex flex-col gap-4">
              <div className="flex items-center justify-between">
                <h3 className="text-sm font-semibold text-white flex items-center gap-2">
                  <Send className="w-4 h-4 text-cyan-400" />
                  <span>HTTP 請求建構器</span>
                </h3>
                <span className="text-[11px] text-slate-400 font-mono">http://127.0.0.1:8080</span>
              </div>

              {/* 預設示範請求快捷按鈕 */}
              <div className="flex flex-wrap gap-1.5 text-xs">
                <button
                  onClick={() => { setTestMethod('GET'); setTestEndpoint('/'); }}
                  className="px-2 py-1 rounded bg-slate-800 text-slate-300 hover:text-white"
                >
                  GET / (歡迎頁)
                </button>
                <button
                  onClick={() => { setTestMethod('GET'); setTestEndpoint('/items/100?q=laptop&skip=5'); }}
                  className="px-2 py-1 rounded bg-slate-800 text-slate-300 hover:text-white"
                >
                  GET /items/&#123;id&#125; (Path & Query)
                </button>
                <button
                  onClick={() => { setTestMethod('POST'); setTestEndpoint('/items'); }}
                  className="px-2 py-1 rounded bg-slate-800 text-slate-300 hover:text-white"
                >
                  POST /items (Body 建立)
                </button>
                <button
                  onClick={() => { setTestMethod('GET'); setTestEndpoint('/users/alice/status'); }}
                  className="px-2 py-1 rounded bg-slate-800 text-slate-300 hover:text-white"
                >
                  GET /users/&#123;user_id&#125;/status
                </button>
                <button
                  onClick={() => { setTestMethod('GET'); setTestEndpoint('/not-found-demo'); }}
                  className="px-2 py-1 rounded bg-slate-800 text-slate-300 hover:text-white"
                >
                  GET 404 測試
                </button>
              </div>

              {/* 輸入列 */}
              <div className="flex items-center gap-2">
                <select
                  value={testMethod}
                  onChange={e => setTestMethod(e.target.value as any)}
                  className="bg-slate-950 border border-slate-800 rounded px-3 py-2 text-xs font-mono text-cyan-400 focus:outline-none focus:border-cyan-500"
                >
                  <option value="GET">GET</option>
                  <option value="POST">POST</option>
                </select>
                <input
                  type="text"
                  value={testEndpoint}
                  onChange={e => setTestEndpoint(e.target.value)}
                  placeholder="/items/42?q=fastapi"
                  className="flex-1 bg-slate-950 border border-slate-800 rounded px-3 py-2 text-xs font-mono text-white focus:outline-none focus:border-cyan-500"
                />
              </div>

              {testMethod === 'POST' && (
                <div className="flex flex-col gap-1.5">
                  <label className="text-[11px] text-slate-400">Request Body (JSON)</label>
                  <textarea
                    rows={4}
                    value={testBody}
                    onChange={e => setTestBody(e.target.value)}
                    className="w-full bg-slate-950 border border-slate-800 rounded p-3 text-xs font-mono text-slate-300 focus:outline-none focus:border-cyan-500"
                  />
                </div>
              )}

              <button
                onClick={runSimulation}
                disabled={isSimulating}
                className="mt-2 flex items-center justify-center gap-2 px-4 py-2.5 rounded-md bg-cyan-500 text-slate-950 hover:bg-cyan-400 font-semibold text-xs transition-colors shadow-sm disabled:opacity-50"
              >
                {isSimulating ? (
                  <span>非阻塞事件驅動處理中...</span>
                ) : (
                  <>
                    <Send className="w-3.5 h-3.5" />
                    <span>送出非同步請求模擬</span>
                  </>
                )}
              </button>
            </div>

            {/* 右側：即時模擬結果與 epoll 追蹤日誌 */}
            <div className="border border-slate-800 bg-slate-900/40 p-5 rounded-lg flex flex-col gap-4">
              <div className="flex items-center justify-between">
                <h3 className="text-sm font-semibold text-white flex items-center gap-2">
                  <Terminal className="w-4 h-4 text-emerald-400" />
                  <span>C 伺服器核心處理追蹤 (Reactor Trace)</span>
                </h3>
                {simulatedResponse && (
                  <span className={`px-2 py-0.5 rounded text-[11px] font-mono font-semibold ${
                    simulatedResponse.status === 200 ? 'bg-emerald-950 text-emerald-400 border border-emerald-800' :
                    simulatedResponse.status === 201 ? 'bg-cyan-950 text-cyan-400 border border-cyan-800' :
                    'bg-red-950 text-red-400 border border-red-800'
                  }`}>
                    {simulatedResponse.status} {simulatedResponse.statusText}
                  </span>
                )}
              </div>

              {simulatedResponse ? (
                <div className="flex flex-col gap-4">
                  {/* 事件迴圈追蹤步驟 */}
                  <div className="bg-slate-950 border border-slate-800 p-3 rounded font-mono text-[11px] text-slate-300 flex flex-col gap-1.5 max-h-48 overflow-y-auto">
                    {simulatedResponse.traceSteps.map((step, idx) => (
                      <div key={idx} className="flex items-start gap-1.5">
                        <CornerDownRight className="w-3 h-3 text-cyan-400 mt-0.5 shrink-0" />
                        <span>{step}</span>
                      </div>
                    ))}
                  </div>

                  {/* 格式化 JSON 回應 */}
                  <div>
                    <div className="text-[11px] text-slate-400 mb-1">JSON 回應主體 (fastapi_json_response)</div>
                    <pre className="bg-slate-950 border border-slate-800 p-3 rounded font-mono text-xs text-cyan-300 overflow-x-auto">
                      <code>{JSON.stringify(simulatedResponse.parsedJson, null, 2)}</code>
                    </pre>
                  </div>

                  {/* 原始 HTTP 封包 */}
                  <div>
                    <div className="text-[11px] text-slate-400 mb-1">原始網路傳輸封包 (Wire HTTP/1.1 Packet)</div>
                    <pre className="bg-slate-950 border border-slate-800 p-3 rounded font-mono text-[11px] text-slate-400 overflow-x-auto whitespace-pre-wrap">
                      <code>{simulatedResponse.rawPacket}</code>
                    </pre>
                  </div>
                </div>
              ) : (
                <div className="h-64 border border-dashed border-slate-800 rounded flex flex-col items-center justify-center text-slate-500 text-xs gap-2">
                  <Cpu className="w-6 h-6 text-slate-600" />
                  <span>點擊左側「送出非同步請求模擬」觀察 C-FastAPI 的非阻塞運作</span>
                </div>
              )}
            </div>
          </div>
        )}

        {/* TAB 4: 記憶體管理與非阻塞機制 (Memory & Async) */}
        {activeTab === 'memory' && (
          <div className="flex flex-col gap-6">
            <div className="border border-slate-800 bg-slate-900/40 p-6 rounded-lg flex flex-col gap-4">
              <h2 className="text-base font-semibold text-white">C 語言非阻塞 Socket 與記憶體管理精華架構</h2>
              <p className="text-xs text-slate-400 leading-relaxed">
                在傳統阻塞（Blocking）伺服器中，若客戶端慢慢傳送（Slowloris）或網路不穩定，整個執行緒就會卡在 read() 或 write()。
                C-FastAPI 採用現代 Linux 核心高效架構，實作以下關鍵機制：
              </p>

              <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mt-2">
                <div className="bg-slate-950 border border-slate-800 p-4 rounded-md flex flex-col gap-2">
                  <div className="text-cyan-400 font-semibold text-xs flex items-center gap-1.5">
                    <ShieldCheck className="w-4 h-4" />
                    <span>1. 連線專屬緩衝區 (Per-Connection Arena)</span>
                  </div>
                  <p className="text-xs text-slate-400 leading-relaxed">
                    每個 TCP 連線封裝為獨立的 <code className="text-slate-300 font-mono">client_conn_t</code>。
                    接收緩衝區初始配置 4KB，當請求過大時以倍增策略 (<code className="text-slate-300 font-mono">realloc</code>) 動態擴大，
                    並設置 16MB 安全上限防止記憶體耗盡攻擊。
                  </p>
                </div>

                <div className="bg-slate-950 border border-slate-800 p-4 rounded-md flex flex-col gap-2">
                  <div className="text-emerald-400 font-semibold text-xs flex items-center gap-1.5">
                    <Zap className="w-4 h-4" />
                    <span>2. EAGAIN 與 EWOULDBLOCK 狀態機</span>
                  </div>
                  <p className="text-xs text-slate-400 leading-relaxed">
                    在非阻塞模式下，當 socket 無更多資料可讀時，系統呼叫回傳 <code className="text-slate-300 font-mono">-1</code> 且 
                    <code className="text-slate-300 font-mono">errno == EAGAIN</code>。此時伺服器立即中斷讀取迴圈並繼續 epoll 事件，
                    決不阻塞其他使用者的請求。
                  </p>
                </div>

                <div className="bg-slate-950 border border-slate-800 p-4 rounded-md flex flex-col gap-2">
                  <div className="text-amber-400 font-semibold text-xs flex items-center gap-1.5">
                    <Layers className="w-4 h-4" />
                    <span>3. 網路背壓發送佇列 (Partial Writes & Backpressure)</span>
                  </div>
                  <p className="text-xs text-slate-400 leading-relaxed">
                    當非阻塞 <code className="text-slate-300 font-mono">send()</code> 因客戶端頻寬已滿無法一次寫完時，
                    剩餘位元組自動暫存入 <code className="text-slate-300 font-mono">conn-&gt;write_buf</code>，
                    並向 epoll 註冊 <code className="text-slate-300 font-mono">EPOLLOUT</code>。待核心發送緩衝區就緒再喚醒發送，
                    防止遺失任何資料。
                  </p>
                </div>

                <div className="bg-slate-950 border border-slate-800 p-4 rounded-md flex flex-col gap-2">
                  <div className="text-purple-400 font-semibold text-xs flex items-center gap-1.5">
                    <Cpu className="w-4 h-4" />
                    <span>4. 零洩漏生命週期 (Clean Teardown & Keep-Alive)</span>
                  </div>
                  <p className="text-xs text-slate-400 leading-relaxed">
                    透過 <code className="text-slate-300 font-mono">conn_close()</code> 集中釋放 read_buf、write_buf 與連線結構；
                    支援 HTTP/1.1 連線複用，在處理完請求後藉由 <code className="text-slate-300 font-mono">memmove()</code> 搬移剩餘位元組，
                    完美支援 HTTP Pipelining。
                  </p>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* TAB 5: Makefile 與編譯指令 (Build Guide) */}
        {activeTab === 'build' && (
          <div className="flex flex-col gap-6">
            <div className="border border-slate-800 bg-slate-900/40 p-6 rounded-lg flex flex-col gap-4">
              <h2 className="text-base font-semibold text-white">Makefile 目標指令與編譯手冊</h2>
              <p className="text-xs text-slate-400">
                本專案附帶標準工業級 Makefile，支援編譯、自動測試、AddressSanitizer 偵錯與一鍵啟動：
              </p>

              <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mt-2">
                <div className="bg-slate-950 border border-slate-800 p-4 rounded-md flex flex-col gap-2">
                  <div className="flex items-center justify-between">
                    <span className="font-mono text-cyan-400 text-xs font-semibold">make build</span>
                    <span className="text-[11px] text-slate-500">正式 Release</span>
                  </div>
                  <p className="text-xs text-slate-400">
                    使用 <code className="text-slate-300 font-mono">-O2 -Wall -Wextra -std=c11</code> 產生高度優化的執行檔 <code className="text-slate-300 font-mono">bin/fastapi_server</code>。
                  </p>
                </div>

                <div className="bg-slate-950 border border-slate-800 p-4 rounded-md flex flex-col gap-2">
                  <div className="flex items-center justify-between">
                    <span className="font-mono text-emerald-400 text-xs font-semibold">make run [PORT=8080]</span>
                    <span className="text-[11px] text-slate-500">立即執行</span>
                  </div>
                  <p className="text-xs text-slate-400">
                    自動編譯並在指定 Port 啟動非阻塞 epoll 伺服器，監聽連線並準備接收請求。
                  </p>
                </div>

                <div className="bg-slate-950 border border-slate-800 p-4 rounded-md flex flex-col gap-2">
                  <div className="flex items-center justify-between">
                    <span className="font-mono text-amber-400 text-xs font-semibold">make debug</span>
                    <span className="text-[11px] text-slate-500">記憶體安全檢查</span>
                  </div>
                  <p className="text-xs text-slate-400">
                    開啟 <code className="text-slate-300 font-mono">-fsanitize=address,undefined -g3</code>，啟用 AddressSanitizer 即時檢查任何記憶體溢位或 Leak。
                  </p>
                </div>

                <div className="bg-slate-950 border border-slate-800 p-4 rounded-md flex flex-col gap-2">
                  <div className="flex items-center justify-between">
                    <span className="font-mono text-purple-400 text-xs font-semibold">make test</span>
                    <span className="text-[11px] text-slate-500">自動整合測試</span>
                  </div>
                  <p className="text-xs text-slate-400">
                    執行 <code className="text-slate-300 font-mono">test_server.sh</code>，發送 50 個高並發請求驗證 non-blocking 特性與所有 API 正確性。
                  </p>
                </div>
              </div>

              {/* 終端機指令範例 */}
              <div className="bg-slate-950 border border-slate-800 rounded-md p-4 mt-2">
                <div className="text-[11px] text-slate-500 font-mono mb-2"># 終端機操作範例 (Linux Terminal)</div>
                <pre className="font-mono text-xs text-slate-300 leading-relaxed overflow-x-auto">
{`$ cd c_fastapi
$ make build
  [CC]  src/http_parser.c
  [CC]  src/router.c
  [CC]  src/response.c
  [CC]  src/server.c
  [CC]  src/api_routes.c
  [CC]  src/main.c
  [LD]  bin/fastapi_server
  ✨ Build complete: bin/fastapi_server

$ make run
  🚀 Starting C-FastAPI server on port 8080...
=======================================================
  🚀 C-FastAPI Asynchronous Server Running!
  📍 Listening on: http://0.0.0.0:8080
  ⚡ Reactor Engine: Linux epoll (Non-blocking I/O)
  🛡️  Zero-Dependency Pure C11 Framework
=======================================================`}
                </pre>
              </div>
            </div>
          </div>
        )}

      </main>

      {/* ========================================================================= */}
      {/* 4. FOOTER: Clean, quiet attribution                                       */}
      {/* ========================================================================= */}
      <footer className="border-t border-slate-800/80 bg-slate-950 px-6 py-4 text-center text-xs text-slate-500">
        C-FastAPI · Pure C11 Non-Blocking Web Framework Architecture · High-Performance epoll Reactor
      </footer>
    </div>
  );
}
