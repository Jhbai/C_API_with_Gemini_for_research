# C-FastAPI: 仿照 Python FastAPI 的純 C 語言非同步 Web 框架

這是一個完全以 **C11** 編寫的高性能、零外部相依性（Zero-Dependency）的非同步 HTTP 伺服器框架。它在架構設計上嚴格參考了 Python FastAPI 的簡潔路由與資料提取體驗，同時在底層利用 Linux `epoll` 事件驅動多路復用實現了極致的高並發與非阻塞（Non-blocking）吞吐量。

---

## 🎯 核心需求實作詳解

### 1. HTTP 的流式解析 (HTTP Streaming Parser)
- 檔案：`src/http_parser.c` & `src/http_parser.h`
- 核心功能：
  - 流式狀態機：在非阻塞 TCP 連線中，資料常以片段形式送達。解析器自動檢查 `\r\n\r\n` 標頭結尾與 `Content-Length` 主體長度。若資料未收齊則返回 `HTTP_PARSE_INCOMPLETE`，不會阻塞服務執行，等待下一個 `EPOLLIN` 事件。
  - 支援所有標準 HTTP 方法 (`GET`, `POST`, `PUT`, `DELETE`, `PATCH` 等)。
  - 自動提取並 URL-decode 查詢參數 (Query String，例如 `?q=hello%20world&page=2`)。
  - 解析 Headers 鍵值對，並支援 HTTP Pipelining (藉由消耗位元組偏移計算)。

### 2. Request 接收與記憶體管理 (Socket Programming & Memory Management)
- 檔案：`src/server.c` & `src/server.h`
- 核心功能：
  - 非阻塞通訊端：所有客戶端 socket 透過 `fcntl(fd, F_SETFL, O_NONBLOCK)` 設為非阻塞。
  - `client_conn_t` 生命週期管理：為每個 TCP 連線維護獨立的動態記憶體區域，包含：
    - `read_buf`：動態倍增擴展的接收緩衝區（設有 16MB 安全邊界，防範惡意記憶體耗盡攻擊）。
    - `write_buf`：用於非阻塞寫入與網路背壓（Backpressure）的發送佇列。
  - 零記憶體外洩（Zero Leaks）：連線關閉時透過 `conn_close()` 徹底釋放所有緩衝區與請求資料；支援 HTTP/1.1 Keep-Alive 複用。

### 3. Return 最終結果與 JSON 回應建構
- 檔案：`src/response.c` & `src/response.h`
- 核心功能：
  - FastAPI 風格的語法糖輔助函式：
    - `fastapi_json_response(res, 200, json_str)`：自動附加 `Content-Type: application/json`。
    - `fastapi_text_response(res, 200, text)`。
    - `fastapi_error_response(res, status, detail)`：標準化錯誤格式。
  - 封包序列化：`response_serialize()` 自動組合 Status Line、`Content-Length`、`Server`、`Connection` 與自訂 Header。

### 4. 服務非同步化 (Async / Non-blocking Event Loop)
- 核心引擎：Linux `epoll` (`epoll_create1`, `epoll_ctl`, `epoll_wait`)
- 運作模式：
  - 單執行緒事件反應器模式（Single-threaded Reactor Pattern），不為每個連線建立耗資源的執行緒。
  - `recv()` 與 `send()` 遇到 `EAGAIN` 或 `EWOULDBLOCK` 時立即讓出 CPU，登記 epoll 事件等待核心通知，絕不因單一慢速客戶端（Slowloris）而阻塞伺服器。

### 5. 伺服器核心與 API 程式碼嚴格分開
- **Server 底層引擎** (`server.c`, `server.h`, `http_parser.c`, `router.c`, `response.c`)：專注於 Socket、epoll、字元串流解析與記憶體配置。
- **業務 API 路由層** (`api_routes.c`, `api_routes.h`)：完全不需要引用 `<sys/socket.h>` 或 `<sys/epoll.h>`，只透過簡潔的高階介面開發端點：
  ```c
  /* 範例：api_routes.c 中的 FastAPI 風格處理器 */
  static void handle_get_item(const http_request_t *req, http_response_t *res) {
      const char *id = fastapi_get_param(req, "id");
      const char *q = fastapi_get_query(req, "q");
      
      char json[512];
      snprintf(json, sizeof(json), "{\"item_id\":\"%s\",\"q\":\"%s\"}", id, q ? q : "null");
      fastapi_json_response(res, 200, json);
  }
  ```

---

## 📂 專案檔案結構

```
c_fastapi/
├── include/
│   └── fastapi.h           # 框架對外公開 API 介面定義
├── src/
│   ├── http_parser.h       # HTTP/1.1 解析器標頭檔
│   ├── http_parser.c       # HTTP/1.1 流式解析實作
│   ├── router.h            # 動態路由表與 Path Params 比對器標頭檔
│   ├── router.c            # 路由解析與範本比對實作
│   ├── response.h          # 回應建構與序列化標頭檔
│   ├── response.c          # 回應建構與序列化實作
│   ├── server.h            # epoll Reactor 引擎內部標頭檔
│   ├── server.c            # epoll 事件迴圈、Socket 非阻塞 I/O 與記憶體管理
│   ├── api_routes.h        # 應用層 API 路由宣告 (與 Server 完全隔離)
│   ├── api_routes.c        # 應用層 API 路由實作 (FastAPI 業務端點)
│   └── main.c              # 伺服器啟動入口
├── Makefile                # 編譯與建構腳本 (build, run, test, debug, clean)
├── test_server.sh          # 自動化測試與並發驗證腳本
└── README.md               # 專案詳細架構說明
```

---

## 🛠️ 編譯與執行方式

### 1. 編譯 Release 版本
```bash
make build
# 產生執行檔於 bin/fastapi_server
```

### 2. 啟動伺服器 (預設 8080 Port)
```bash
make run
# 或手動執行並指定 Port:
./bin/fastapi_server 8080 0.0.0.0
```

### 3. 執行自動化測試
```bash
make test
```

### 4. 編譯 Debug 檢查版本 (包含 AddressSanitizer 記憶體安全檢查)
```bash
make debug
./bin/fastapi_server_debug
```

### 5. 清理編譯產物
```bash
make clean
```
