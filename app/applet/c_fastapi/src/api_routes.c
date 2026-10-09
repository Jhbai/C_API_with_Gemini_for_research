/**
 * @file api_routes.c
 * @brief 應用程式層 API 路由實作 (純業務邏輯，完全獨立於 Server Socket / epoll)
 * 
 * 展示仿照 Python FastAPI 的精簡風格：
 * - Path Parameters: fastapi_get_param(req, "key")
 * - Query Parameters: fastapi_get_query(req, "key")
 * - Request Body: req->body
 * - JSON Response: fastapi_json_response(res, status, json)
 */

#include "api_routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* GET /: 根路徑歡迎與系統資訊 */
static void handle_root(const http_request_t *req, http_response_t *res) {
    (void)req;
    const char *json_body = 
        "{\n"
        "  \"message\": \"Welcome to C-FastAPI!\",\n"
        "  \"framework\": \"C-FastAPI\",\n"
        "  \"version\": \"" FASTAPI_VERSION "\",\n"
        "  \"engine\": \"Linux epoll non-blocking reactor\",\n"
        "  \"docs\": \"https://github.com/fastapi-c/c-fastapi\"\n"
        "}";
    fastapi_json_response(res, 200, json_body);
}

/* GET /health: 健康檢查 */
static void handle_health(const http_request_t *req, http_response_t *res) {
    (void)req;
    fastapi_json_response(res, 200, "{\"status\": \"healthy\", \"uptime\": \"ok\"}");
}

/* GET /items/{id}: 示範 Path Parameter 與 Query Parameter 結合 */
static void handle_get_item(const http_request_t *req, http_response_t *res) {
    /* 提取 Path Parameter */
    const char *item_id = fastapi_get_param(req, "id");
    if (!item_id) {
        fastapi_error_response(res, 400, "Missing path parameter 'id'");
        return;
    }

    /* 提取可選的 Query Parameter: ?q=keywords&skip=0 */
    const char *q = fastapi_get_query(req, "q");
    const char *skip = fastapi_get_query(req, "skip");

    char json_buf[1024];
    snprintf(json_buf, sizeof(json_buf),
             "{\n"
             "  \"item_id\": \"%s\",\n"
             "  \"name\": \"Item-%s\",\n"
             "  \"query_q\": %s%s%s,\n"
             "  \"query_skip\": \"%s\",\n"
             "  \"price\": 199.99,\n"
             "  \"in_stock\": true\n"
             "}",
             item_id, 
             item_id,
             q ? "\"" : "", q ? q : "null", q ? "\"" : "",
             skip ? skip : "0");

    fastapi_json_response(res, 200, json_buf);
}

/* POST /items: 接收並建立新資料項目 (支援 Body 解析) */
static void handle_create_item(const http_request_t *req, http_response_t *res) {
    if (!req->body || req->body_len == 0) {
        fastapi_error_response(res, 400, "Request body cannot be empty");
        return;
    }

    /* 回傳 201 Created 並回傳解析結果 */
    char json_buf[2048];
    snprintf(json_buf, sizeof(json_buf),
             "{\n"
             "  \"status\": \"created\",\n"
             "  \"code\": 201,\n"
             "  \"received_bytes\": %zu,\n"
             "  \"payload\": %s\n"
             "}",
             req->body_len,
             req->body);

    fastapi_json_response(res, 201, json_buf);
}

/* GET /users/{user_id}/status: 示範多層巢狀 Path 參數 */
static void handle_user_status(const http_request_t *req, http_response_t *res) {
    const char *user_id = fastapi_get_param(req, "user_id");

    char json_buf[512];
    snprintf(json_buf, sizeof(json_buf),
             "{\n"
             "  \"user_id\": \"%s\",\n"
             "  \"role\": \"developer\",\n"
             "  \"online\": true,\n"
             "  \"concurrent_capable\": true\n"
             "}",
             user_id ? user_id : "unknown");

    fastapi_json_response(res, 200, json_buf);
}

/* GET /async-demo: 示範高並發非阻塞服務能力 */
static void handle_async_demo(const http_request_t *req, http_response_t *res) {
    (void)req;
    const char *json_body = 
        "{\n"
        "  \"async\": true,\n"
        "  \"description\": \"Requests never block the reactor event loop!\",\n"
        "  \"architecture\": \"Linux epoll Edge/Level I/O multiplexing\",\n"
        "  \"concurrency_model\": \"Single-threaded asynchronous event loop\"\n"
        "}";
    fastapi_json_response(res, 200, json_body);
}

/* 註冊所有 API 路由 */
void register_api_routes(fastapi_app_t *app) {
    /* 仿照 Python FastAPI 的裝飾器路由設計 */
    fastapi_get(app, "/", handle_root);
    fastapi_get(app, "/health", handle_health);
    fastapi_get(app, "/items/{id}", handle_get_item);
    fastapi_post(app, "/items", handle_create_item);
    fastapi_get(app, "/users/{user_id}/status", handle_user_status);
    fastapi_get(app, "/async-demo", handle_async_demo);
}
