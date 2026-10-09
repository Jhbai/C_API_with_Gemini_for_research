/**
 * @file response.c
 * @brief HTTP 回應建構、序列化與記憶體清理實作
 */

#include "response.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *http_status_to_string(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 422: return "Unprocessable Entity";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

void fastapi_response_init(http_response_t *res) {
    if (!res) return;
    memset(res, 0, sizeof(http_response_t));
    res->status_code = 200;
    res->status_text = "OK";
    res->header_count = 0;
    res->body = NULL;
    res->body_len = 0;
    res->body_dynamically_allocated = false;
}

void fastapi_response_set_header(http_response_t *res, const char *key, const char *value) {
    if (!res || !key || !value) return;

    /* 檢查是否已存在該 Header，存在則覆蓋 */
    for (size_t i = 0; i < res->header_count; i++) {
        if (strcasecmp(res->headers[i].key, key) == 0) {
            free(res->headers[i].value);
            res->headers[i].value = strdup(value);
            return;
        }
    }

    if (res->header_count < FASTAPI_MAX_HEADERS) {
        res->headers[res->header_count].key = strdup(key);
        res->headers[res->header_count].value = strdup(value);
        res->header_count++;
    }
}

void fastapi_json_response(http_response_t *res, int status_code, const char *json_str) {
    if (!res) return;

    res->status_code = status_code;
    res->status_text = http_status_to_string(status_code);
    fastapi_response_set_header(res, "Content-Type", "application/json; charset=utf-8");

    if (res->body_dynamically_allocated && res->body) {
        free(res->body);
    }

    if (json_str) {
        res->body = strdup(json_str);
        res->body_len = strlen(json_str);
        res->body_dynamically_allocated = true;
    } else {
        res->body = strdup("{}");
        res->body_len = 2;
        res->body_dynamically_allocated = true;
    }
}

void fastapi_text_response(http_response_t *res, int status_code, const char *text) {
    if (!res) return;

    res->status_code = status_code;
    res->status_text = http_status_to_string(status_code);
    fastapi_response_set_header(res, "Content-Type", "text/plain; charset=utf-8");

    if (res->body_dynamically_allocated && res->body) {
        free(res->body);
    }

    if (text) {
        res->body = strdup(text);
        res->body_len = strlen(text);
        res->body_dynamically_allocated = true;
    } else {
        res->body = NULL;
        res->body_len = 0;
        res->body_dynamically_allocated = false;
    }
}

void fastapi_error_response(http_response_t *res, int status_code, const char *detail) {
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"error\":true,\"status\":%d,\"detail\":\"%s\"}", 
             status_code, detail ? detail : http_status_to_string(status_code));
    fastapi_json_response(res, status_code, buf);
}

void response_cleanup(http_response_t *res) {
    if (!res) return;

    for (size_t i = 0; i < res->header_count; i++) {
        if (res->headers[i].key) free(res->headers[i].key);
        if (res->headers[i].value) free(res->headers[i].value);
    }
    res->header_count = 0;

    if (res->body_dynamically_allocated && res->body) {
        free(res->body);
        res->body = NULL;
    }
    res->body_len = 0;
}

/* 將回應結構序列化為 HTTP 封包字串 */
char *response_serialize(const http_response_t *res, bool keep_alive, size_t *out_len) {
    if (!res || !out_len) return NULL;

    /* 計算緩衝區大小 */
    size_t est_header_len = 512;
    for (size_t i = 0; i < res->header_count; i++) {
        est_header_len += strlen(res->headers[i].key) + strlen(res->headers[i].value) + 4;
    }
    size_t total_alloc = est_header_len + res->body_len + 64;

    char *buf = (char *)malloc(total_alloc);
    if (!buf) return NULL;

    /* 1. 寫入 Status Line (例如: "HTTP/1.1 200 OK\r\n") */
    int written = snprintf(buf, total_alloc, "HTTP/1.1 %d %s\r\n", 
                           res->status_code, 
                           res->status_text ? res->status_text : http_status_to_string(res->status_code));
    size_t offset = written;

    /* 2. 寫入基本 Headers: Server, Content-Length, Connection */
    offset += snprintf(buf + offset, total_alloc - offset, 
                       "Server: C-FastAPI/1.0\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: %s\r\n", 
                       res->body_len, 
                       keep_alive ? "keep-alive" : "close");

    /* 3. 寫入使用者自訂 Headers */
    for (size_t i = 0; i < res->header_count; i++) {
        /* 跳過手動設定的 Content-Length 與 Connection 避免重複 */
        if (strcasecmp(res->headers[i].key, "Content-Length") == 0 ||
            strcasecmp(res->headers[i].key, "Connection") == 0) {
            continue;
        }
        offset += snprintf(buf + offset, total_alloc - offset, "%s: %s\r\n", 
                           res->headers[i].key, res->headers[i].value);
    }

    /* 4. 寫入空行分隔標記 */
    offset += snprintf(buf + offset, total_alloc - offset, "\r\n");

    /* 5. 寫入 Body */
    if (res->body && res->body_len > 0) {
        memcpy(buf + offset, res->body, res->body_len);
        offset += res->body_len;
    }

    *out_len = offset;
    return buf;
}

/* 參數提取工具實作 */
const char *fastapi_get_param(const http_request_t *req, const char *key) {
    if (!req || !key) return NULL;
    for (size_t i = 0; i < req->param_count; i++) {
        if (strcmp(req->params[i].key, key) == 0) {
            return req->params[i].value;
        }
    }
    return NULL;
}

const char *fastapi_get_query(const http_request_t *req, const char *key) {
    if (!req || !key) return NULL;
    for (size_t i = 0; i < req->query_count; i++) {
        if (strcmp(req->query_params[i].key, key) == 0) {
            return req->query_params[i].value;
        }
    }
    return NULL;
}

const char *fastapi_get_header(const http_request_t *req, const char *key) {
    if (!req || !key) return NULL;
    for (size_t i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].key, key) == 0) {
            return req->headers[i].value;
        }
    }
    return NULL;
}
