#ifndef QUEUE_H
#define QUEUE_H

#include "Driver.hpp"
#include <vector>
#include <queue>
#include <string>

using namespace std;

/*
 * Struct HeapNode
 * ---------------------------------------------------------------
 * Mục đích: Một phần tử trong hàng đợi ưu tiên (heap) dùng để xếp hạng
 * tài xế khi điều phối chuyến. Mỗi node gắn một tài xế với điểm số của họ.
 */
struct HeapNode {
    string driverID;   // Mã định danh của tài xế.
    double score;      // Điểm ưu tiên của tài xế (càng cao càng được gọi trước).

    /*
     * operator<
     * Chức năng : Định nghĩa phép so sánh "nhỏ hơn" giữa hai HeapNode dựa
     *             trên điểm số (score), để std::priority_queue biết cách
     *             sắp xếp. Node có score lớn nhất sẽ nằm ở đỉnh heap.
     * Tham số   : other - node cần so sánh với node hiện tại.
     * Trả về    : true nếu score của node hiện tại nhỏ hơn score của other.
     */
    bool operator<(const HeapNode& other) const;
};

/*
 * Lớp DriverMathAndFilterProcessor
 * ---------------------------------------------------------------
 * Mục đích: Tập hợp các hàm tính toán (khoảng cách, góc quay, điểm số) và
 * lọc tài xế phục vụ việc chọn tài xế phù hợp nhất cho một khách hàng.
 * Lớp không lưu dữ liệu tài xế; chỉ có hằng số PI để tính góc.
 */
class DriverMathAndFilterProcessor {
private:
    // Hằng số Pi, dùng để đổi góc từ radian sang độ trong calculateTurningAngle.
    const double PI = 3.14159265358979323846;

public:
    /*
     * calculateDistance
     * Chức năng : Tính khoảng cách Euclid giữa hai điểm (x1, y1) và (x2, y2).
     * Tham số   : x1, y1 - tọa độ điểm thứ nhất.
     *             x2, y2 - tọa độ điểm thứ hai.
     * Trả về    : Khoảng cách giữa hai điểm (double, luôn >= 0).
     */
    double calculateDistance(double x1, double y1, double x2, double y2);

    /*
     * calculateTurningAngle
     * Chức năng : Tính góc (đơn vị độ, từ 0 đến 180) giữa hướng đang di
     *             chuyển của tài xế và hướng từ vị trí tài xế tới khách.
     *             Góc càng nhỏ nghĩa là tài xế càng đang chạy về phía khách.
     * Tham số   : dX, dY           - vector hướng di chuyển hiện tại của tài xế.
     *             targetX, targetY - tọa độ điểm đích (vị trí khách).
     *             currX, currY     - tọa độ hiện tại của tài xế.
     * Trả về    : Góc giữa hai vector (độ). Trả về 0 nếu một trong hai vector
     *             có độ dài bằng 0 (không xác định được hướng).
     */
    double calculateTurningAngle(double dX, double dY, double targetX, double targetY, double currX, double currY);

    /*
     * calculateScore
     * Chức năng : Tính điểm ưu tiên tổng hợp của một tài xế đối với một
     *             khách hàng. Điểm càng cao, tài xế càng nên được gọi trước.
     *             Điểm là tổng có trọng số của 5 tiêu chí: khoảng cách,
     *             đánh giá, tỷ lệ nhận cuốc, hướng di chuyển, thời gian chờ.
     * Tham số   : driver    - tài xế cần chấm điểm (không bị sửa đổi).
     *             customerX - tọa độ x của khách.
     *             customerY - tọa độ y của khách.
     * Trả về    : Điểm tổng hợp (double), xấp xỉ trong khoảng [0, 1].
     */
    double calculateScore(const Driver& driver, double customerX, double customerY);

    /*
     * filterDriversInRadiusAndType
     * Chức năng : Lọc ra những tài xế thỏa đồng thời 3 điều kiện: đang rảnh
     *             (RANH), đúng loại phương tiện yêu cầu, và nằm trong bán kính
     *             radiusR quanh vị trí khách.
     * Tham số   : freeDriversList    - danh sách tài xế cần lọc.
     *             customerX, customerY - tọa độ khách (tâm vùng lọc).
     *             radiusR            - bán kính tìm kiếm.
     *             requiredVehicleType - loại phương tiện khách yêu cầu.
     * Trả về    : vector<Driver> chứa BẢN SAO các tài xế thỏa điều kiện.
     */
    vector<Driver> filterDriversInRadiusAndType(const vector<Driver>& freeDriversList, 
                                                double customerX, double customerY, 
                                                double radiusR, VehicleType requiredVehicleType);
};

/*
 * processDispatch
 * Chức năng : Điều phối một yêu cầu đặt xe: lọc các tài xế phù hợp quanh
 *             khách, xếp hạng bằng điểm số trong một priority_queue, rồi lần
 *             lượt gọi từ tài xế điểm cao nhất xuống cho đến khi có người nhận
 *             hoặc hết tài xế.
 * Tham số   : allDrivers - danh sách tất cả tài xế của hệ thống.
 *             customerX, customerY - tọa độ khách.
 *             radiusR    - bán kính tìm tài xế.
 *             reqType    - loại phương tiện khách yêu cầu.
 * Lưu ý     : Hàm này là hàm tự do (không thuộc lớp nào).
 */
void processDispatch(vector<Driver>& allDrivers, double customerX, double customerY, double radiusR, VehicleType reqType);

#endif