#pragma once
#include <string>
#include <stack>
#include <chrono>

/*
 * Enum State
 * ---------------------------------------------------------------
 * Các trạng thái có thể có của một chuyến đi trong quá trình đặt / hủy xe.
 */
enum State {
    TIM_XE,             // Đang tìm xe: khách đã đặt, chưa có tài xế nhận.
    NHAN_CUOC,          // Tài xế đã nhận cuốc.
    CHO_XAC_NHAN_HUY,   // Khách vừa bấm hủy, đang trong thời gian chờ (5 giây) và vẫn có thể hoàn tác.
    HUY_HOAN_TOAN       // Chuyến đã bị hủy hẳn, không thể hoàn tác.
};

/*
 * stateToString
 * Chức năng : Chuyển một giá trị State thành chuỗi tiếng Việt (không dấu)
 *             để in log / hiển thị.
 * Tham số   : state - trạng thái cần chuyển đổi.
 * Trả về    : Chuỗi mô tả trạng thái; "Khong xac dinh" nếu giá trị không
 *             nằm trong enum.
 */
std::string stateToString(State state);

/*
 * Struct Trip
 * ---------------------------------------------------------------
 * Mục đích: Một bản ghi (snapshot) trạng thái của chuyến đi tại một thời
 * điểm. Các Trip được xếp chồng trong stack lịch sử để có thể hoàn tác.
 */
struct Trip {
    State state;            // Trạng thái của chuyến tại thời điểm ghi nhận.
    std::string driverID;   // Mã tài xế liên quan đến chuyến.
    std::string tripID;     // Mã định danh của chuyến đi.
    long long timestamp;    // Thời điểm ghi nhận, tính bằng mili giây từ epoch.
};

/*
 * getCurrentTimestamp
 * Chức năng : Lấy thời gian hiện tại của hệ thống dưới dạng số mili giây kể
 *             từ mốc epoch (1/1/1970), dùng làm nhãn thời gian cho Trip.
 * Trả về    : Timestamp (long long, đơn vị mili giây).
 */
long long getCurrentTimestamp();

class Operation;

/*
 * Lớp TripManager
 * ---------------------------------------------------------------
 * Mục đích: Quản lý vòng đời trạng thái của MỘT chuyến đi, đặc biệt là
 * luồng hủy chuyến có cửa sổ hoàn tác 5 giây. Dùng stack (tripHistory) để
 * lưu lịch sử trạng thái: đỉnh stack là trạng thái hiện tại, và "hoàn tác"
 * đơn giản là pop trạng thái CHO_XAC_NHAN_HUY để quay về trạng thái trước.
 *
 * Có hai cách hủy chuyến:
 *   - cancelTripRequest(): bản chặn (blocking), tự đợi 5 giây trong một hàm.
 *   - requestCancel() + undoCancel() + finalizeCancelIfExpired(): bản không
 *     chặn, người gọi chủ động kiểm tra theo thời gian.
 */
class TripManager {
private:
    // Stack lịch sử trạng thái chuyến đi; top() là trạng thái hiện tại.
    std::stack<Trip> tripHistory; 
    // Con trỏ tới Operation để cập nhật thống kê tài xế (không sở hữu).
    // Có thể là nullptr, khi đó không cập nhật thống kê.
    Operation* operation;

    // Mốc thời gian lúc bắt đầu chờ hủy (đặt trong requestCancel).
    std::chrono::steady_clock::time_point cancelStart;  
    // Thời gian chờ hủy / cửa sổ hoàn tác: 5000 ms = 5 giây.
    static constexpr int CANCEL_WINDOW_MS = 5000;      

public:
    /*
     * TripManager (hàm khởi tạo)
     * Chức năng : Tạo bộ quản lý cho một chuyến đi mới và đặt trạng thái
     *             ban đầu là TIM_XE.
     * Tham số   : driverID  - mã tài xế gắn với chuyến.
     *             tripID    - mã chuyến đi.
     *             operation - con trỏ Operation dùng để cập nhật thống kê tài xế.
     */
    TripManager(const std::string& driverID, const std::string& tripID, Operation* operation);

    /*
     * pushState
     * Chức năng : Ghi nhận một trạng thái mới cho chuyến bằng cách tạo Trip
     *             (kèm timestamp hiện tại) và đẩy lên đỉnh stack lịch sử.
     * Tham số   : newState - trạng thái mới.
     *             driverID, tripID - mã tài xế và mã chuyến của bản ghi.
     */
    void pushState(State newState, const std::string& driverID, const std::string& tripID);

    /*
     * getCurrentState
     * Chức năng : Lấy trạng thái hiện tại của chuyến (phần tử ở đỉnh stack).
     * Trả về    : State ở đỉnh stack; nếu stack rỗng (lịch sử đã bị xóa sau
     *             khi hủy xong) thì trả về HUY_HOAN_TOAN.
     */
    State getCurrentState() const;

    /*
     * cancelTripRequest
     * Chức năng : Hủy chuyến theo kiểu CHẶN (blocking): chuyển sang trạng thái
     *             chờ xác nhận hủy, đợi đủ 5 giây rồi hủy hoàn toàn.
     * Lưu ý     : Hàm chặn luồng gọi trong 5 giây. Nhánh hoàn tác trong hàm
     *             hiện chưa dùng được (xem chi tiết ở stackundo.cpp). Muốn
     *             hủy có hoàn tác thật sự, dùng bộ ba hàm không chặn bên dưới.
     */
    void cancelTripRequest();

    /*
     * clearTripData
     * Chức năng : Xóa toàn bộ lịch sử trạng thái của chuyến (pop hết stack).
     */
    void clearTripData();

    /*
     * updateDriverStats
     * Chức năng : Cập nhật thống kê của tài xế sau khi chuyến kết thúc bằng
     *             cách gọi Operation::updateTripResult.
     * Tham số   : driverID   - mã tài xế cần cập nhật.
     *             completed  - true nếu chuyến hoàn thành, false nếu bị hủy.
     *             tripRating - điểm đánh giá (1 đến 5), chỉ dùng khi completed
     *                          = true; mặc định 0.0f.
     * Lưu ý     : Nếu operation là nullptr thì không làm gì.
     */
    void updateDriverStats(const std::string& driverID, bool completed, float tripRating = 0.0f);

    /*
     * requestCancel
     * Chức năng : Bắt đầu hủy chuyến theo kiểu KHÔNG chặn: đẩy trạng thái
     *             CHO_XAC_NHAN_HUY và bắt đầu đếm 5 giây.
     * Trả về    : true nếu bắt đầu thành công; false nếu chuyến không còn
     *             lịch sử hoặc đang ở trạng thái chờ hủy rồi.
     */
    bool requestCancel();
    
    /*
     * undoCancel
     * Chức năng : Hoàn tác yêu cầu hủy: bỏ trạng thái CHO_XAC_NHAN_HUY để
     *             quay về trạng thái trước đó (TIM_XE hoặc NHAN_CUOC).
     * Trả về    : true nếu hoàn tác thành công; false nếu chuyến không ở
     *             trạng thái chờ hủy hoặc đã hết 5 giây.
     */
    bool undoCancel();              

    /*
     * finalizeCancelIfExpired
     * Chức năng : Nếu thời gian chờ hủy đã hết, chốt việc hủy: chuyển sang
     *             HUY_HOAN_TOAN, xóa lịch sử và cập nhật thống kê tài xế
     *             (tính một cuốc bị hủy).
     * Trả về    : true nếu đã chốt hủy; false nếu chưa ở trạng thái chờ hủy
     *             hoặc vẫn còn thời gian chờ.
     * Lưu ý     : Người gọi cần gọi định kỳ (ví dụ trong vòng lặp chính).
     */
    bool finalizeCancelIfExpired();    
    
    /*
     * cancelRemainingMs
     * Chức năng : Cho biết còn bao nhiêu mili giây trong cửa sổ hoàn tác hủy.
     * Trả về    : Số ms còn lại (0 đến 5000); 0 nếu không ở trạng thái chờ hủy
     *             hoặc đã hết thời gian.
     */
    int cancelRemainingMs() const;     
};