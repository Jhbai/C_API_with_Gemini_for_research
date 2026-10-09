/**
 * @file fastapi.h
 * @brief C-FastAPI: 仿照 Python FastAPI 的 C 語言高效非同步 Web 框架
 * 
 * 提供 HTTP 解析、動態路由匹配 (Path Parameters)、查詢參數 (Query String)、
 * 非阻塞 Socket I/O 與 JSON 回應建構等核心 API 介面。
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

/* HTTP 請求方法列舉 */
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

/* HTTP 鍵值對結構 (用於 Headers, Query, Path Params) */
typedef struct {
    char *key;
    char *value;
} http_key_value_t;

/* HTTP 請求物件 (傳遞給 API 處理函式) */
typedef struct {
    http_method_t method;
    const char *method_str;
    char *raw_path;        /* 原始 URI (包含 query string) */
    char *path;            /* 僅路徑部分 (例如 /items/42) */
    char *query_string;    /* Query 字串 (例如 q=test&page=1) */
    
    /* Headers 串列 */
    http_key_value_t headers[FASTAPI_MAX_HEADERS];
    size_t header_count;

    /* Path Parameters 字典 (例如 /items/{id} => id: 42) */
    http_key_value_t params[FASTAPI_MAX_PARAMS];
    size_t param_count;

    /* Query Parameters 字典 (例如 ?name=alice&age=20) */
    http_key_value_t query_params[FASTAPI_MAX_QUERY_PARAMS];
    size_t query_count;

    /* 請求主體 (Body) */
    char *body;
    size_t body_len;

    /* 內部狀態與 Keep-Alive 標記 */
    bool keep_alive;
    void *client_conn;     /* 指向底層非阻塞 connection 的私有指標 */
} http_request_t;

/* HTTP 回應物件 */
typedef struct {
    int status_code;
    const char *status_text;
    
    http_key_value_t headers[FASTAPI_MAX_HEADERS];
    size_t header_count;

    char *body;            /* 回應內容 (動態配置或字面常數) */
    size_t body_len;
    bool body_dynamically_allocated; /* 若為 true，框架傳送完後自動 free() */
} http_response_t;

/* 路由處理函式型別定義 (Handler Signature) */
typedef void (*route_handler_t)(const http_request_t *req, http_response_t *res);

/* 框架 App 結構宣告 (不透明結構封裝) */
typedef struct fastapi_app fastapi_app_t;

/* ---------------- API 介面宣告 ---------------- */

/**
 * @brief 建立新的 C-FastAPI 應用實例
 */
fastapi_app_t *fastapi_create(void);

/**
 * @brief 釋放 C-FastAPI 應用與相關資源
 */
void fastapi_destroy(fastapi_app_t *app);

/**
 * @brief 註冊路由 (類似 FastAPI @app.get, @app.post)
 * @param app 應用實例
 * @param method HTTP 方法
 * @param path_pattern 路徑範本 (支援如 "/items/{item_id}" 或 "/users/{id}")
 * @param handler 處理函式指標
 */
int fastapi_route(fastapi_app_t *app, http_method_t method, const char *path_pattern, route_handler_t handler);

/* 快捷路由巨集 */
#define fastapi_get(app, path, handler)    fastapi_route(app, HTTP_METHOD_GET, path, handler)
#define fastapi_post(app, path, handler)   fastapi_route(app, HTTP_METHOD_POST, path, handler)
#define fastapi_put(app, path, handler)    fastapi_route(app, HTTP_METHOD_PUT, path, handler)
#define fastapi_delete(app, path, handler) fastapi_route(app, HTTP_METHOD_DELETE, path, handler)

/**
 * @brief 啟動非同步 epoll 伺服器 (非阻塞 Reactor 迴圈)
 * @param app 應用實例
 * @param host 綁定 IP (如 "0.0.0.0" 或 "127.0.0.1")
 * @param port 監聽 Port (如 8080)
 */
int fastapi_run(fastapi_app_t *app, const char *host, int port);

/**
 * @brief 請求停止伺服器 (安全退出 epoll 迴圈)
 */
void fastapi_stop(fastapi_app_t *app);

/* ---------------- 輔助函式：參數提取 ---------------- */

/**
 * @brief 取得指定名稱的 Path Parameter (路徑參數)
 * 例如範本為 "/items/{id}"，請求為 "/items/100"，fastapi_get_param(req, "id") 返回 "100"
 */
const char *fastapi_get_param(const http_request_t *req, const char *key);

/**
 * @brief 取得指定名稱的 Query Parameter (查詢參數)
 */
const char *fastapi_get_query(const http_request_t *req, const char *key);

/**
 * @brief 取得指定名稱的 Header
 */
const char *fastapi_get_header(const http_request_t *req, const char *key);

/* ---------------- 輔助函式：回應建構 ---------------- */

/**
 * @brief 初始化預設 HTTP 回應結構
 */
void fastapi_response_init(http_response_t *res);

/**
 * @brief 設定自訂 Header
 */
void fastapi_response_set_header(http_response_t *res, const char *key, const char *value);

/**
 * @brief 快速建構 JSON 格式回應 (自動設定 Content-Type: application/json)
 * @param res 回應結構
 * @param status_code HTTP 狀態碼 (如 200, 201, 404)
 * @param json_str JSON 字串內容 (會內部複製，呼叫者保留所有權)
 */
void fastapi_json_response(http_response_t *res, int status_code, const char *json_str);

/**
 * @brief 快速建構純文字格式回應
 */
void fastapi_text_response(http_response_t *res, int status_code, const char *text);

/**
 * @brief 快速建構錯誤回應 (如 400 Bad Request, 404 Not Found)
 */
void fastapi_error_response(http_response_t *res, int status_code, const char *detail);

#ifdef __cplusplus
}
#endif

#endif /* FASTAPI_H */
