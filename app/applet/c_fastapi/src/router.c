/**
 * @file router.c
 * @brief 仿 FastAPI 路由解析與 Path Parameter 比對實作
 */

#include "router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 路徑分割為 segments (以 '/' 為分隔符號) */
static size_t split_path(const char *path, char segments[][64], size_t max_segments) {
    if (!path) return 0;

    size_t count = 0;
    const char *p = path;

    while (*p) {
        /* 跳過連續斜線 */
        while (*p == '/') p++;
        if (*p == '\0') break;

        const char *start = p;
        while (*p && *p != '/') p++;

        size_t seg_len = p - start;
        if (seg_len > 0 && count < max_segments) {
            size_t copy_len = seg_len < 63 ? seg_len : 63;
            strncpy(segments[count], start, copy_len);
            segments[count][copy_len] = '\0';
            count++;
        }
    }
    return count;
}

void router_init(router_t *router) {
    if (!router) return;
    router->route_count = 0;
    memset(router->routes, 0, sizeof(router->routes));
}

void router_cleanup(router_t *router) {
    if (!router) return;
    for (size_t i = 0; i < router->route_count; i++) {
        if (router->routes[i].pattern) {
            free(router->routes[i].pattern);
            router->routes[i].pattern = NULL;
        }
    }
    router->route_count = 0;
}

int router_add(router_t *router, http_method_t method, const char *pattern, route_handler_t handler) {
    if (!router || !pattern || !handler) return -1;
    if (router->route_count >= FASTAPI_MAX_ROUTES) return -1;

    route_entry_t *entry = &router->routes[router->route_count];
    entry->method = method;
    entry->pattern = strdup(pattern);
    entry->handler = handler;

    /* 解析路徑範本的分段 (例如: /items/{item_id}) */
    char raw_segs[FASTAPI_MAX_PATH_SEGMENTS][64];
    size_t seg_count = split_path(pattern, raw_segs, FASTAPI_MAX_PATH_SEGMENTS);
    entry->segment_count = seg_count;

    for (size_t i = 0; i < seg_count; i++) {
        size_t len = strlen(raw_segs[i]);
        if (len >= 2 && raw_segs[i][0] == '{' && raw_segs[i][len - 1] == '}') {
            /* 動態參數，例如 "{id}" => 提取 "id" */
            entry->segments[i].type = SEGMENT_PARAM;
            strncpy(entry->segments[i].name, raw_segs[i] + 1, len - 2);
            entry->segments[i].name[len - 2] = '\0';
        } else {
            /* 固定字面值 */
            entry->segments[i].type = SEGMENT_LITERAL;
            strncpy(entry->segments[i].name, raw_segs[i], 63);
            entry->segments[i].name[63] = '\0';
        }
    }

    router->route_count++;
    return 0;
}

/* 比對請求路徑與註冊路由 */
int router_dispatch(const router_t *router, http_request_t *req, http_response_t *res) {
    if (!router || !req || !res) return -1;

    char req_segs[FASTAPI_MAX_PATH_SEGMENTS][64];
    size_t req_seg_count = split_path(req->path, req_segs, FASTAPI_MAX_PATH_SEGMENTS);

    bool path_matched_other_method = false;

    for (size_t i = 0; i < router->route_count; i++) {
        const route_entry_t *entry = &router->routes[i];

        /* 分段長度必須一致 (例如 /items/{id} 有 2 個分段) */
        if (entry->segment_count != req_seg_count) {
            continue;
        }

        /* 暫存比對出來的 Path Parameters */
        http_key_value_t temp_params[FASTAPI_MAX_PARAMS];
        size_t temp_param_count = 0;
        bool match = true;

        for (size_t s = 0; s < entry->segment_count; s++) {
            if (entry->segments[s].type == SEGMENT_LITERAL) {
                if (strcmp(entry->segments[s].name, req_segs[s]) != 0) {
                    match = false;
                    break;
                }
            } else if (entry->segments[s].type == SEGMENT_PARAM) {
                if (temp_param_count < FASTAPI_MAX_PARAMS) {
                    temp_params[temp_param_count].key = (char *)entry->segments[s].name;
                    temp_params[temp_param_count].value = req_segs[s];
                    temp_param_count++;
                }
            }
        }

        if (match) {
            /* 路徑匹配，檢查 Method */
            if (entry->method != req->method) {
                path_matched_other_method = true;
                continue;
            }

            /* 匹配成功！將 Path Parameters 複製進 req 結構 */
            for (size_t p = 0; p < temp_param_count; p++) {
                if (req->param_count < FASTAPI_MAX_PARAMS) {
                    req->params[req->param_count].key = strdup(temp_params[p].key);
                    req->params[req->param_count].value = strdup(temp_params[p].value);
                    req->param_count++;
                }
            }

            /* 執行使用者在 api_routes.c 註冊的處理函式 */
            entry->handler(req, res);
            return 0; /* 成功處理 */
        }
    }

    if (path_matched_other_method) {
        return 405; /* Method Not Allowed */
    }
    return 404;     /* Not Found */
}
