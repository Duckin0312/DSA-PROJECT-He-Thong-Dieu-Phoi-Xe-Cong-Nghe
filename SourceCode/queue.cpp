#include "queue.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
using namespace std;

/*
 * HeapNode::operator<
 * Chức năng : So sánh hai HeapNode theo điểm số để std::priority_queue sắp xếp.
 * Cách làm  : Trả về true nếu score của node hiện tại nhỏ hơn score của node
 *             kia. Vì priority_queue mặc định là max-heap, node có score cao
 *             nhất sẽ nằm ở đỉnh (top) và được lấy ra trước.
 */
bool HeapNode::operator<(const HeapNode& other) const {
    return this->score < other.score; 
}

/*
 * DriverMathAndFilterProcessor::calculateDistance
 * Chức năng : Tính khoảng cách Euclid giữa hai điểm.
 * Công thức : sqrt((x2 - x1)^2 + (y2 - y1)^2)
 * Trả về    : Khoảng cách (double, >= 0).
 */
double DriverMathAndFilterProcessor::calculateDistance(double x1, double y1, double x2, double y2) {
    return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

/*
 * DriverMathAndFilterProcessor::calculateTurningAngle
 * Chức năng : Tính góc (độ, 0 đến 180) giữa hướng đi hiện tại của tài xế và
 *             hướng từ tài xế tới khách. 0 độ = đang chạy thẳng về phía khách,
 *             180 độ = đang chạy ngược hướng khách.
 * Các bước  :
 *   1) v1 = (dX, dY): vector hướng di chuyển của tài xế.
 *      v2 = (targetX - currX, targetY - currY): vector từ tài xế tới khách.
 *   2) Tính tích vô hướng (dotProduct) và độ dài lenV1, lenV2 của hai vector.
 *   3) Nếu một trong hai vector có độ dài 0 thì trả về 0.0 (tránh chia cho 0).
 *   4) cosTheta = dotProduct / (lenV1 * lenV2), rồi kẹp vào [-1, 1] để tránh
 *      sai số làm tròn khiến acos trả về NaN.
 *   5) Dùng acos để ra góc theo radian, đổi sang độ bằng rad * 180 / PI.
 */
double DriverMathAndFilterProcessor::calculateTurningAngle(double dX, double dY, double targetX, double targetY, double currX, double currY) {
    double v1x = dX, v1y = dY; 
    double v2x = targetX - currX, v2y = targetY - currY; 

    double dotProduct = (v1x * v2x) + (v1y * v2y);
    double lenV1 = sqrt(v1x * v1x + v1y * v1y);
    double lenV2 = sqrt(v2x * v2x + v2y * v2y);

    if (lenV1 == 0 || lenV2 == 0) return 0.0; 

    double cosTheta = dotProduct / (lenV1 * lenV2);
    cosTheta = max(-1.0, min(1.0, cosTheta)); 
    
    double rad = acos(cosTheta);
    return rad * (180.0 / PI); 
}

/*
 * DriverMathAndFilterProcessor::calculateScore
 * Chức năng : Tính điểm ưu tiên tổng hợp của tài xế đối với khách. Mỗi tiêu
 *             chí được chuẩn hóa về khoảng [0, 1] rồi nhân trọng số.
 * Các tiêu chí (sau chuẩn hóa) và trọng số:
 *   - distScore       (w1 = 0.60): 1 - khoảng cách/10, tối thiểu 0. Càng gần
 *                      khách điểm càng cao; từ khoảng cách 10 trở lên điểm = 0.
 *   - ratingScore     (w2 = 0.05): rating / 5 (rating tối đa 5 sao).
 *   - acceptanceScore (w3 = 0.10): acceptanceRate / 100 (tỷ lệ nhận cuốc %).
 *   - anglePenalty    (w4 = 0.10): 1 - góc/180, góc lấy từ
 *                      calculateTurningAngle. Chạy thẳng về phía khách = 1,
 *                      chạy ngược hướng = 0 (tên "penalty" nhưng thực chất
 *                      là điểm: góc càng nhỏ điểm càng cao).
 *   - waitingScore    (w5 = 0.15): freeTime / 300, tối đa 1. Tài xế rảnh càng
 *                      lâu điểm càng cao, từ freeTime >= 300 là điểm tối đa.
 * Tổng các trọng số bằng 1.0 nên điểm cuối (finalScore) nằm trong [0, 1].
 * Tham số   : driver - tài xế cần chấm; customerX, customerY - vị trí khách.
 * Trả về    : finalScore - điểm tổng hợp, càng cao càng được ưu tiên gọi trước.
 */
double DriverMathAndFilterProcessor::calculateScore(const Driver& driver, double customerX, double customerY) {
    double dist = calculateDistance(driver.status.x, driver.status.y, customerX, customerY);
    double distScore = max(0.0, 1.0 - (dist / 10.0)); 
    double ratingScore = driver.performance.rating / 5.0;         
    double acceptanceScore = driver.performance.acceptanceRate / 100.0; 
    double angle = calculateTurningAngle(driver.status.diractionX, driver.status.diractionY, customerX, customerY, driver.status.x, driver.status.y);
    double anglePenalty = 1.0 - (angle / 180.0);              
    double waitingScore = min(1.0, driver.status.freeTime / 300.0); 
    double w1 = 0.6, w2 = 0.05, w3 = 0.1, w4 = 0.1, w5 = 0.15;
    double finalScore = (w1 * distScore) + 
                        (w2 * ratingScore) + 
                        (w3 * acceptanceScore) + 
                        (w4 * anglePenalty) + 
                        (w5 * waitingScore);
    return finalScore;
}

/*
 * DriverMathAndFilterProcessor::filterDriversInRadiusAndType
 * Chức năng : Lọc tài xế đang rảnh, đúng loại xe và nằm trong bán kính radiusR
 *             quanh vị trí khách.
 * Các bước  : Với mỗi tài xế trong danh sách:
 *   1) Bỏ qua nếu currentStatus khác RANH (không rảnh).
 *   2) Bỏ qua nếu vehicleType khác requiredVehicleType.
 *   3) Kiểm tra nhanh bằng "hình vuông bao": nếu độ lệch |x| hoặc |y| so với
 *      khách lớn hơn radiusR thì chắc chắn nằm ngoài bán kính, bỏ qua mà
 *      không cần tính bình phương.
 *   4) Kiểm tra chính xác: dx^2 + dy^2 <= radiusR^2 (so sánh bình phương để
 *      tránh phải gọi sqrt). Thỏa thì thêm bản sao tài xế vào kết quả.
 * Trả về    : vector<Driver> các tài xế thỏa cả 3 điều kiện.
 * Độ phức tạp: O(n) với n là số tài xế trong danh sách.
 */
vector<Driver> DriverMathAndFilterProcessor::filterDriversInRadiusAndType(const vector<Driver>& freeDriversList, 
                                            double customerX, double customerY, 
                                            double radiusR, VehicleType requiredVehicleType) {
    vector<Driver> resultList;
    double radiusSquared = radiusR * radiusR;
    for (const auto& driver : freeDriversList) {
         if (driver.status.currentStatus != RANH) continue;
         if (driver.info.vehicleType != requiredVehicleType) continue;

         double dx = abs(driver.status.x - customerX);
         double dy = abs(driver.status.y - customerY);
         if (dx > radiusR || dy > radiusR) {
            continue;
        }
         double distSquared = dx * dx + dy * dy;
         if (distSquared <= radiusSquared) {
            resultList.push_back(driver);
        }
    }
     return resultList; 
}

/*
 * processDispatch
 * Chức năng : Điều phối một yêu cầu đặt xe: chọn các tài xế phù hợp quanh
 *             khách, xếp hạng theo điểm và lần lượt gọi từ cao xuống thấp
 *             cho đến khi có người nhận chuyến hoặc hết tài xế.
 * Các bước  :
 *   1) Tạo DriverMathAndFilterProcessor và gọi filterDriversInRadiusAndType
 *      để lấy danh sách tài xế hợp lệ (rảnh, đúng loại xe, trong bán kính).
 *   2) Với mỗi tài xế hợp lệ, tính điểm bằng calculateScore rồi đẩy
 *      HeapNode{ID, điểm} vào priority_queue (max-heap: điểm cao nhất ở đỉnh).
 *   3) Vòng lặp khi heap chưa rỗng: lấy tài xế ở đỉnh, in thông báo đang gọi
 *      (kèm ID và Score), rồi xét kết quả:
 *        - Nếu tài xế nhận (isAccepted = true): in thông báo nhận chuyến và
 *          thoát vòng lặp (heap vẫn còn các tài xế còn lại).
 *        - Nếu từ chối / hết thời gian: in thông báo và pop tài xế khỏi heap
 *          để gọi người điểm cao kế tiếp.
 *   4) Nếu sau vòng lặp heap rỗng (không ai nhận) thì in thông báo không có
 *      tài xế nào nhận chuyến trong khu vực.
 * Lưu ý     : Hiện tại isAccepted được gán cứng là false (chỗ giả lập, chưa
 *             nối với phản hồi thật của tài xế), nên mọi tài xế đều bị coi là
 *             từ chối. Chỗ này cần thay bằng kết quả thực tế khi tích hợp.
 *             Hàm cũng chưa thực sự cập nhật trạng thái tài xế sang CO_KHACH,
 *             chỉ in thông báo.
 */
void processDispatch(vector<Driver>& allDrivers, double customerX, double customerY, double radiusR, VehicleType reqType) {
    DriverMathAndFilterProcessor processor;

    vector<Driver> validDrivers = processor.filterDriversInRadiusAndType(allDrivers, customerX, customerY, radiusR, reqType);
    
    

    priority_queue<HeapNode> pq;
    for (auto d : validDrivers) {
        double sc = processor.calculateScore(d, customerX, customerY);
        pq.push(HeapNode{d.info.getId(), sc});
    }

    while (!pq.empty()) {
        HeapNode topDriver = pq.top();
        
        cout << "[HỆ THỐNG] Đang gọi tài xế ID: " << topDriver.driverID << " với Score = " << topDriver.score << "\n";
        
        bool isAccepted = false; 
        
        if (isAccepted) {
            cout << "=> Tài xế đã nhận chuyến! Cập nhật trạng thái sang CO_KHACH.\n";
            break;
        } else {
            cout << "=> Tài xế từ chối hoặc hết thời gian (Timeout). Đẩy ra khỏi Heap!\n";
            pq.pop(); 
        }
    }

    if (pq.empty()) {
        cout << "[THÔNG BÁO] Không có tài xế nào nhận chuyến trong khu vực.\n";
    }
}