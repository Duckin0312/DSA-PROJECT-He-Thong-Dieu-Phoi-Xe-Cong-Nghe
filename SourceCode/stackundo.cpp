#include "stackundo.hpp"
#include "operation.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std; 

/*
 * stateToString
 * Chức năng : Chuyển State thành chuỗi mô tả (tiếng Việt không dấu) để in log.
 * Cách làm  : Dùng switch trên state, ví dụ TIM_XE -> "Tim xe",
 *             NHAN_CUOC -> "Nhan cuoc", CHO_XAC_NHAN_HUY -> "Cho xac nhan huy
 *             (Dem 5s)", HUY_HOAN_TOAN -> "Huy hoan toan".
 *             Giá trị ngoài enum trả về "Khong xac dinh".
 */
string stateToString(State state){
    switch (state) {
        case TIM_XE: return "Tim xe";
        case NHAN_CUOC: return "Nhan cuoc";
        case CHO_XAC_NHAN_HUY: return "Cho xac nhan huy (Dem 5s)";
        case HUY_HOAN_TOAN: return "Huy hoan toan";
        default: return "Khong xac dinh";
    }
}

/*
 * getCurrentTimestamp
 * Chức năng : Lấy thời gian hiện tại theo mili giây kể từ epoch (1/1/1970).
 * Cách làm  : Lấy system_clock::now(), tính khoảng thời gian từ epoch rồi
 *             đổi sang mili giây bằng duration_cast.
 * Trả về    : Timestamp (long long) dùng làm nhãn thời gian của Trip.
 */
long long getCurrentTimestamp() {
    return chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch()
    ).count();
}

/*
 * TripManager::TripManager (hàm khởi tạo)
 * Chức năng : Tạo bộ quản lý chuyến đi mới.
 * Cách làm  : Lưu con trỏ operation (danh sách khởi tạo), rồi gọi pushState
 *             để ghi nhận trạng thái đầu tiên là TIM_XE.
 */
TripManager::TripManager(const string& driverID, const string& tripID, Operation* operation) : operation(operation) {
    pushState(TIM_XE, driverID, tripID);
}

/*
 * TripManager::pushState
 * Chức năng : Ghi nhận trạng thái mới cho chuyến đi.
 * Cách làm  : Tạo một Trip gồm {state, driverID, tripID, timestamp hiện tại}
 *             rồi push lên đỉnh stack tripHistory.
 */
void TripManager::pushState(State newState, const string& driverID, const string& tripID) {
    Trip t = { newState, driverID, tripID, getCurrentTimestamp() };
    tripHistory.push(t);
}

/*
 * TripManager::getCurrentState
 * Chức năng : Lấy trạng thái hiện tại của chuyến.
 * Cách làm  : Nếu stack không rỗng thì trả về state ở đỉnh stack. Nếu rỗng
 *             (lịch sử đã bị xóa sau khi hủy hoàn toàn) thì trả về
 *             HUY_HOAN_TOAN.
 */
State TripManager::getCurrentState() const {
    if (!tripHistory.empty()) {
        return tripHistory.top().state;
    }
    return HUY_HOAN_TOAN;
}

/*
 * TripManager::cancelTripRequest
 * Chức năng : Hủy chuyến theo kiểu CHẶN (blocking): đợi đủ 5 giây rồi hủy
 *             hoàn toàn.
 * Các bước  :
 *   1) Nếu stack rỗng thì thoát. Ngược lại lấy bản ghi hiện tại (đỉnh stack)
 *      để biết driverID và tripID.
 *   2) Đẩy trạng thái CHO_XAC_NHAN_HUY lên stack.
 *   3) Vòng lặp chờ: dùng steady_clock đo thời gian đã trôi qua, mỗi vòng
 *      ngủ 50 ms; thoát vòng lặp khi đủ 5000 ms. (Trong vòng lặp có tính số
 *      giây còn lại, remainingSeconds, nhưng hiện chưa in ra.)
 *   4) Sau khi hết giờ:
 *        - Nếu undoExecuted = true: pop trạng thái chờ hủy (hoàn tác).
 *        - Ngược lại: đẩy HUY_HOAN_TOAN, xóa toàn bộ lịch sử bằng
 *          clearTripData và cập nhật thống kê tài xế (cuốc bị hủy) bằng
 *          updateDriverStats(driverID, false).
 * Lưu ý     : Hàm chặn luồng gọi suốt 5 giây. Biến undoExecuted là biến cục
 *             bộ khởi tạo false và không có đoạn code nào đổi nó trong lúc
 *             chờ, nên hiện tại nhánh hoàn tác không bao giờ chạy: hàm luôn
 *             kết thúc bằng hủy hoàn toàn. Để hủy có hoàn tác thật sự, dùng
 *             requestCancel / undoCancel / finalizeCancelIfExpired.
 */
void TripManager::cancelTripRequest() {
    if (tripHistory.empty()) return;

    Trip currentTrip = tripHistory.top();

    pushState(CHO_XAC_NHAN_HUY, currentTrip.driverID, currentTrip.tripID);

    bool undoExecuted = false;
    auto startTime = chrono::steady_clock::now();
    int lastDisplayedSecond = 5;

    while (true) {
        auto currentTime = chrono::steady_clock::now();
        auto elapsedMs = chrono::duration_cast<chrono::milliseconds>(currentTime - startTime).count();

        if (elapsedMs >= 5000) {
            break;
        }

        int remainingSeconds = 5 - static_cast<int>(elapsedMs / 1000);
        if (remainingSeconds < lastDisplayedSecond) {
            lastDisplayedSecond = remainingSeconds;
        }

        this_thread::sleep_for(chrono::milliseconds(50));
    }

    if (undoExecuted) {
        tripHistory.pop();
    }
    else {
        pushState(HUY_HOAN_TOAN, currentTrip.driverID, currentTrip.tripID);

        clearTripData();
        updateDriverStats(currentTrip.driverID, false);
    }
}

/*
 * TripManager::clearTripData
 * Chức năng : Xóa toàn bộ lịch sử trạng thái của chuyến.
 * Cách làm  : Pop liên tục cho đến khi stack tripHistory rỗng. Sau đó
 *             getCurrentState() sẽ trả về HUY_HOAN_TOAN.
 */
void TripManager::clearTripData() {
    while (!tripHistory.empty()) {
        tripHistory.pop();
    }
}

/*
 * TripManager::updateDriverStats
 * Chức năng : Cập nhật thống kê hiệu suất của tài xế khi chuyến kết thúc.
 * Cách làm  : Nếu operation là nullptr thì thoát. Ngược lại gọi
 *             operation->updateTripResult(driverID, completed, tripRating).
 *             Hàm đó sẽ cập nhật điểm đánh giá (nếu hoàn thành) hoặc tăng số
 *             cuốc hủy (nếu bị hủy), và đưa tài xế về trạng thái rảnh. Tài xế
 *             đã bị cảnh báo hủy nhiều mà tiếp tục hủy có thể bị xóa khỏi hệ
 *             thống (xem Operation::updateTripResult).
 * Tham số   : driverID, completed, tripRating - xem mô tả ở stackundo.hpp.
 */
void TripManager::updateDriverStats(const string& driverID, bool completed, float tripRating) {
    if (operation == nullptr) return;
    operation->updateTripResult(driverID, completed, tripRating);
}

/*
 * TripManager::requestCancel
 * Chức năng : Bắt đầu hủy chuyến theo kiểu KHÔNG chặn: mở cửa sổ hoàn tác
 *             5 giây rồi trả về ngay.
 * Các bước  :
 *   1) Trả về false nếu lịch sử rỗng hoặc chuyến đã ở CHO_XAC_NHAN_HUY
 *      (tránh bấm hủy hai lần).
 *   2) Lấy bản ghi hiện tại, đẩy trạng thái CHO_XAC_NHAN_HUY.
 *   3) Ghi lại mốc thời gian bắt đầu (cancelStart) để đo 5 giây.
 * Trả về    : true nếu bắt đầu thành công, false nếu không.
 */
bool TripManager::requestCancel() {
    if (tripHistory.empty() || getCurrentState() == CHO_XAC_NHAN_HUY) return false;
    Trip t = tripHistory.top();
    pushState(CHO_XAC_NHAN_HUY, t.driverID, t.tripID);
    cancelStart = chrono::steady_clock::now();
    return true;
}

/*
 * TripManager::cancelRemainingMs
 * Chức năng : Cho biết còn bao nhiêu mili giây trong cửa sổ hoàn tác hủy.
 * Cách làm  : Nếu không ở trạng thái CHO_XAC_NHAN_HUY thì trả về 0. Ngược lại
 *             tính elapsed = thời gian đã trôi từ cancelStart, rồi trả về
 *             max(0, CANCEL_WINDOW_MS - elapsed).
 * Trả về    : Số ms còn lại, nằm trong khoảng 0 đến 5000.
 */
int TripManager::cancelRemainingMs() const {
    if (getCurrentState() != CHO_XAC_NHAN_HUY) return 0;
    long long elapsed = chrono::duration_cast<chrono::milliseconds>(
        chrono::steady_clock::now() - cancelStart).count();
    return static_cast<int>(max<long long>(0, CANCEL_WINDOW_MS - elapsed));
}

/*
 * TripManager::undoCancel
 * Chức năng : Hoàn tác yêu cầu hủy trong cửa sổ 5 giây.
 * Các bước  :
 *   1) Trả về false nếu chuyến không ở CHO_XAC_NHAN_HUY hoặc đã hết thời
 *      gian (cancelRemainingMs() == 0).
 *   2) Pop trạng thái CHO_XAC_NHAN_HUY khỏi stack, nhờ đó chuyến quay lại
 *      trạng thái trước đó (TIM_XE hoặc NHAN_CUOC).
 *   3) In log "[LOG] Da hoan tac huy cuoc, tro ve: ..." kèm tên trạng thái.
 * Trả về    : true nếu hoàn tác thành công, false nếu không.
 */
bool TripManager::undoCancel() {
    if (getCurrentState() != CHO_XAC_NHAN_HUY || cancelRemainingMs() == 0) return false;
    tripHistory.pop();   // quay lại trạng thái trước đó (TIM_XE hoặc NHAN_CUOC)
    cout << "[LOG] Da hoan tac huy cuoc, tro ve: " << stateToString(getCurrentState()) << endl;
    return true;
}

/*
 * TripManager::finalizeCancelIfExpired
 * Chức năng : Chốt việc hủy chuyến nếu cửa sổ hoàn tác 5 giây đã hết.
 * Các bước  :
 *   1) Trả về false nếu chuyến không ở CHO_XAC_NHAN_HUY hoặc vẫn còn thời
 *      gian chờ (cancelRemainingMs() > 0).
 *   2) Lấy bản ghi hiện tại, đẩy trạng thái HUY_HOAN_TOAN.
 *   3) Xóa toàn bộ lịch sử bằng clearTripData.
 *   4) Gọi updateDriverStats(driverID, false): tăng số cuốc hủy của tài xế
 *      và đặt tài xế về trạng thái rảnh (RANH).
 * Trả về    : true nếu đã chốt hủy, false nếu chưa đến lúc.
 * Lưu ý     : Cần được gọi định kỳ bởi phần code bên ngoài (ví dụ vòng lặp
 *             chính hoặc bộ hẹn giờ), vì hàm không tự chạy nền.
 */
bool TripManager::finalizeCancelIfExpired() {
    if (getCurrentState() != CHO_XAC_NHAN_HUY || cancelRemainingMs() > 0) return false;
    Trip t = tripHistory.top();
    pushState(HUY_HOAN_TOAN, t.driverID, t.tripID);
    clearTripData();
    updateDriverStats(t.driverID, false);   // tăng cancelledTrips, đặt tài xế về RANH
    return true;
}