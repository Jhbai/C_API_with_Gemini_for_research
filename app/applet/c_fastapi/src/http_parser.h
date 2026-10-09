/**
 * @file http_parser.h
 * @brief 純 C 語言流式 HTTP/1.1 協議解析器 (Zero-Dependency)
 */

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "../include/fastapi.h"

/* HTTP 解析階段狀態 */
typedef enum {
    HTTP_PARSE_INCOMPLETE = 0, /* 資料尚未接收完全，需繼續從 Socket 讀取 */
    HTTP_PARSE_OK = 1,         /* 解析完整，包含 Request-Line, Headers 與 Body */
    HTTP_PARSE_ERROR = -1      /* 語法錯誤或惡意請求 (400 Bad Request) */
} http_parse_status_t;

/**
 * @brief 解析 HTTP 請求緩衝區
 * @param raw_buffer 接收到的原始位元組資料
 * @param buffer_len 資料長度
 * @param req 填入解析後的結構
 * @param consumed_bytes 成功解析所消耗的總位元組數 (用於處理 HTTP Pipeline)
 * @return http_parse_status_t 狀態碼
 */
http_parse_status_t http_parse_request(const char *raw_buffer, size_t buffer_len, 
                                       http_request_t *req, size_t *consumed_bytes);

/**
 * @brief 釋放 http_request_t 內部動態配置的所有欄位記憶體
 */
void http_request_free(http_request_t *req);

/**
 * @brief URL 解碼函式 (處理 %20, %2B 等編碼字元)
 */
char *url_decode(const char *src);

/**
 * @brief 解析 Query String 並填入 req->query_params
 */
void parse_query_string(http_request_t *req, const char *qs);

#endif /* HTTP_PARSER_H */
