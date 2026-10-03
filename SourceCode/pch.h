/*
 * pch.h - Precompiled Header (tệp tiêu đề biên dịch trước)
 * ---------------------------------------------------------------
 * File này KHÔNG chứa hàm nào. Nó gom các thư viện bên ngoài dùng chung
 * trong dự án để đưa vào biên dịch một lần, giúp rút ngắn thời gian build.
 */
#pragma once   // Đảm bảo file chỉ được include một lần trong mỗi đơn vị biên dịch.

// Khai báo phiên bản Windows mục tiêu là Windows 10 (0x0A00). Thư viện
// httplib dựa vào macro này để dùng đúng các API mạng của Windows.
// Macro phải được định nghĩa TRƯỚC khi include httplib.h.
#define _WIN32_WINNT 0x0A00

// Thư viện httplib: tạo HTTP server / client (xử lý request, response).
#include "httplib.h"

// Thư viện JSON (json.hpp, thường là nlohmann/json): đọc, ghi và xử lý dữ liệu JSON.
#include "json.hpp"