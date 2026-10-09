/**
 * @file http_parser.c
 * @brief 純 C 語言流式 HTTP/1.1 解析器實作
 * 
 * 負責將 socket 收到的原始位元組串流解析為 http_request_t，
 * 支援非阻塞 TCP 串流的 Partial Reads 判定 (HTTP_PARSE_INCOMPLETE)，
 * 以及安全的記憶體管理。
 */

#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* 字串安全去除前後空白 */
static char *trim_whitespace(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/* 簡單 URL 十六進位解碼 */
static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

char *url_decode(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;

    size_t i = 0, j = 0;
    while (i < len) {
        if (src[i] == '%' && i + 2 < len) {
            int h1 = hex_val(src[i + 1]);
            int h2 = hex_val(src[i + 2]);
            if (h1 >= 0 && h2 >= 0) {
                out[j++] = (char)((h1 << 4) | h2);
                i += 3;
                continue;
            }
        }
        if (src[i] == '+') {
            out[j++] = ' ';
        } else {
            out[j++] = src[i];
        }
        i++;
    }
    out[j] = '\0';
    return out;
}

/* 解析 Query String: 例如 "q=fastapi&limit=10&page=2" */
void parse_query_string(http_request_t *req, const char *qs) {
    if (!req || !qs || *qs == '\0') return;

    char *qs_copy = strdup(qs);
    if (!qs_copy) return;

    char *token = strtok(qs_copy, "&");
    while (token && req->query_count < FASTAPI_MAX_QUERY_PARAMS) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            req->query_params[req->query_count].key = url_decode(token);
            req->query_params[req->query_count].value = url_decode(eq + 1);
        } else {
            req->query_params[req->query_count].key = url_decode(token);
            req->query_params[req->query_count].value = strdup("");
        }
        req->query_count++;
        token = strtok(NULL, "&");
    }
    free(qs_copy);
}

/* 解析 HTTP Method */
static http_method_t parse_method(const char *str, size_t len) {
    if (len == 3 && strncmp(str, "GET", 3) == 0) return HTTP_METHOD_GET;
    if (len == 4 && strncmp(str, "POST", 4) == 0) return HTTP_METHOD_POST;
    if (len == 3 && strncmp(str, "PUT", 3) == 0) return HTTP_METHOD_PUT;
    if (len == 6 && strncmp(str, "DELETE", 6) == 0) return HTTP_METHOD_DELETE;
    if (len == 5 && strncmp(str, "PATCH", 5) == 0) return HTTP_METHOD_PATCH;
    if (len == 7 && strncmp(str, "OPTIONS", 7) == 0) return HTTP_METHOD_OPTIONS;
    if (len == 4 && strncmp(str, "HEAD", 4) == 0) return HTTP_METHOD_HEAD;
    return HTTP_METHOD_UNKNOWN;
}

/* 主要流式解析函式 */
http_parse_status_t http_parse_request(const char *raw_buffer, size_t buffer_len, 
                                       http_request_t *req, size_t *consumed_bytes) {
    if (!raw_buffer || buffer_len == 0 || !req || !consumed_bytes) {
        return HTTP_PARSE_ERROR;
    }

    /* 步驟 1: 尋找 HTTP Header 結尾標記 "\r\n\r\n" */
    const char *header_end = NULL;
    size_t header_end_offset = 0;
    for (size_t i = 0; i + 3 < buffer_len; i++) {
        if (raw_buffer[i] == '\r' && raw_buffer[i+1] == '\n' &&
            raw_buffer[i+2] == '\r' && raw_buffer[i+3] == '\n') {
            header_end = raw_buffer + i;
            header_end_offset = i + 4;
            break;
        }
    }

    /* 若尚未接收到完整的 Headers，通知上層繼續監聽 socket (Non-blocking) */
    if (!header_end) {
        return HTTP_PARSE_INCOMPLETE;
    }

    /* 初始化 Request 結構 */
    memset(req, 0, sizeof(http_request_t));
    req->keep_alive = true; /* HTTP/1.1 預設 Keep-Alive */

    /* 複製 Headers 區塊以利破壞性標記 (strtok/strchr) */
    size_t headers_len = header_end - raw_buffer;
    char *headers_copy = (char *)malloc(headers_len + 1);
    if (!headers_copy) return HTTP_PARSE_ERROR;
    memcpy(headers_copy, raw_buffer, headers_len);
    headers_copy[headers_len] = '\0';

    /* 步驟 2: 解析 Request-Line (例如: "GET /items/42?q=fastapi HTTP/1.1") */
    char *saveptr = NULL;
    char *line = strtok_r(headers_copy, "\r\n", &saveptr);
    if (!line) {
        free(headers_copy);
        return HTTP_PARSE_ERROR;
    }

    char *method_token = strtok(line, " ");
    char *uri_token = strtok(NULL, " ");
    char *version_token = strtok(NULL, " ");

    if (!method_token || !uri_token || !version_token) {
        free(headers_copy);
        return HTTP_PARSE_ERROR;
    }

    req->method = parse_method(method_token, strlen(method_token));
    req->method_str = (req->method == HTTP_METHOD_GET) ? "GET" :
                      (req->method == HTTP_METHOD_POST) ? "POST" :
                      (req->method == HTTP_METHOD_PUT) ? "PUT" :
                      (req->method == HTTP_METHOD_DELETE) ? "DELETE" :
                      (req->method == HTTP_METHOD_PATCH) ? "PATCH" :
                      (req->method == HTTP_METHOD_OPTIONS) ? "OPTIONS" :
                      (req->method == HTTP_METHOD_HEAD) ? "HEAD" : "UNKNOWN";

    req->raw_path = strdup(uri_token);

    /* 分離 Path 與 Query String */
    char *qmark = strchr(uri_token, '?');
    if (qmark) {
        *qmark = '\0';
        req->path = strdup(uri_token);
        req->query_string = strdup(qmark + 1);
        parse_query_string(req, req->query_string);
    } else {
        req->path = strdup(uri_token);
        req->query_string = NULL;
    }

    /* 步驟 3: 解析 Headers */
    size_t content_length = 0;
    while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
        if (req->header_count >= FASTAPI_MAX_HEADERS) break;
        
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *key = trim_whitespace(line);
            char *val = trim_whitespace(colon + 1);

            req->headers[req->header_count].key = strdup(key);
            req->headers[req->header_count].value = strdup(val);
            req->header_count++;

            /* 檢查 Content-Length */
            if (strcasecmp(key, "Content-Length") == 0) {
                content_length = (size_t)strtoul(val, NULL, 10);
            }
            /* 檢查 Connection 模式 */
            if (strcasecmp(key, "Connection") == 0) {
                if (strcasecmp(val, "close") == 0) {
                    req->keep_alive = false;
                } else if (strcasecmp(val, "keep-alive") == 0) {
                    req->keep_alive = true;
                }
            }
        }
    }
    free(headers_copy);

    /* 步驟 4: 檢查 Body 完整性 */
    size_t total_required_bytes = header_end_offset + content_length;
    if (buffer_len < total_required_bytes) {
        /* Headers 齊全，但 Body 尚未完全到達，釋放已配置欄位並等待下一個 read 事件 */
        http_request_free(req);
        return HTTP_PARSE_INCOMPLETE;
    }

    /* 讀取 Body */
    if (content_length > 0) {
        req->body = (char *)malloc(content_length + 1);
        if (!req->body) {
            http_request_free(req);
            return HTTP_PARSE_ERROR;
        }
        memcpy(req->body, raw_buffer + header_end_offset, content_length);
        req->body[content_length] = '\0';
        req->body_len = content_length;
    } else {
        req->body = NULL;
        req->body_len = 0;
    }

    *consumed_bytes = total_required_bytes;
    return HTTP_PARSE_OK;
}

/* 釋放 Request 所有動態配置記憶體 */
void http_request_free(http_request_t *req) {
    if (!req) return;

    if (req->raw_path) {
        free(req->raw_path);
        req->raw_path = NULL;
    }
    if (req->path) {
        free(req->path);
        req->path = NULL;
    }
    if (req->query_string) {
        free(req->query_string);
        req->query_string = NULL;
    }
    if (req->body) {
        free(req->body);
        req->body = NULL;
    }

    for (size_t i = 0; i < req->header_count; i++) {
        if (req->headers[i].key) free(req->headers[i].key);
        if (req->headers[i].value) free(req->headers[i].value);
    }
    req->header_count = 0;

    for (size_t i = 0; i < req->param_count; i++) {
        if (req->params[i].key) free(req->params[i].key);
        if (req->params[i].value) free(req->params[i].value);
    }
    req->param_count = 0;

    for (size_t i = 0; i < req->query_count; i++) {
        if (req->query_params[i].key) free(req->query_params[i].key);
        if (req->query_params[i].value) free(req->query_params[i].value);
    }
    req->query_count = 0;
}
