/**
 * @file router.h
 * @brief 仿 FastAPI 路由表與 Path Parameters 動態比對器
 */

#ifndef ROUTER_H
#define ROUTER_H

#include "../include/fastapi.h"

#define FASTAPI_MAX_ROUTES 128
#define FASTAPI_MAX_PATH_SEGMENTS 16

/* 路由單一路徑分段型態 */
typedef enum {
    SEGMENT_LITERAL = 0, /* 固定字串 (例如 "items", "users") */
    SEGMENT_PARAM = 1    /* 動態參數 (例如 "{item_id}", "{id}") */
} segment_type_t;

/* 路徑分段定義 */
typedef struct {
    segment_type_t type;
    char name[64];       /* 參數名稱或字面值 */
} path_segment_t;

/* 單一註冊路由節點 */
typedef struct {
    http_method_t method;
    char *pattern;       /* 原始註冊路徑 (例如 "/items/{id}") */
    route_handler_t handler;
    path_segment_t segments[FASTAPI_MAX_PATH_SEGMENTS];
    size_t segment_count;
} route_entry_t;

/* 路由表結構 */
typedef struct {
    route_entry_t routes[FASTAPI_MAX_ROUTES];
    size_t route_count;
} router_t;

/**
 * @brief 初始化路由表
 */
void router_init(router_t *router);

/**
 * @brief 清理路由表記憶體
 */
void router_cleanup(router_t *router);

/**
 * @brief 註冊新路由
 */
int router_add(router_t *router, http_method_t method, const char *pattern, route_handler_t handler);

/**
 * @brief 尋找並派發符合的路由
 * 若匹配成功，會自動將 URL 中的動態參數填入 req->params，並呼叫 handler
 * @return 0 匹配成功, 404 找不到路由, 405 方法不允許
 */
int router_dispatch(const router_t *router, http_request_t *req, http_response_t *res);

#endif /* ROUTER_H */
