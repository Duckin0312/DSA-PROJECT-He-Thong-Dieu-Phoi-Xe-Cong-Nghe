#ifndef OPERATION_H
#define OPERATION_H

#include "HashTable.hpp"
#include <vector>

/*
 * Lớp Operation
 * ---------------------------------------------------------------
 * Mục đích: Chứa các nghiệp vụ (logic xử lý) liên quan đến tài xế, được
 * xây dựng phía trên lớp HashTable. Bao gồm: ghi nhận lời mời cuốc, cập
 * nhật kết quả chuyến đi (đánh giá, hủy cuốc), cập nhật thông tin tài xế,
 * lọc tài xế đang rảnh và cảnh báo tài xế hay hủy cuốc.
 *
 * Lớp không tự lưu dữ liệu mà giữ MỘT THAM CHIẾU tới HashTable bên ngoài,
 * nên mọi thay đổi đều tác động trực tiếp lên bảng băm gốc. Vì Operation là
 * friend của HashTable, nó truy cập được thành viên private `data`.
 */
class Operation {
private:
    // Tham chiếu tới bảng băm chứa dữ liệu tài xế (không sở hữu, không sao chép).
    // Bảng băm này phải còn tồn tại trong suốt vòng đời của Operation.
    HashTable &table;
public:
    /*
     * Operation (hàm khởi tạo)
     * Chức năng : Tạo đối tượng Operation gắn với một HashTable có sẵn.
     * Tham số   : hashTable - bảng băm tài xế mà Operation sẽ thao tác trên đó.
     * Lưu ý     : Từ khóa `explicit` ngăn việc tự động chuyển kiểu ngầm từ
     *             HashTable sang Operation.
     */
    explicit Operation(HashTable &hashTable) : table(hashTable) {}

    /*
     * recordTripOffer
     * Chức năng : Ghi nhận một lời mời cuốc đã gửi cho tài xế và việc tài xế
     *             có nhận hay không; đồng thời tính lại tỷ lệ nhận cuốc (%).
     * Tham số   : ID       - mã tài xế được mời cuốc.
     *             accepted - true nếu tài xế nhận cuốc, false nếu từ chối.
     * Lưu ý     : Mỗi lời mời chỉ nên gọi hàm này một lần. Nếu ID không tồn
     *             tại, hàm im lặng thoát (không in thông báo).
     */
    void recordTripOffer(std::string ID, bool accepted);

    /*
     * updateTripResult
     * Chức năng : Cập nhật kết quả của một chuyến đi sau khi nó hoàn thành
     *             hoặc bị hủy, rồi đưa tài xế về trạng thái rảnh. Với cuốc
     *             hoàn thành thì cập nhật điểm đánh giá trung bình; với cuốc
     *             bị hủy thì tăng số cuốc hủy và có thể xóa tài xế vi phạm.
     * Tham số   : ID         - mã tài xế thực hiện chuyến đi.
     *             completed  - true nếu chuyến hoàn thành, false nếu bị hủy.
     *             tripRating - điểm đánh giá của chuyến (từ 1 đến 5), chỉ có
     *                          ý nghĩa khi completed = true. Mặc định 0.0f
     *                          (dùng khi chuyến bị hủy).
     * Lưu ý     : Nếu ID không tồn tại, hàm im lặng thoát.
     */
    void updateTripResult(std::string ID, bool completed, float tripRating = 0.0f);

    /*
     * updateDriverInfo
     * Chức năng : Cập nhật thông tin cá nhân và phương tiện của tài xế.
     * Tham số   : ID           - mã tài xế cần cập nhật.
     *             name         - họ tên mới.
     *             phone        - số điện thoại mới.
     *             licensePlate - biển số xe mới.
     *             vehicleType  - loại phương tiện mới.
     * Lưu ý     : Ghi đè cả 4 trường cùng lúc. Nếu ID không tồn tại, hàm im
     *             lặng thoát.
     */
    void updateDriverInfo(std::string ID, std::string name, std::string phone,
                          std::string licensePlate, VehicleType vehicleType);

    /*
     * getFreeDrivers (phiên bản không tham số)
     * Chức năng : Lấy danh sách ID của tất cả tài xế đang ở trạng thái rảnh (RANH).
     * Trả về    : std::vector<std::string> các ID; rỗng nếu không có ai rảnh.
     */
    std::vector<std::string> getFreeDrivers();

    /*
     * getFreeDrivers (phiên bản có lọc loại xe)
     * Chức năng : Lấy danh sách ID tài xế đang rảnh (RANH) VÀ có loại phương
     *             tiện trùng với loại được yêu cầu.
     * Tham số   : vehicleType - loại phương tiện cần lọc.
     * Trả về    : std::vector<std::string> các ID thỏa điều kiện; rỗng nếu không có.
     */
    std::vector<std::string> getFreeDrivers(VehicleType vehicleType);

    /*
     * warnHighCancellationDrivers
     * Chức năng : Quét toàn bộ tài xế, cảnh báo những người có tỷ lệ hủy cuốc
     *             từ ngưỡng cho trước trở lên (và đã có ít nhất 3 chuyến).
     * Tham số   : threshold - ngưỡng tỷ lệ hủy (%), hợp lệ từ 0 đến 100,
     *                         mặc định 30.0.
     * Trả về    : std::vector<std::string> ID các tài xế bị cảnh báo; rỗng
     *             nếu không có ai hoặc threshold không hợp lệ.
     * Lưu ý     : Tài xế đã bị cảnh báo mà tiếp tục hủy cuốc (vẫn vượt
     *             ngưỡng) sẽ bị xóa khỏi hệ thống trong updateTripResult.
     */
    std::vector<std::string> warnHighCancellationDrivers(double threshold = 30.0);
};

#endif