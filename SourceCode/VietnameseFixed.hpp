#pragma once
#include <iostream>
#include <string>
#include <unordered_map>
#include <cctype>

/*
 * toLower
 * Chức năng : Chuẩn hóa một chuỗi UTF-8 về dạng "chữ thường, không dấu".
 *             Hàm làm đồng thời hai việc:
 *               1) Đổi chữ hoa thành chữ thường.
 *               2) Bỏ dấu tiếng Việt (à, á, ả, ã, ạ, ă, â, ê, ô, ơ, ư...
 *                  thành a, e, o, u...; đ và Đ thành d).
 *             Thường dùng để so sánh / tìm kiếm tên, địa chỉ mà không phụ
 *             thuộc vào việc người dùng gõ hoa hay thường, có dấu hay không.
 * Tham số   : input - chuỗi gốc, mã hóa UTF-8 (có thể chứa tiếng Việt có dấu).
 * Trả về    : Chuỗi mới chỉ gồm ký tự ASCII chữ thường (không sửa chuỗi gốc).
 * Ví dụ     : "Nguyễn Văn Đức" -> "nguyen van duc"
 * Lưu ý     : Ký tự ASCII khác (số, khoảng trắng, dấu câu) được giữ nguyên.
 *             Ký tự không phải ASCII và không có trong bảng ánh xạ tiếng Việt
 *             (ví dụ chữ Hán, emoji, byte UTF-8 không hợp lệ) sẽ bị BỎ đi.
 *             Tên hàm là "toLower" nhưng hàm còn bỏ dấu, nên không chỉ đơn
 *             thuần là đổi sang chữ thường.
 */
std::string toLower(const std::string& input);