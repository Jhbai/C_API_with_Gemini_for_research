/**
 * @file response.h
 * @brief HTTP 回應物件序列化與輔助工具
 */

#ifndef RESPONSE_H
#define RESPONSE_H

#include "../include/fastapi.h"

/**
 * @brief 將 http_response_t 序列化為 HTTP/1.1 網路傳輸格式 (包含 Status Line, Headers, Body)
 * @param res 回應物件
 * @param keep_alive 是否維持連線
 * @param out_len 輸出位元組數長度
 * @return 動態配置的字串緩衝區 (呼叫端需負責 free)
 */
char *response_serialize(const http_response_t *res, bool keep_alive, size_t *out_len);

/**
 * @brief 釋放 http_response_t 內部分配的資源
 */
void response_cleanup(http_response_t *res);

/**
 * @brief 根據 HTTP 狀態碼取得對應說明文字 (例如 200 => "OK", 404 => "Not Found")
 */
const char *http_status_to_string(int status_code);

#endif /* RESPONSE_H */
