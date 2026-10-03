#include "HashTable.hpp"
#include <iostream>
#include <vector>
using namespace std;

/*
 * HashTable::checkID
 * Chức năng : Kiểm tra ID tài xế đã tồn tại trong bảng băm hay chưa.
 * Cách làm  : Dùng data.find(ID); nếu iterator trả về khác data.end()
 *             nghĩa là đã tìm thấy key.
 * Trả về    : true nếu tồn tại, false nếu không.
 * Độ phức tạp: O(1) trung bình.
 */
bool HashTable::checkID(string ID) {
    return data.find(ID) != data.end();
}

/*
 * HashTable::addDriver
 * Chức năng : Thêm tài xế mới vào bảng băm với key là ID.
 * Cách làm  : 1) Gọi checkID để xem ID đã có chưa.
 *             2) Nếu đã có: in "ID da ton tai!" rồi thoát, không ghi đè.
 *             3) Nếu chưa có: gán data[ID] = driver.
 * Độ phức tạp: O(1) trung bình.
 */
void HashTable::addDriver(string ID, Driver driver) {
    if(checkID(ID)) {
        cout << "ID da ton tai!\n";
        return;
    }
    data[ID] = driver;
}

/*
 * HashTable::findDriver
 * Chức năng : Tìm tài xế theo ID.
 * Cách làm  : Dùng data.find(ID).
 *             - Không thấy: trả về nullptr.
 *             - Thấy: trả về địa chỉ của Driver (it->second) nằm trong
 *               bảng băm, KHÔNG phải bản sao.
 * Lưu ý     : Nơi gọi hàm phải kiểm tra nullptr trước khi dùng con trỏ.
 * Độ phức tạp: O(1) trung bình.
 */
Driver* HashTable::findDriver(string ID) {
    auto it = data.find(ID);
    if(it == data.end()) {
        return nullptr;
    }
    return &(it->second);
}

/*
 * HashTable::deleteDriver
 * Chức năng : Xóa tài xế khỏi bảng băm theo ID.
 * Cách làm  : Nếu ID không tồn tại thì in "Khong tim thay tai xe!" và thoát;
 *             ngược lại gọi data.erase(ID) để xóa phần tử.
 * Độ phức tạp: O(1) trung bình.
 */
void HashTable::deleteDriver(string ID) {
    if(!checkID(ID)) {
        cout << "Khong tim thay tai xe!\n";
        return;
    }
    data.erase(ID);
}

/*
 * HashTable::updateLocation
 * Chức năng : Cập nhật vị trí hiện tại của tài xế.
 * Cách làm  : Nếu ID không tồn tại thì in "Khong tim thay tai xe!" và thoát;
 *             ngược lại gán tọa độ mới vào status.x và status.y của tài xế.
 * Tham số   : x, y - tọa độ mới (kiểu double).
 */
void HashTable::updateLocation(string ID, double x, double y) {
    if(!checkID(ID)) {
        cout << "Khong tim thay tai xe!\n";
        return;
    }
    data[ID].status.x = x;
    data[ID].status.y = y;
}

/*
 * HashTable::updateStatus
 * Chức năng : Cập nhật trạng thái hoạt động hiện tại của tài xế.
 * Cách làm  : Nếu ID không tồn tại thì in "Khong tim thay tai xe!" và thoát;
 *             ngược lại gán giá trị mới vào status.currentStatus.
 * Tham số   : status - trạng thái mới (kiểu DriverStatusType).
 */
void HashTable::updateStatus(string ID, DriverStatusType status) {
    if(!checkID(ID)) {
        cout << "Khong tim thay tai xe!\n";
        return;
    }
    data[ID].status.currentStatus = status;
}

/*
 * HashTable::updateFreeTime
 * Chức năng : Cập nhật thời gian rảnh của tài xế.
 * Cách làm  : Nếu ID không tồn tại thì in "Khong tim thay tai xe!" và thoát;
 *             ngược lại gán giá trị mới vào status.freeTime.
 * Tham số   : time - thời gian rảnh mới (kiểu int).
 */
void HashTable::updateFreeTime(string ID, int time) {
    if(!checkID(ID)) {
        cout << "Khong tim thay tai xe!\n";
        return;
    }
    data[ID].status.freeTime = time;
}

/*
 * HashTable::getAllDrivers
 * Chức năng : Lấy danh sách toàn bộ tài xế đang lưu trong bảng băm.
 * Cách làm  : 1) Tạo vector rỗng và reserve(data.size()) để cấp phát sẵn
 *                bộ nhớ, tránh phải cấp phát lại nhiều lần.
 *             2) Duyệt toàn bộ unordered_map, push_back phần tử second
 *                (đối tượng Driver) vào vector.
 * Trả về    : vector<Driver> chứa bản sao của tất cả tài xế; thứ tự không
 *             được đảm bảo. Sửa vector này không ảnh hưởng bảng băm gốc.
 * Độ phức tạp: O(n) với n là số tài xế.
 */
vector<Driver> HashTable::getAllDrivers(){
    vector<Driver> drivers;
    drivers.reserve(data.size());
    for(const auto& driver : data){
        drivers.push_back(driver.second);
    }
    return drivers;
}