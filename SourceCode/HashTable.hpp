#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "Driver.hpp"
#include <unordered_map>
#include <vector>

class Operation;

/*
 * Lớp HashTable
 * ---------------------------------------------------------------
 * Mục đích: Quản lý danh sách tài xế (Driver) bằng bảng băm.
 * Cấu trúc dữ liệu bên trong là std::unordered_map với:
 *   - key   : ID của tài xế (std::string)
 *   - value : đối tượng Driver tương ứng
 * Nhờ bảng băm, các thao tác thêm / tìm / xóa / cập nhật theo ID
 * có độ phức tạp trung bình O(1).
 *
 * Lớp Operation được khai báo là friend nên có thể truy cập trực tiếp
 * vào thành viên private `data` của lớp này.
 */
class HashTable {
    friend class Operation;
private:
    // Bảng băm lưu toàn bộ tài xế: key = ID tài xế, value = thông tin Driver.
    std::unordered_map<std::string, Driver> data;
public:
    /*
     * checkID
     * Chức năng : Kiểm tra một ID tài xế đã tồn tại trong bảng băm hay chưa.
     * Tham số   : ID - mã định danh tài xế cần kiểm tra.
     * Trả về    : true nếu ID đã tồn tại, false nếu chưa.
     */
    bool checkID(std::string ID);

    /*
     * addDriver
     * Chức năng : Thêm một tài xế mới vào bảng băm.
     * Tham số   : ID     - mã định danh của tài xế mới.
     *             driver - đối tượng Driver cần lưu.
     * Lưu ý     : Nếu ID đã tồn tại thì in thông báo lỗi và KHÔNG thêm
     *             (không ghi đè dữ liệu cũ).
     */
    void addDriver(std::string ID, Driver driver); 

    /*
     * findDriver
     * Chức năng : Tìm tài xế theo ID.
     * Tham số   : ID - mã định danh tài xế cần tìm.
     * Trả về    : Con trỏ tới Driver nằm trong bảng băm nếu tìm thấy,
     *             nullptr nếu không có.
     * Lưu ý     : Trả về con trỏ tới dữ liệu gốc (không phải bản sao),
     *             nên thay đổi qua con trỏ sẽ ảnh hưởng trực tiếp dữ liệu
     *             trong bảng. Luôn kiểm tra nullptr trước khi dùng.
     */
    Driver* findDriver(std::string ID); 

    /*
     * deleteDriver
     * Chức năng : Xóa tài xế khỏi bảng băm theo ID.
     * Tham số   : ID - mã định danh tài xế cần xóa.
     * Lưu ý     : Nếu không tìm thấy ID thì in thông báo lỗi và không làm gì.
     */
    void deleteDriver(std::string ID);

    /*
     * updateLocation
     * Chức năng : Cập nhật vị trí (tọa độ) hiện tại của tài xế.
     * Tham số   : ID - mã định danh tài xế.
     *             x  - tọa độ x mới.
     *             y  - tọa độ y mới.
     * Lưu ý     : Nếu không tìm thấy ID thì in thông báo lỗi và không làm gì.
     */
    void updateLocation(std::string ID, double x, double y);

    /*
     * updateStatus
     * Chức năng : Cập nhật trạng thái hoạt động hiện tại của tài xế
     *             (ví dụ: rảnh, đang chạy... theo kiểu DriverStatusType).
     * Tham số   : ID     - mã định danh tài xế.
     *             status - trạng thái mới.
     * Lưu ý     : Nếu không tìm thấy ID thì in thông báo lỗi và không làm gì.
     */
    void updateStatus(std::string ID, DriverStatusType status);

    /*
     * updateFreeTime
     * Chức năng : Cập nhật thời gian rảnh (freeTime) của tài xế.
     * Tham số   : ID   - mã định danh tài xế.
     *             time - giá trị thời gian rảnh mới (kiểu int).
     * Lưu ý     : Nếu không tìm thấy ID thì in thông báo lỗi và không làm gì.
     */
    void updateFreeTime(std::string ID, int time);

    /*
     * getAllDrivers
     * Chức năng : Lấy toàn bộ tài xế đang có trong bảng băm.
     * Trả về    : std::vector<Driver> chứa BẢN SAO của mọi tài xế.
     * Lưu ý     : Thứ tự phần tử không được đảm bảo (unordered_map không
     *             có thứ tự). Sửa vector trả về không ảnh hưởng bảng băm.
     */
    std::vector<Driver> getAllDrivers();
};

#endif