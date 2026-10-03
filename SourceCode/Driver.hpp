#ifndef DRIVER_H
#define DRIVER_H

#include <iostream>

/**
 * @file Driver.hpp
 * Khai báo các kiểu dữ liệu mô tả một TÀI XẾ trong hệ thống điều phối xe:
 *   - VehicleType       : loại phương tiện (xe máy / ô tô).
 *   - DriverStatusType  : trạng thái hoạt động (rảnh / có khách / ngừng chạy).
 *   - DriverInfo        : thông tin cá nhân (id, tên, SĐT, biển số, loại xe).
 *   - DriverStatus      : trạng thái thời gian thực (vị trí, hướng chạy, trạng thái, chuyến hiện tại).
 *   - DriverPerformance : thống kê hiệu suất (rating, số cuốc hoàn thành/nhận/hủy...).
 *   - Driver            : gộp ba nhóm trên thành một tài xế hoàn chỉnh.
 * Phần cài đặt các hàm của DriverInfo nằm trong Driver.cpp.
 */

/**
 * @enum VehicleType
 * Loại phương tiện của tài xế. Khi lưu JSON/nhận từ API được biểu diễn bằng số nguyên.
 */
enum VehicleType {
    XE_MAY,   // = 0: xe máy
    O_TO      // = 1: ô tô
};

/**
 * @enum DriverStatusType
 * Trạng thái hoạt động hiện tại của tài xế. Khi lưu JSON/nhận từ API được biểu diễn bằng số nguyên.
 */
enum DriverStatusType {
    RANH,        // = 0: đang rảnh, sẵn sàng nhận cuốc
    CO_KHACH,    // = 1: đang chở khách (đang thực hiện một chuyến)
    NGUNG_CHAY   // = 2: tạm ngừng hoạt động, không nhận cuốc
};

/**
 * @class DriverInfo
 * Lưu THÔNG TIN CÁ NHÂN của tài xế.
 * Các trường name, phone, licensePlate, id là private nên chỉ đọc/ghi qua getter/setter;
 * riêng vehicleType là public nên truy cập trực tiếp.
 * @note Các getter không được khai báo const, vì vậy không gọi được trên đối tượng const.
 */
class DriverInfo {
private:
    std::string name;            // Họ tên tài xế
    std::string phone;           // Số điện thoại liên hệ
    std::string licensePlate;    // Biển số xe
    std::string id;              // Mã định danh duy nhất, dùng làm khóa trong bảng băm
public:
    VehicleType vehicleType;     // Loại xe (XE_MAY hoặc O_TO); truy cập trực tiếp, không có getter/setter

    /**
     * @brief  Lấy họ tên của tài xế.
     * @return Chuỗi họ tên.
     */
    std::string getName();

    /**
     * @brief Đặt (ghi đè) họ tên của tài xế.
     * @param name Họ tên mới.
     */
    void setName(std::string name);

    /**
     * @brief  Lấy số điện thoại của tài xế.
     * @return Chuỗi số điện thoại.
     */
    std::string getPhone();

    /**
     * @brief Đặt (ghi đè) số điện thoại của tài xế.
     * @param phone Số điện thoại mới.
     */
    void setPhone(std::string phone);

    /**
     * @brief  Lấy biển số xe của tài xế.
     * @return Chuỗi biển số xe.
     */
    std::string getLicensePlate();

    /**
     * @brief Đặt (ghi đè) biển số xe của tài xế.
     * @param licensePlate Biển số xe mới.
     */
    void setLicensePlate(std::string licensePlate);

    /**
     * @brief  Lấy mã định danh (id) của tài xế.
     * @return Chuỗi id.
     */
    std::string getId();

    /**
     * @brief Đặt (ghi đè) mã định danh (id) của tài xế.
     * @param id Id mới.
     * @note  Đổi id của tài xế đã nằm trong HashTable có thể làm lệch khóa lưu trong bảng băm,
     *        nên chỉ nên đặt id khi tạo mới tài xế.
     */
    void setId(std::string id);
};

/**
 * @struct DriverStatus
 * TRẠNG THÁI THỜI GIAN THỰC của tài xế: đang ở đâu, chạy hướng nào, đang làm gì.
 * @note Struct này không có giá trị khởi tạo mặc định: nếu tạo bằng Driver() thì các trường
 *       phải được gán thủ công (xem jsonToDriver trong main.cpp). Constructor có tham số của Driver
 *       đã gán sẵn giá trị ban đầu.
 */
struct DriverStatus {
    double x, y;                     // Tọa độ hiện tại của tài xế trên bản đồ
    double diractionX, diractionY;   // Vector hướng đang chạy (tên "diraction" viết sai chính tả từ "direction", giữ nguyên để không phá code cũ)
    DriverStatusType currentStatus;  // Trạng thái hoạt động: RANH / CO_KHACH / NGUNG_CHAY
    int freeTime;                    // Thời gian tài xế đã rảnh (dùng khi xếp hạng điều phối); 0 khi không rảnh
    std::string currentTripID;       // Mã chuyến đang thực hiện; chuỗi rỗng "" nếu chưa có chuyến
};

/**
 * @struct DriverPerformance
 * THỐNG KÊ HIỆU SUẤT của tài xế, dùng để đánh giá và xếp hạng khi điều phối.
 * @note rating, completedTrips, acceptedTrips, acceptanceRate không có giá trị mặc định
 *       (cần gán thủ công); bốn trường còn lại có mặc định ngay trong khai báo.
 */
struct DriverPerformance {
    float rating;                        // Điểm đánh giá trung bình (thang 1 đến 5); tài xế mới bắt đầu ở 5.0
    int completedTrips;                  // Số cuốc đã hoàn thành
    int acceptedTrips;                   // Số cuốc tài xế đã nhận
    float acceptanceRate;                // Tỷ lệ nhận cuốc (nhận / được mời)
    int offeredTrips = 0;                // Số cuốc đã được mời cho tài xế (mặc định 0)
    int cancelledTrips = 0;              // Số cuốc tài xế đã hủy (mặc định 0)
    bool cancellationWarned = false;     // Đã bị cảnh báo vì hủy cuốc nhiều hay chưa (mặc định false)
    double cancellationThreshold = 30.0; // Ngưỡng tỷ lệ hủy cuốc (%) dùng để cảnh báo (mặc định 30%)
};

/**
 * @struct Driver
 * Một TÀI XẾ hoàn chỉnh, gồm ba nhóm dữ liệu:
 *   - info        : thông tin cá nhân.
 *   - status      : trạng thái thời gian thực.
 *   - performance : thống kê hiệu suất.
 * Đây là kiểu dữ liệu chính được lưu trong HashTable, ghi vào database.json và trả qua API.
 */
struct Driver {
    DriverInfo info;               // Thông tin cá nhân
    DriverStatus status;           // Trạng thái hiện tại
    DriverPerformance performance; // Thống kê hiệu suất

    /**
     * @brief Constructor mặc định: tạo tài xế "trống".
     * @note  Dùng `= default` nên KHÔNG khởi tạo các trường của info và status, cùng các trường
     *        rating/completedTrips/acceptedTrips/acceptanceRate của performance (giá trị rác).
     *        Người dùng phải gán đầy đủ sau khi tạo, ví dụ khi đọc từ JSON (hàm jsonToDriver).
     */
    Driver() = default;

    /**
     * @brief Constructor tạo tài xế MỚI với giá trị ban đầu hợp lý.
     * @param id           Mã định danh duy nhất.
     * @param name         Họ tên.
     * @param phone        Số điện thoại.
     * @param licensePlate Biển số xe.
     * @param type         Loại xe (XE_MAY hoặc O_TO).
     * @note  Giá trị khởi tạo:
     *        - status: vị trí (0, 0), hướng (0, 0), currentStatus = RANH, freeTime = 0, currentTripID = "".
     *        - performance: rating = 5.0, completedTrips = acceptedTrips = 0, acceptanceRate = 0.
     *          (offeredTrips, cancelledTrips, cancellationWarned, cancellationThreshold lấy mặc định
     *          từ khai báo của DriverPerformance.)
     */
    Driver(std::string id, std::string name, std::string phone, std::string licensePlate, VehicleType type) {
        info.setId(id);
        info.setName(name);
        info.setPhone(phone);
        info.setLicensePlate(licensePlate);
        info.vehicleType = type;

        status.x = 0.0;
        status.y = 0.0;
        status.diractionX = 0.0;
        status.diractionY = 0.0;
        status.currentStatus = RANH;
        status.freeTime = 0;
        status.currentTripID = "";

        performance.rating = 5.0f;
        performance.completedTrips = 0;
        performance.acceptedTrips = 0;
        performance.acceptanceRate = 0.0f;
    }
};

#endif