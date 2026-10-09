/**
 * @file api_routes.h
 * @brief 應用程式層 API 路由定義 (與底層 Server Socket / epoll 程式碼完全解耦)
 * 
 * 依照 Python FastAPI 的設計模式，API 開發者只需專注於請求處理邏輯，
 * 不需接觸底層 Socket、Buffer 管理或 epoll 呼叫。
 */

#ifndef API_ROUTES_H
#define API_ROUTES_H

#include "../include/fastapi.h"

/**
 * @brief 註冊所有的業務 API 路由至 C-FastAPI 實例
 * @param app C-FastAPI 應用實例
 */
void register_api_routes(fastapi_app_t *app);

#endif /* API_ROUTES_H */
