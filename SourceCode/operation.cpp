#include "operation.hpp"
using namespace std;

/*
 * Operation::recordTripOffer
 * Chức năng : Ghi nhận một lời mời cuốc gửi cho tài xế và kết quả (nhận / từ
 *             chối), sau đó tính lại tỷ lệ nhận cuốc.
 * Các bước  :
 *   1) Tìm tài xế theo ID; không thấy thì thoát im lặng.
 *   2) Tăng offeredTrips (số cuốc đã được mời) lên 1.
 *   3) Nếu accepted = true thì tăng acceptedTrips (số cuốc đã nhận) lên 1.
 *   4) acceptanceRate = 100 * acceptedTrips / offeredTrips (đơn vị %).
 * Lưu ý     : Mỗi lời mời cuốc chỉ được ghi nhận một lần (người gọi hàm
 *             có trách nhiệm không gọi lặp lại cho cùng một lời mời).
 */
void Operation::recordTripOffer(string ID, bool accepted) {
    auto it = table.data.find(ID);
    if(it == table.data.end()) return;
    DriverPerformance &p = it->second.performance;
    p.offeredTrips++;
    if(accepted) p.acceptedTrips++;
    p.acceptanceRate = 100.0f * p.acceptedTrips / p.offeredTrips;
}

/*
 * Operation::updateTripResult
 * Chức năng : Cập nhật kết quả chuyến đi (hoàn thành hoặc bị hủy) và đưa tài
 *             xế về trạng thái rảnh. Gọi sau khi cuốc đã hoàn thành hoặc bị
 *             hủy; điểm rating chỉ áp dụng cho cuốc hoàn thành.
 * Các bước  :
 *   1) Tìm tài xế theo ID; không thấy thì thoát im lặng.
 *   2) Nếu cuốc HOÀN THÀNH (completed = true):
 *        - Kiểm tra tripRating trong khoảng [1, 5]. Nếu sai thì in
 *          "Rating phai tu 1 den 5!" và thoát, KHÔNG thay đổi gì cả
 *          (kể cả trạng thái tài xế).
 *        - Cập nhật điểm trung bình:
 *          rating = (rating * completedTrips + tripRating) / (completedTrips + 1)
 *          rồi tăng completedTrips lên 1.
 *   3) Nếu cuốc BỊ HỦY (completed = false):
 *        - Tăng cancelledTrips lên 1 và tính tổng số cuốc đã kết thúc
 *          (total = completedTrips + cancelledTrips) cùng tỷ lệ hủy (%).
 *        - Nếu tài xế ĐÃ bị cảnh báo trước đó (cancellationWarned) VÀ
 *          total >= 3 VÀ tỷ lệ hủy >= cancellationThreshold thì xóa tài xế
 *          khỏi bảng băm, in thông báo và thoát.
 *   4) Với các trường hợp còn lại, đặt lại trạng thái tài xế: xóa
 *      currentTripID, đặt currentStatus = RANH, freeTime = 0.
 * Tham số   : ID         - mã tài xế.
 *             completed  - true: hoàn thành, false: bị hủy.
 *             tripRating - điểm đánh giá 1..5 (bỏ qua khi bị hủy).
 */
void Operation::updateTripResult(string ID, bool completed, float tripRating) {
    auto it = table.data.find(ID);
    if(it == table.data.end()) return;
    DriverPerformance &p = it->second.performance;
    if(completed) {
        if(tripRating < 1.0f || tripRating > 5.0f) {
            cout << "Rating phai tu 1 den 5!\n";
            return;
        }
        p.rating = (p.rating * p.completedTrips + tripRating) / (p.completedTrips + 1);
        p.completedTrips++;
    } else {
        p.cancelledTrips++;
        int total = p.completedTrips + p.cancelledTrips;
        double rate = 100.0 * p.cancelledTrips / total;
        if(p.cancellationWarned && total >= 3 && rate >= p.cancellationThreshold) {
            table.data.erase(it);
            cout << "Tai xe " << ID << " tiep tuc huy cuoc, da xoa!\n";
            return;
        }
    }
    it->second.status.currentTripID = "";
    it->second.status.currentStatus = RANH;
    it->second.status.freeTime = 0;
}

/*
 * Operation::updateDriverInfo
 * Chức năng : Cập nhật thông tin cá nhân và phương tiện của tài xế.
 * Các bước  :
 *   1) Tìm tài xế theo ID; không thấy thì thoát im lặng.
 *   2) Gọi các hàm setName, setPhone, setLicensePlate để cập nhật tên, số
 *      điện thoại, biển số; gán trực tiếp vehicleType cho loại xe.
 * Tham số   : ID, name, phone, licensePlate, vehicleType - mã tài xế và các
 *             giá trị mới cần ghi đè.
 */
void Operation::updateDriverInfo(string ID, string name, string phone,
                                 string licensePlate, VehicleType vehicleType) {
    auto it = table.data.find(ID);
    if(it == table.data.end()) return;
    it->second.info.setName(name);
    it->second.info.setPhone(phone);
    it->second.info.setLicensePlate(licensePlate);
    it->second.info.vehicleType = vehicleType;
}

/*
 * Operation::getFreeDrivers (không tham số)
 * Chức năng : Lấy danh sách ID của mọi tài xế đang rảnh.
 * Cách làm  : Duyệt toàn bộ bảng băm, tài xế nào có currentStatus == RANH thì
 *             thêm ID (item.first) vào kết quả.
 * Trả về    : vector<string> các ID tài xế rảnh (rỗng nếu không có).
 * Độ phức tạp: O(n) với n là số tài xế.
 */
vector<string> Operation::getFreeDrivers() {
    vector<string> result;
    for(const auto &item : table.data)
        if(item.second.status.currentStatus == RANH) result.push_back(item.first);
    return result;
}

/*
 * Operation::getFreeDrivers (lọc theo loại xe)
 * Chức năng : Lấy danh sách ID tài xế đang rảnh và có loại phương tiện đúng
 *             với yêu cầu.
 * Cách làm  : Duyệt toàn bộ bảng băm, chỉ thêm ID khi đồng thời thỏa
 *             currentStatus == RANH và info.vehicleType == vehicleType.
 * Tham số   : vehicleType - loại phương tiện cần tìm.
 * Trả về    : vector<string> các ID thỏa điều kiện (rỗng nếu không có).
 * Độ phức tạp: O(n) với n là số tài xế.
 */
vector<string> Operation::getFreeDrivers(VehicleType vehicleType) {
    vector<string> result;
    for(const auto &item : table.data)
        if(item.second.status.currentStatus == RANH &&
           item.second.info.vehicleType == vehicleType) result.push_back(item.first);
    return result;
}

/*
 * Operation::warnHighCancellationDrivers
 * Chức năng : Quét toàn bộ tài xế và cảnh báo những người hủy cuốc quá nhiều.
 * Các bước  :
 *   1) Nếu threshold nằm ngoài [0, 100] thì trả về danh sách rỗng, không làm gì.
 *   2) Với mỗi tài xế, tính total = completedTrips + cancelledTrips.
 *   3) Nếu total >= 3 VÀ tỷ lệ hủy (cancelledTrips / total * 100) >= threshold:
 *        - Đặt cancellationWarned = true và lưu cancellationThreshold = threshold
 *          (để updateTripResult dùng khi tài xế tiếp tục hủy cuốc).
 *        - Thêm ID vào kết quả và in cảnh báo ra màn hình.
 * Tham số   : threshold - ngưỡng tỷ lệ hủy (%), mặc định 30.0 (khai báo ở .hpp).
 * Trả về    : vector<string> ID các tài xế vừa bị cảnh báo.
 * Độ phức tạp: O(n) với n là số tài xế.
 */
vector<string> Operation::warnHighCancellationDrivers(double threshold) {
    vector<string> result;
    if(threshold < 0.0 || threshold > 100.0) return result;
    for(auto &item : table.data) {
        DriverPerformance &p = item.second.performance;
        int total = p.completedTrips + p.cancelledTrips;
        if(total >= 3 && 100.0 * p.cancelledTrips / total >= threshold) {
            p.cancellationWarned = true;
            p.cancellationThreshold = threshold;
            result.push_back(item.first);
            cout << "Canh bao tai xe " << item.first << ": ty le huy cuoc cao!\n";
        }
    }
    return result;
}