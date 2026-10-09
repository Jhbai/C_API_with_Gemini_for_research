# C-FastAPI: Asynchronous High-Performance C Web Framework

A modular, zero-dependency asynchronous HTTP server framework written in C11 inspired by Python's FastAPI. The architecture strictly decouples the asynchronous `epoll` reactor engine and HTTP protocol parser from business API routes, complete with dynamic path parameter extraction, safe per-connection buffer lifecycle management, and a comprehensive Makefile.

## User Review & Critical Decisions

> [!IMPORTANT]
> The following architectural decisions were confirmed via interactive clarification:
> - **I/O Engine**: Linux `epoll` reactor with non-blocking sockets (`O_NONBLOCK`), enabling high-concurrency event-driven processing without thread-per-connection overhead.
> - **Dependencies**: Zero external C dependencies (Standard C11/POSIX libc) for maximum portability, security, and lean binary footprint.
> - **Routing Style**: FastAPI-inspired route registration (`fastapi_get`, `fastapi_post`) supporting path parameters (e.g. `/items/{id}`), query parameters, and helper JSON response constructors.

---

## 1. Overview & Core Concept

- **What It Does**: Provides a lightweight yet production-grade C web framework modeling FastAPI's ergonomics while achieving C-native non-blocking I/O speed. Features streaming HTTP/1.1 parsing, dynamic URI template matching (`{param}`), dynamic JSON response construction, connection state machines, and graceful signal-driven shutdown.
- **Architectural Separation**:
  - **Server Engine Layer (`server/`)**: Socket creation, `epoll` event loop, non-blocking socket buffering, streaming HTTP parser, and router table.
  - **Application API Layer (`api/`)**: Pure route handler definitions and business logic, completely unaware of low-level socket handles or `epoll` calls.
- **Target Audience**: Systems programmers, backend engineers, and embedded/cloud developers needing C-speed microservices with FastAPI-level code organization.

---

## 2. Technical Architecture & System Design

```
+─────────────────────────────────────────────────────────────────────────────+
│                           Linux epoll Event Loop                            │
│  [ epoll_wait(epfd, events, MAX_EVENTS, -1) ]                               │
+──────────────────────┬──────────────────────────────────────────────────────+
                       │
       ┌───────────────┴─────────────────┐
       ▼                                 ▼
[ Accept Socket Event ]       [ Client Socket I/O (EPOLLIN / EPOLLOUT) ]
Non-blocking accept4()         Non-blocking read/write with EAGAIN handling
Initialize conn_t lifecycle    
                                         │
                                         ▼
                               +──────────────────+
                               │ Streaming Parser │
                               │ Method, URI,     │
                               │ Headers, Body    │
                               +─────────┬────────+
                                         │
                                         ▼
                               +──────────────────+
                               │ Router & Matcher │
                               │ Pattern Match    │
                               │ /items/{id}      │
                               +─────────┬────────+
                                         │
                                         ▼
                               +──────────────────+
                               │ API Route Handler│ <── Isolated api_routes.c
                               │ Build JSON Resp  │
                               +─────────┬────────+
                                         │
                                         ▼
                               +──────────────────+
                               │ Async Send Queue │
                               │ Non-blocking     │
                               │ Flush & Cleanup  │
                               +──────────────────+
```

### Module Responsibilities

1. **`c_fastapi/include/fastapi.h`**:
   - Universal framework header defining `fastapi_app_t`, `http_request_t`, `http_response_t`, and public routing macros.
2. **`c_fastapi/src/http_parser.c` & `http_parser.h`**:
   - Pure C streaming HTTP/1.1 parser.
   - Parses request line, headers (key-value pairs), query strings, and body payload safely with bounds checking.
3. **`c_fastapi/src/router.c` & `router.h`**:
   - Dynamic router supporting templated path parameters (e.g., `/users/{user_id}/items/{item_id}`).
   - Fast lookup and parameter extraction passed directly to handler context.
4. **`c_fastapi/src/server.c` & `server.h`**:
   - Asynchronous Linux `epoll` reactor.
   - Non-blocking `recv()` and `send()` with `EAGAIN`/`EWOULDBLOCK` state machine.
   - Per-connection memory management: dynamically expanding read/write buffers, keep-alive timers, and immediate leak-free teardown upon disconnect.
5. **`c_fastapi/src/api_routes.c` & `api_routes.h`**:
   - Application-level business endpoints:
     - `GET /` (System status & metadata)
     - `GET /items/{id}` (Path param extraction + query parameter parsing)
     - `POST /items` (Body parsing, JSON deserialization, 201 Created)
     - `GET /users/{user_id}` (Nested route params)
     - `GET /health` (Lightweight heartbeat)
6. **`c_fastapi/src/main.c`**:
   - Application entry point: initializes app, registers routes, sets up `SIGINT`/`SIGTERM` handlers for graceful epoll loop termination.
7. **`c_fastapi/Makefile`**:
   - Complete build script with targets: `all`, `build`, `run`, `test`, `clean`, and compiler flags `-Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE`.
8. **Interactive Web Dashboard (`src/App.tsx`)**:
   - Modern developer interface featuring an interactive Architecture Visualizer, Live C Code Explorer with copy/download options, Protocol State Machine Inspector, and a built-in API test runner.

---

## 3. Key Product Decisions & Memory Strategy

- **Zero-Copy & Heap Safety**:
  - The HTTP parser references sliced string views (`string_view` style pointer + length) into the connection buffer where possible to avoid redundant heap allocations.
  - Per-connection structures (`conn_t`) maintain self-contained read/write arenas that are allocated on connection accept and wiped cleanly on close, preventing memory fragmentation and leaks.
- **Handling Asynchronous Edge Cases**:
  - **Partial Reads**: Sockets maintain incoming byte buffers across multiple `EPOLLIN` notifications until double CRLF (`\r\n\r\n`) and full `Content-Length` are received.
  - **Partial Writes / Backpressure**: If `send()` returns `EAGAIN` or writes fewer bytes than the response size, remaining bytes are queued in the client's write buffer and `EPOLLOUT` is monitored until complete.
- **FastAPI Ergonomics in C**:
  - Helper functions for JSON responses (`fastapi_json_response(res, status, json_str)`) and parameter extraction (`fastapi_param(req, "id")`, `fastapi_query(req, "q")`).

---

## 4. Verification & Testing Plan

1. **Compilation Verification**:
   - Compile the C framework with `make -C c_fastapi` using GCC on the Linux environment to verify zero warnings and clean build.
2. **Unit & Integration Testing**:
   - Execute an automated test suite sending concurrent HTTP requests, pipeline requests, path parameter tests, and large payloads.
3. **Frontend Applet Verification**:
   - Run `compile_applet` to confirm the React interactive dashboard compiles with zero errors.
