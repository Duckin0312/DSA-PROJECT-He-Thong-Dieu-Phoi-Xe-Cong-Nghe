#include "Driver.hpp"
using namespace std;

/**
 * @file Driver.cpp
 * Cài đặt các hàm getter/setter của lớp DriverInfo (thông tin cá nhân của tài xế).
 * Khai báo lớp nằm trong Driver.hpp. Các hàm ở đây chỉ đọc hoặc ghi trực tiếp các
 * thuộc tính private, không kiểm tra tính hợp lệ của dữ liệu; việc kiểm tra (ví dụ name
 * không được rỗng) do tầng gọi (các route trong main.cpp) đảm nhiệm.
 */

/**
 * @brief  DriverInfo::getId, lấy mã định danh của tài xế.
 * @return Chuỗi id (trả về bản sao).
 */
string DriverInfo::getId(){
    return id;
}

/**
 * @brief  DriverInfo::getName, lấy họ tên của tài xế.
 * @return Chuỗi họ tên (trả về bản sao).
 */
string DriverInfo::getName() {
    return name;
}

/**
 * @brief DriverInfo::setId, gán mã định danh cho tài xế.
 * @param id Id mới; ghi đè giá trị cũ. (`this->id` là thuộc tính của đối tượng,
 *           `id` là tham số truyền vào, hai tên trùng nhau nên phải dùng `this->`.)
 */
void DriverInfo::setId(string id) {
    this->id = id;
}

/**
 * @brief DriverInfo::setName, gán họ tên cho tài xế.
 * @param name Họ tên mới; ghi đè giá trị cũ.
 */
void DriverInfo::setName(string name) {
    this->name = name;
}

/**
 * @brief  DriverInfo::getPhone, lấy số điện thoại của tài xế.
 * @return Chuỗi số điện thoại (trả về bản sao).
 */
string DriverInfo::getPhone() {
    return phone;
}

/**
 * @brief DriverInfo::setPhone, gán số điện thoại cho tài xế.
 * @param phone Số điện thoại mới; ghi đè giá trị cũ.
 */
void DriverInfo::setPhone(string phone) {
    this->phone = phone;
}

/**
 * @brief  DriverInfo::getLicensePlate, lấy biển số xe của tài xế.
 * @return Chuỗi biển số xe (trả về bản sao).
 */
string DriverInfo::getLicensePlate() {
    return licensePlate;
}

/**
 * @brief DriverInfo::setLicensePlate, gán biển số xe cho tài xế.
 * @param licensePlate Biển số xe mới; ghi đè giá trị cũ.
 */
void DriverInfo::setLicensePlate(string licensePlate) {
    this->licensePlate = licensePlate;
}