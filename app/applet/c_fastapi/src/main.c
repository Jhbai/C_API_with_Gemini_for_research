/**
 * @file main.c
 * @brief 伺服器啟動入口 (Application Entrypoint)
 * 
 * 整合 C-FastAPI 核心引擎與業務 API 路由，解析命令列參數並啟動伺服器。
 */

#include "fastapi.h"
#include "api_routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    const char *host = "0.0.0.0";
    int port = FASTAPI_DEFAULT_PORT;

    /* 支援自訂命令列參數: ./server [port] [host] */
    if (argc >= 2) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number. Defaulting to %d\n", FASTAPI_DEFAULT_PORT);
            port = FASTAPI_DEFAULT_PORT;
        }
    }
    if (argc >= 3) {
        host = argv[2];
    }

    /* 1. 建立 C-FastAPI 實例 */
    fastapi_app_t *app = fastapi_create();
    if (!app) {
        fprintf(stderr, "Failed to initialize C-FastAPI application instance.\n");
        return EXIT_FAILURE;
    }

    /* 2. 註冊解耦的業務 API 路由 (定義於 api_routes.c) */
    register_api_routes(app);

    printf("🔧 Registered API Routes:\n");
    printf("   - GET  /\n");
    printf("   - GET  /health\n");
    printf("   - GET  /items/{id}\n");
    printf("   - POST /items\n");
    printf("   - GET  /users/{user_id}/status\n");
    printf("   - GET  /async-demo\n");

    /* 3. 啟動 Linux epoll 非同步事件驅動迴圈 */
    int ret = fastapi_run(app, host, port);

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
