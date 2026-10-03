#include "pch.h"
#include "HashTable.hpp"
#include "operation.hpp"
#include "Trie.hpp"
#include "stackundo.hpp"
#include "queue.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <mutex>
#include <map>
#include <memory>
#include <thread>
#include <chrono>
#include <unordered_set>
#include <climits>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using json = nlohmann::json;

// Khai báo trước (forward declaration) hai hàm đọc/ghi database.
// Phần định nghĩa nằm bên dưới, sau hàm jsonToDriver().
vector<Driver> GetDatabase();
void PushDatabase(vector<Driver> drivers);

// ===== BIẾN TOÀN CỤC =====
// table         : bảng băm chứa toàn bộ tài xế trong bộ nhớ (khóa = id tài xế).
// operation     : đối tượng nghiệp vụ thao tác trên table (cập nhật thông tin, ghi nhận cuốc, cảnh báo hủy cuốc...).
// processor     : bộ xử lý toán học + lọc tài xế (khoảng cách, góc quay, điểm ưu tiên, lọc theo bán kính).
// databaseMutex : khóa bảo vệ table và trips khỏi truy cập đồng thời giữa các luồng (mỗi request chạy trên 1 luồng).
HashTable table;
Operation operation(table);
DriverMathAndFilterProcessor processor;
mutex databaseMutex;

// autoComplete : cây Trie dùng để gợi ý địa điểm theo tiền tố.
// locationList : danh sách gốc các địa điểm đã thêm (dùng để dựng lại Trie khi cần, vì Trie không có hàm xóa).
// trieMutex    : khóa bảo vệ autoComplete, locationList và vị trí người dùng (userPosX/userPosY).
AutoComplete autoComplete;
vector<Location> locationList; 
mutex trieMutex;                

/**
 * @struct TripSession
 * Một "phiên chuyến đi" đang hoạt động trong bộ nhớ.
 *  - driverId : id tài xế phụ trách chuyến.
 *  - manager  : đối tượng TripManager quản lý trạng thái và lịch sử (stack undo) của chuyến.
 * Tất cả phiên được lưu trong map `trips` với khóa là tripId.
 */
struct TripSession {
    string driverId;
    unique_ptr<TripManager> manager;
};
map<string, TripSession> trips;

/**
 * @brief  Chuyển một đối tượng Driver thành JSON (dùng để trả về client và để ghi vào database.json).
 * @param  driver Tài xế cần chuyển đổi.
 * @return json object gồm 3 nhóm:
 *   - Thông tin cơ bản: id, name, phone, licensePlate, vehicleType.
 *   - "status": x, y, directionX, directionY, currentStatus, freeTime, currentTripID.
 *   - "performance": rating, completedTrips, acceptedTrips, acceptanceRate,
 *     offeredTrips, cancelledTrips, cancellationWarned, cancellationThreshold.
 * @note   vehicleType và currentStatus là enum nên được lưu dưới dạng số nguyên.
 *         Khóa JSON "directionX/directionY" tương ứng với trường diractionX/diractionY trong struct
 *         (tên trường trong struct bị viết sai chính tả "diraction", được giữ nguyên để không phá code cũ).
 *         Hàm ngược lại là jsonToDriver().
 */
static json driverToJson(Driver& driver) {
    json result = {
        {"id", driver.info.getId()},
        {"name", driver.info.getName()},
        {"phone", driver.info.getPhone()},
        {"licensePlate", driver.info.getLicensePlate()},
        {"vehicleType", static_cast<int>(driver.info.vehicleType)},
        {"status", {
            {"x", driver.status.x},
            {"y", driver.status.y},
            {"directionX", driver.status.diractionX},
            {"directionY", driver.status.diractionY},
            {"currentStatus", static_cast<int>(driver.status.currentStatus)},
            {"freeTime", driver.status.freeTime},
            {"currentTripID", driver.status.currentTripID}
        }},
        {"performance", {
            {"rating", driver.performance.rating},
            {"completedTrips", driver.performance.completedTrips},
            {"acceptedTrips", driver.performance.acceptedTrips},
            {"acceptanceRate", driver.performance.acceptanceRate},
            {"offeredTrips", driver.performance.offeredTrips},
            {"cancelledTrips", driver.performance.cancelledTrips},
            {"cancellationWarned", driver.performance.cancellationWarned},
            {"cancellationThreshold", driver.performance.cancellationThreshold}
        }}
    };
    return result;
}

/**
 * @brief  Chuyển một JSON object thành đối tượng Driver (ngược lại với driverToJson).
 * @param  j JSON object mô tả một tài xế (thường là một phần tử trong database.json).
 * @return Driver đã được điền dữ liệu.
 * @note   Các trường bị thiếu sẽ nhận giá trị mặc định:
 *         - Thông tin: id/name/phone/licensePlate = "", vehicleType = 0.
 *         - status: x, y, directionX, directionY = 0; currentStatus = RANH; freeTime = 0; currentTripID = "".
 *         - performance: rating = 5.0; completedTrips, acceptedTrips, offeredTrips, cancelledTrips = 0;
 *           acceptanceRate = 0; cancellationWarned = false; cancellationThreshold = 30.0.
 *         Khối "status" và "performance" là tùy chọn: nếu thiếu hoặc không phải object thì dùng mặc định.
 *         Việc gán mặc định cho status/performance được làm thủ công vì constructor mặc định Driver()
 *         không khởi tạo các trường này.
 */
static Driver jsonToDriver(const json& j) {
    Driver driver;

    driver.info.setId(j.value("id", ""));
    driver.info.setName(j.value("name", ""));
    driver.info.setPhone(j.value("phone", ""));
    driver.info.setLicensePlate(j.value("licensePlate", ""));
    driver.info.vehicleType = static_cast<VehicleType>(j.value("vehicleType", 0));

    // Khởi tạo giá trị mặc định (Driver() = default không gán các trường này)
    driver.status.x = 0.0;
    driver.status.y = 0.0;
    driver.status.diractionX = 0.0;
    driver.status.diractionY = 0.0;
    driver.status.currentStatus = RANH;
    driver.status.freeTime = 0;
    driver.status.currentTripID = "";

    if (j.contains("status") && j["status"].is_object()) {
        const json& s = j["status"];
        driver.status.x = s.value("x", 0.0);
        driver.status.y = s.value("y", 0.0);
        driver.status.diractionX = s.value("directionX", 0.0);
        driver.status.diractionY = s.value("directionY", 0.0);
        driver.status.currentStatus = static_cast<DriverStatusType>(s.value("currentStatus", 0));
        driver.status.freeTime = s.value("freeTime", 0);
        driver.status.currentTripID = s.value("currentTripID", "");
    }

    driver.performance.rating = 5.0f;
    driver.performance.completedTrips = 0;
    driver.performance.acceptedTrips = 0;
    driver.performance.acceptanceRate = 0.0f;

    if (j.contains("performance") && j["performance"].is_object()) {
        const json& p = j["performance"];
        driver.performance.rating = p.value("rating", 5.0f);
        driver.performance.completedTrips = p.value("completedTrips", 0);
        driver.performance.acceptedTrips = p.value("acceptedTrips", 0);
        driver.performance.acceptanceRate = p.value("acceptanceRate", 0.0f);
        driver.performance.offeredTrips = p.value("offeredTrips", 0);
        driver.performance.cancelledTrips = p.value("cancelledTrips", 0);
        driver.performance.cancellationWarned = p.value("cancellationWarned", false);
        driver.performance.cancellationThreshold = p.value("cancellationThreshold", 30.0);
    }

    return driver;
}

/**
 * @brief  Đọc danh sách tài xế từ file database.json (nằm trong thư mục chạy chương trình).
 * @return vector<Driver> chứa các tài xế đọc được. Trả về vector RỖNG nếu:
 *         - không mở được file (có in lỗi ra cerr), hoặc
 *         - nội dung không phải JSON hợp lệ, hoặc gốc JSON không phải mảng.
 * @note   Phần tử nào trong mảng không phải object sẽ bị bỏ qua.
 *         json::parse(..., nullptr, false) không ném exception mà trả về giá trị "discarded" khi lỗi.
 *         Được gọi một lần ở đầu main() để nạp dữ liệu vào table.
 */
vector<Driver> GetDatabase(){
    vector<Driver> drivers;

    ifstream inFile("database.json");
    if(!inFile.is_open()){
        cerr << "Không thể mở file database.json\n";
        return drivers;
    }
    
    json data = json::parse(inFile, nullptr, false);
    inFile.close();

    if(data.is_discarded() || !data.is_array()){
        return drivers;
    }

    for(const auto& item : data){
        if(!item.is_object()) continue;
        drivers.push_back(jsonToDriver(item));
    }

    return drivers;
}

/**
 * @brief  Ghi toàn bộ danh sách tài xế xuống file database.json.
 * @param  drivers Danh sách tài xế cần lưu (truyền theo giá trị, tức là một bản sao).
 * @note   Ghi ĐÈ hoàn toàn nội dung cũ của file nên phải truyền đầy đủ tất cả tài xế, không chỉ phần thay đổi.
 *         Dữ liệu được định dạng thụt 4 khoảng trắng (dump(4)) để dễ đọc.
 *         Nếu không mở được file thì chỉ in lỗi ra cerr và thoát, không ném exception.
 */
void PushDatabase(vector<Driver> drivers){
    ofstream outFile("database.json");
    if(!outFile.is_open()){
        cerr << "Không thể mở file database.json!\n";
        return;
    }
    
    json data = json::array();
    for(auto& driver : drivers){
        data.push_back(driverToJson(driver));
    }

    outFile << data.dump(4);   outFile.close();
}

//=== HÀM TIỆN ÍCH ===

/**
 * @brief Lưu toàn bộ tài xế đang có trong bảng băm `table` xuống database.json.
 * @note  Bên trong gọi PushDatabase(table.getAllDrivers()).
 *        Được gọi sau MỖI thay đổi dữ liệu tài xế (thêm, xóa, cập nhật...) để file luôn đồng bộ với bộ nhớ.
 *        Người gọi phải đang giữ databaseMutex.
 */
static void SaveTable(){
    PushDatabase(table.getAllDrivers());
}

/**
 * @brief Gửi phản hồi JSON về client.
 * @param res    Đối tượng phản hồi HTTP của httplib.
 * @param status Mã trạng thái HTTP (200, 201, 400, 404...).
 * @param body   Nội dung JSON cần gửi.
 * @note  Content-Type được đặt là "application/json; charset=utf-8" để hiển thị đúng tiếng Việt.
 */
static void SendJson(httplib::Response& res, int status, const json& body){
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

/**
 * @brief Gửi phản hồi lỗi về client dưới dạng {"error": "<msg>"}.
 * @param res    Đối tượng phản hồi HTTP.
 * @param status Mã lỗi HTTP (400 dữ liệu sai, 404 không tìm thấy, 409 xung đột...).
 * @param msg    Thông báo lỗi cho người dùng/lập trình viên phía client.
 * @note  Là lớp bọc (wrapper) mỏng quanh SendJson().
 */
static void SendError(httplib::Response& res, int status, const string& msg){
    SendJson(res, status, {{"error", msg}});
}

/**
 * @brief  Phân tích (parse) body của request thành JSON object và kiểm tra tính hợp lệ.
 * @param  req  Request HTTP (đọc req.body).
 * @param  res  Phản hồi HTTP, dùng để gửi lỗi 400 khi body sai.
 * @param  body [OUT] JSON object nhận kết quả khi parse thành công.
 * @return true  nếu body là JSON object hợp lệ.
 *         false nếu body không phải JSON hợp lệ hoặc không phải object (đã tự gửi lỗi 400).
 * @note   Cách dùng chuẩn trong các route: `if (!parseBody(req, res, body)) return;`
 *         Khi trả về false, route phải thoát ngay vì phản hồi lỗi đã được gửi.
 */
static bool parseBody(const httplib::Request& req, httplib::Response& res, json& body){
    body = json::parse(req.body, nullptr, false);
    if(body.is_discarded() || !body.is_object()){
        SendError(res, 400, "Body phải là JSON object hợp lệ");
        return false;
    }
    return true;
}

/**
 * @brief  Đọc một tham số số thực từ query string, ví dụ ?x1=1.5.
 * @param  req  Request HTTP.
 * @param  res  Phản hồi HTTP, dùng để gửi lỗi 400.
 * @param  name Tên tham số cần đọc.
 * @param  out  [OUT] Giá trị số thực sau khi chuyển đổi.
 * @return true  nếu tham số tồn tại và là số hợp lệ.
 *         false nếu thiếu tham số hoặc không phải số (đã tự gửi lỗi 400).
 * @note   Dùng stod và kiểm tra vị trí kết thúc, nên chuỗi có ký tự thừa như "1.5abc" cũng bị từ chối.
 *         Cách dùng: `if (!getParamDouble(req, res, "x1", x1)) return;`
 */
static bool getParamDouble(const httplib::Request& req, httplib::Response& res,
                           const char* name, double& out) {
    if (!req.has_param(name)) {
        SendError(res, 400, string("Thieu tham so: ") + name);
        return false;
    }
    try {
        string s = req.get_param_value(name);
        size_t pos = 0;
        out = stod(s, &pos);
        if (pos != s.size()) throw invalid_argument("extra chars");
    } catch (...) {
        SendError(res, 400, string("Tham so phai la so: ") + name);
        return false;
    }
    return true;
}

/**
 * @brief  Chuyển một phiên chuyến đi (TripSession) thành JSON.
 * @param  tripId Mã chuyến.
 * @param  s      Phiên chuyến cần chuyển đổi.
 * @return json gồm: tripId, driverId, state (số nguyên), stateName (tên trạng thái dạng chữ,
 *         lấy từ stateToString). Nếu chuyến đang ở trạng thái CHO_XAC_NHAN_HUY thì thêm
 *         cancelRemainingMs: số mili-giây còn lại trong cửa sổ 5 giây để hoàn tác việc hủy.
 */
static json tripToJson(const string& tripId, TripSession& s) {
    State st = s.manager->getCurrentState();
    json j = {
        {"tripId", tripId},
        {"driverId", s.driverId},
        {"state", static_cast<int>(st)},
        {"stateName", stateToString(st)}
    };
    if (st == CHO_XAC_NHAN_HUY) j["cancelRemainingMs"] = s.manager->cancelRemainingMs();
    return j;
}

/**
 * @brief  Chuyển một địa điểm (Location) thành JSON.
 * @param  l Địa điểm cần chuyển đổi.
 * @return json gồm: name, posX, posY, population (dân số).
 */
static json locationToJson(const Location& l) {
    return {{"name", l.name}, {"posX", l.posX}, {"posY", l.posY}, {"population", l.population}};
}

/**
 * @brief  Kiểm tra và chuyển một JSON thành Location.
 * @param  j   JSON cần chuyển đổi.
 * @param  out [OUT] Location nhận kết quả. Chỉ được gán khi dữ liệu hợp lệ hoàn toàn.
 * @param  err [OUT] Thông báo lỗi khi trả về false.
 * @return true nếu hợp lệ, false nếu có lỗi (nguyên nhân nằm trong err).
 * @note   Điều kiện hợp lệ:
 *         - j phải là object.
 *         - "name" là chuỗi và KHÔNG rỗng sau khi chuẩn hóa bằng toLower().
 *         - "posX", "posY" là số.
 *         - "population" là tùy chọn, mặc định 0; nếu có phải là số nguyên trong khoảng 0..INT_MAX.
 */
static bool jsonToLocation(const json& j, Location& out, string& err) {
    if (!j.is_object()) { err = "Moi phan tu phai la JSON object"; return false; }
    if (!j.contains("name") || !j["name"].is_string()) { err = "Thieu name (string)"; return false; }
    if (!j.contains("posX") || !j["posX"].is_number() ||
        !j.contains("posY") || !j["posY"].is_number()) { err = "posX, posY phai la so"; return false; }

    long long population = 0;
    if (j.contains("population")) {
        if (!j["population"].is_number_integer()) { err = "population phai la so nguyen"; return false; }
        population = j["population"].get<long long>();
        if (population < 0 || population > INT_MAX) { err = "population phai tu 0 den 2147483647"; return false; }
    }

    string name = j["name"].get<string>();
    if (toLower(name).empty()) { err = "name khong hop le (rong sau khi chuan hoa)"; return false; }

    out.name = name;
    out.posX = j["posX"].get<double>();
    out.posY = j["posY"].get<double>();
    out.population = static_cast<int>(population);
    return true;
}

/**
 * @brief  Tìm vị trí của một địa điểm trong locationList theo tên.
 * @param  name Tên địa điểm cần tìm.
 * @return Chỉ số (index) trong locationList, hoặc -1 nếu không tìm thấy.
 * @note   So sánh theo tên ĐÃ CHUẨN HÓA bằng toLower() (bỏ dấu, chữ thường), nên "Hà Nội" và "ha noi"
 *         được xem là cùng một địa điểm (vì chúng nằm trên cùng một nút của Trie).
 *         Hàm này dùng để phát hiện trùng khi thêm và để tìm khi xóa. Độ phức tạp O(n).
 */
static int findLocationIndex(const string& name) {
    string key = toLower(name);
    for (size_t i = 0; i < locationList.size(); i++)
        if (toLower(locationList[i].name) == key) return static_cast<int>(i);
    return -1;
}

/**
 * @brief Dựng lại toàn bộ cây Trie từ locationList.
 * @note  Cần gọi khi: (1) xóa một địa điểm (Trie không có hàm xóa), (2) đổi vị trí người dùng
 *        userPosX/userPosY (vì điểm gợi ý được tính lúc insert, nên phải insert lại để xếp hạng mới có hiệu lực).
 *        Cách làm: autoComplete.Clear() rồi InsertLocation() lại từng địa điểm.
 *        Người gọi phải đang giữ trieMutex.
 */
static void rebuildTrie() {
    autoComplete.Clear();
    for (auto& l : locationList) autoComplete.InsertLocation(l);
}

/**
 * @brief  Tạo JSON mô tả toàn bộ nội dung cây Trie (cũng chính là nội dung file suggestions.json).
 * @return json gồm:
 *         - userPosition  : vị trí người dùng hiện tại {x, y}.
 *         - maxSuggest    : số gợi ý tối đa mỗi nút.
 *         - locationCount : tổng số địa điểm.
 *         - nodes         : với mỗi tiền tố (prefix) là một mục {endOfWord, suggest, fixedSuggest}.
 * @note   Người gọi phải đang giữ trieMutex.
 */
static json buildSuggestionsJson() {
    json nodes = json::object();
    // Duyệt từng nút của Trie. `prefix` là chuỗi tiền tố dẫn tới nút đó.
    // suggest      <- node.topSuggestLocation       (danh sách gợi ý hàng đầu của nút)
    // fixedSuggest <- node.fixedTopSuggestLocation  (danh sách gợi ý tương ứng cho tên đã chuẩn hóa; xem Trie.hpp)
    autoComplete.ForEachNode([&](const string& prefix, const TrieNode& node) {
        json suggest = json::array(), fixedSuggest = json::array();
        for (auto& l : node.topSuggestLocation)      suggest.push_back(locationToJson(l));
        for (auto& l : node.fixedTopSuggestLocation) fixedSuggest.push_back(locationToJson(l));
        nodes[prefix] = {{"endOfWord", node.endOfWord},
                         {"suggest", suggest},
                         {"fixedSuggest", fixedSuggest}};
    });
    return {{"userPosition", {{"x", userPosX}, {"y", userPosY}}},
            {"maxSuggest", autoComplete.GetMaxSuggest()},
            {"locationCount", locationList.size()},
            {"nodes", nodes}};
}

/**
 * @brief Ghi dữ liệu địa điểm xuống đĩa, gồm hai file:
 *        - locations.json   : vị trí người dùng + danh sách địa điểm gốc (dùng để nạp lại khi khởi động).
 *        - suggestions.json : ảnh chụp cây Trie hiện tại (kết quả của buildSuggestionsJson()).
 * @note  Phải gọi khi đang giữ trieMutex.
 *        Nếu không mở được file thì chỉ in lỗi ra cerr, không ném exception.
 */
static void persistTrie() {
    json arr = json::array();
    for (auto& l : locationList) arr.push_back(locationToJson(l));
    json data = {{"userPosition", {{"x", userPosX}, {"y", userPosY}}}, {"locations", arr}};

    ofstream f1("locations.json");
    if (f1.is_open()) f1 << data.dump(4); else cerr << "Khong the mo locations.json\n";

    ofstream f2("suggestions.json");
    if (f2.is_open()) f2 << buildSuggestionsJson().dump(4); else cerr << "Khong the mo suggestions.json\n";
}

/**
 * @brief Nạp dữ liệu địa điểm từ locations.json khi khởi động chương trình.
 * @note  Các bước: đọc file -> đọc userPosition (userPosX/userPosY) -> nạp locationList
 *        (bỏ qua mục không hợp lệ hoặc trùng tên) -> rebuildTrie().
 *        Nếu file không tồn tại hoặc sai định dạng thì thoát sớm, danh sách địa điểm giữ nguyên.
 *        Chỉ được gọi trong main() trước khi server chạy nên không cần khóa trieMutex.
 */
static void loadLocations() {
    ifstream in("locations.json");
    if (!in.is_open()) return;
    json data = json::parse(in, nullptr, false);
    if (data.is_discarded() || !data.is_object()) return;

    if (data.contains("userPosition") && data["userPosition"].is_object()) {
        userPosX = data["userPosition"].value("x", 0.0);
        userPosY = data["userPosition"].value("y", 0.0);
    }
    locationList.clear();
    if (data.contains("locations") && data["locations"].is_array()) {
        for (const auto& item : data["locations"]) {
            Location l; string err;
            if (jsonToLocation(item, l, err) && findLocationIndex(l.name) < 0)
                locationList.push_back(l);
        }
    }
    rebuildTrie();
}


/**
 * @brief  Điểm vào chương trình: khởi tạo dữ liệu, đăng ký toàn bộ route HTTP và chạy server.
 * @return 0 khi kết thúc (thực tế server.listen() chặn luồng chính nên hàm gần như không thoát).
 * @note   Trình tự: (1) đặt console UTF-8 -> (2) nạp tài xế từ database.json vào table
 *         -> (3) nạp địa điểm (loadLocations) -> (4) tạo server và bật CORS
 *         -> (5) đăng ký các nhóm route -> (6) chạy luồng nền hủy chuyến -> (7) listen cổng 8080.
 *         Các nhóm route:
 *         - Quản lý tài xế : /drivers, /drivers/<id>, /location, /status, /free-time
 *         - Operation      : /info, /offer, /trip-result, /free-drivers, /drivers/warn-cancellation
 *         - Toán học/lọc   : /math/*, /drivers/<id>/score, /nearby-drivers, /dispatch/ranking
 *         - Chuyến đi      : /timestamp, /trips/*  (đếm ngược 5 giây khi hủy, có thể hoàn tác)
 *         - Địa điểm (Trie): /locations/*, /user-position
 *         Mọi route đều lock databaseMutex (hoặc trieMutex cho nhóm Trie) khi đụng đến dữ liệu dùng chung.
 */
int main(){
    // Đặt console sang UTF-8 (code page 65001) để in tiếng Việt có dấu đúng.
    // Lưu ý: SetConsoleOutputCP/SetConsoleCP chỉ có trên Windows (cần <windows.h>).
    SetConsoleOutputCP(65001); 
    SetConsoleCP(65001);

    //Nạp dữ liệu từ file cũ vào table
    for(auto& driver : GetDatabase()){
        table.addDriver(driver.info.getId(), driver);
    }

    loadLocations();

    // Tạo HTTP server (thư viện cpp-httplib).
    // set_mount_point("/", "./public"): phục vụ file tĩnh (giao diện web) trong thư mục ./public tại đường dẫn gốc "/".
    httplib::Server server;
    server.set_mount_point("/", "./public");

    /**
     * [Lambda] Pre-routing handler: chạy TRƯỚC mọi route.
     * Chức năng : bật CORS để trình duyệt từ origin khác (ví dụ trang web chạy chỗ khác) gọi được API.
     *   - Thêm các header Access-Control-Allow-Origin/Headers/Methods cho mọi phản hồi.
     *   - Nếu là request OPTIONS (preflight của trình duyệt): trả 200 và dừng ở đây (Handled).
     *   - Còn lại: trả Unhandled để request đi tiếp tới route tương ứng.
     */
    server.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");

        if (req.method == "OPTIONS") {
            res.status = 200;
            return httplib::Server::HandlerResponse::Handled;
        }

        return httplib::Server::HandlerResponse::Unhandled;
    });

    /**
     * [ROUTE] POST /drivers
     * Chức năng : Thêm tài xế mới và tự động lưu vào database.json.
     * Body      : {"id","name","phone","licensePlate"} (chuỗi, bắt buộc); "vehicleType" (số nguyên, tùy chọn, mặc định 0).
     *             vehicleType: 0 = XE_MAY, 1 = O_TO.
     * Kiểm tra  : body là JSON object; id và name không rỗng; vehicleType hợp lệ; id chưa tồn tại.
     * Kết quả   : 201 + thông tin tài xế | 400 dữ liệu sai | 409 id đã tồn tại.
     * Hàm dùng  : table.checkID, table.addDriver, SaveTable, driverToJson.
     */
    server.Post("/drivers", [](const httplib::Request& req, httplib::Response& res){
        //json::parse() chuyển đổi từ văn bản thành json
        json body = json::parse(req.body, nullptr, false);

        //Kiểm tra json có hợp lể không
        if(body.is_discarded() || !body.is_object()){
            return SendError(res, 400, "Body phai la JSON object hop le");
        }

        string id, name, phone, plate;
        int vehicleType;
        try{
            id          = body.at("id").get<string>();
            name        = body.at("name").get<string>();
            phone       = body.at("phone").get<string>();
            plate       = body.at("licensePlate").get<string>();
            vehicleType = body.value("vehicleType", 0);
        }catch(const json::exception&){
            return SendError(res, 400, "Thiếu hoặc sai kiểu trường");
        }

        if(id.empty() || name.empty()){
            return SendError(res, 400, "id và name không được rỗng");
        }

        if(vehicleType != XE_MAY && vehicleType != O_TO){
            return SendError(res, 400, "vehicleType chỉ nhận 0 (XE_MAY) hoặc 1 (O_TO)");
        }

        //bảo vệ dữ liệu khỏi đa luồng
        lock_guard<mutex> lock(databaseMutex);

        if(table.checkID(id)){
            return SendError(res, 409, "Id đã tồn tại");
        }

        Driver driver(id, name, phone, plate, static_cast<VehicleType>(vehicleType));
        table.addDriver(id, driver);
        SaveTable();

        SendJson(res, 201, driverToJson(driver));
    });

    /**
     * [ROUTE] GET /drivers
     * Chức năng : Lấy danh sách tất cả tài xế.
     * Kết quả   : 200 + mảng JSON các tài xế (mảng rỗng nếu chưa có ai).
     * Hàm dùng  : table.getAllDrivers, driverToJson.
     */
    server.Get("/drivers", [](const httplib::Request&, httplib::Response& res){
        lock_guard<mutex> lock(databaseMutex);
        json arr = json::array();
        for(auto& d : table.getAllDrivers()){
            arr.push_back(driverToJson(d));
        }
        SendJson(res, 200, arr);
    });

    /**
     * [ROUTE] GET /drivers/<id>
     * Chức năng : Lấy thông tin một tài xế theo id (id được lấy từ URL bằng regex, trong req.matches[1]).
     * Kết quả   : 200 + thông tin tài xế | 404 không tìm thấy.
     * Hàm dùng  : table.findDriver, driverToJson.
     */
    server.Get(R"(/drivers/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        Driver* d = table.findDriver(id);
        if (!d) return SendError(res, 404, "Khong tim thay tai xe");
        SendJson(res, 200, driverToJson(*d));
    });

    /**
     * [ROUTE] DELETE /drivers/<id>
     * Chức năng : Xóa tài xế theo id và tự động lưu vào database.json.
     * Kết quả   : 200 + {"deleted": id} | 404 không tìm thấy.
     * Hàm dùng  : table.checkID, table.deleteDriver, SaveTable.
     */
    server.Delete(R"(/drivers/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(id)) return SendError(res, 404, "Khong tim thay tai xe");
        table.deleteDriver(id);
        SaveTable();
        SendJson(res, 200, {{"deleted", id}});
    });

    /**
     * [ROUTE] PUT /drivers/<id>/location
     * Chức năng : Cập nhật tọa độ hiện tại (x, y) của tài xế.
     * Body      : {"x": 1.5, "y": 2.5} (cả hai là số thực, bắt buộc).
     * Kết quả   : 200 + thông tin tài xế sau cập nhật | 400 thiếu/sai kiểu x, y | 404 không tìm thấy tài xế.
     * Hàm dùng  : table.updateLocation, SaveTable.
     */
    server.Put(R"(/drivers/([^/]+)/location)", [](const httplib::Request& req, httplib::Response& res){
        string id = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        double x, y;
        try {
            x = body.at("x").get<double>();
            y = body.at("y").get<double>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can hai truong so: x, y");
        }

        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(id)) return SendError(res, 404, "Khong tim thay tai xe");
        table.updateLocation(id, x, y);
        SaveTable();
        SendJson(res, 200, driverToJson(*table.findDriver(id)));
    });

    /**
     * [ROUTE] PUT /drivers/<id>/status
     * Chức năng : Đổi trạng thái hoạt động của tài xế.
     * Body      : {"status": n} (số nguyên): 0 = RANH (rảnh), 1 = CO_KHACH (đang có khách), 2 = NGUNG_CHAY (ngừng chạy).
     * Kết quả   : 200 + thông tin tài xế | 400 status sai kiểu hoặc ngoài 0..2 | 404 không tìm thấy tài xế.
     * Hàm dùng  : table.updateStatus, SaveTable.
     */
    server.Put(R"(/drivers/([^/]+)/status)", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        int status;
        try {
            status = body.at("status").get<int>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can truong so nguyen: status");
        }
        if (status < RANH || status > NGUNG_CHAY) {
            return SendError(res, 400, "status chi nhan 0 (RANH), 1 (CO_KHACH), 2 (NGUNG_CHAY)");
        }

        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(id)) return SendError(res, 404, "Khong tim thay tai xe");
        table.updateStatus(id, static_cast<DriverStatusType>(status));
        SaveTable();
        SendJson(res, 200, driverToJson(*table.findDriver(id)));
    });

    /**
     * [ROUTE] PUT /drivers/<id>/free-time
     * Chức năng : Cập nhật trường freeTime (thời gian rảnh) của tài xế.
     * Body      : {"freeTime": 120} (số nguyên, không được âm).
     * Kết quả   : 200 + thông tin tài xế | 400 sai kiểu hoặc âm | 404 không tìm thấy tài xế.
     * Hàm dùng  : table.updateFreeTime, SaveTable.
     */
    server.Put(R"(/drivers/([^/]+)/free-time)", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        int freeTime;
        try {
            freeTime = body.at("freeTime").get<int>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can truong so nguyen: freeTime");
        }
        if (freeTime < 0) return SendError(res, 400, "freeTime khong duoc am");

        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(id)) return SendError(res, 404, "Khong tim thay tai xe");
        table.updateFreeTime(id, freeTime);
        SaveTable();
        SendJson(res, 200, driverToJson(*table.findDriver(id)));
    });

    // ===== Route cho Operation =====

    /**
     * [ROUTE] PUT /drivers/<id>/info
     * Chức năng : Cập nhật thông tin cá nhân của tài xế. Cập nhật từng phần: trường nào KHÔNG gửi lên thì giữ giá trị cũ.
     * Body      : {"name","phone","licensePlate"} (chuỗi) và "vehicleType" (0 = XE_MAY, 1 = O_TO); tất cả đều tùy chọn.
     * Kiểm tra  : name không được rỗng; vehicleType hợp lệ; sai kiểu dữ liệu -> 400.
     * Kết quả   : 200 + thông tin tài xế | 400 dữ liệu sai | 404 không tìm thấy tài xế.
     * Hàm dùng  : operation.updateDriverInfo, SaveTable.
     */
    server.Put(R"(/drivers/([^/]+)/info)", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        lock_guard<mutex> lock(databaseMutex);
        Driver* d = table.findDriver(id);
        if (!d) return SendError(res, 404, "Khong tim thay tai xe");

        string name, phone, plate;
        int vehicleType;
        try {
            name        = body.value("name", d->info.getName());
            phone       = body.value("phone", d->info.getPhone());
            plate       = body.value("licensePlate", d->info.getLicensePlate());
            vehicleType = body.value("vehicleType", static_cast<int>(d->info.vehicleType));
        } catch (const json::exception&) {
            return SendError(res, 400, "name, phone, licensePlate phai la string; vehicleType phai la so nguyen");
        }
        if (name.empty()) return SendError(res, 400, "name khong duoc rong");
        if (vehicleType != XE_MAY && vehicleType != O_TO) {
            return SendError(res, 400, "vehicleType chi nhan 0 (XE_MAY) hoac 1 (O_TO)");
        }

        operation.updateDriverInfo(id, name, phone, plate, static_cast<VehicleType>(vehicleType));
        SaveTable();
        SendJson(res, 200, driverToJson(*table.findDriver(id)));
    });

    /**
     * [ROUTE] POST /drivers/<id>/offer
     * Chức năng : Ghi nhận việc tài xế được mời một cuốc và tài xế có nhận hay không
     *             (cập nhật số cuốc được mời/đã nhận và tỷ lệ nhận cuốc trong performance).
     * Body      : {"accepted": true} hoặc {"accepted": false} (boolean, bắt buộc).
     * Kết quả   : 200 + thông tin tài xế | 400 thiếu/sai kiểu accepted | 404 không tìm thấy tài xế.
     * Hàm dùng  : operation.recordTripOffer, SaveTable.
     */
    server.Post(R"(/drivers/([^/]+)/offer)", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        bool accepted;
        try {
            accepted = body.at("accepted").get<bool>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can truong boolean: accepted");
        }

        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(id)) return SendError(res, 404, "Khong tim thay tai xe");
        operation.recordTripOffer(id, accepted);
        SaveTable();
        SendJson(res, 200, driverToJson(*table.findDriver(id)));
    });

    /**
     * [ROUTE] POST /drivers/<id>/trip-result
     * Chức năng : Ghi nhận kết quả một cuốc của tài xế: hoàn thành (kèm đánh giá sao) hoặc bị hủy.
     * Body      : - Hoàn thành: {"completed": true, "rating": 4.5}  (rating bắt buộc, từ 1 đến 5)
     *             - Hủy cuốc  : {"completed": false}
     * Kết quả   : 200 + thông tin tài xế | 400 dữ liệu sai | 404 không tìm thấy tài xế.
     *             Trường hợp đặc biệt: nếu tài xế hủy cuốc vượt ngưỡng sau khi đã bị cảnh báo thì bị XÓA khỏi hệ thống,
     *             khi đó trả 200 + {"driverRemoved": true, "id": ..., "reason": ...}.
     * Hàm dùng  : operation.updateTripResult, SaveTable, table.findDriver.
     */
    server.Post(R"(/drivers/([^/]+)/trip-result)", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        bool completed;
        float rating = 0.0f;
        try {
            completed = body.at("completed").get<bool>();
            if (completed) rating = body.at("rating").get<float>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can truong boolean: completed; neu completed = true thi can them rating (so)");
        }
        if (completed && (rating < 1.0f || rating > 5.0f)) {
            return SendError(res, 400, "rating phai tu 1 den 5");
        }

        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(id)) return SendError(res, 404, "Khong tim thay tai xe");

        operation.updateTripResult(id, completed, rating);
        SaveTable();

        // updateTripResult co the xoa tai xe neu huy cuoc qua nhieu sau khi da bi canh bao
        Driver* d = table.findDriver(id);
        if (!d) {
            return SendJson(res, 200, {{"driverRemoved", true}, {"id", id},
                                       {"reason", "Huy cuoc vuot nguong sau khi bi canh bao"}});
        }
        SendJson(res, 200, driverToJson(*d));
    });

    /**
     * [ROUTE] GET /free-drivers
     * Chức năng : Lấy danh sách id các tài xế đang RẢNH.
     * Query     : - Không có tham số      -> tất cả tài xế rảnh.
     *             - ?vehicleType=0 hoặc 1 -> chỉ tài xế rảnh thuộc loại xe đó (0 = XE_MAY, 1 = O_TO).
     * Kết quả   : 200 + {"count": n, "driverIds": [...]} | 400 vehicleType không phải số nguyên hoặc không hợp lệ.
     * Hàm dùng  : operation.getFreeDrivers() / operation.getFreeDrivers(vehicleType).
     */
    server.Get("/free-drivers", [](const httplib::Request& req, httplib::Response& res) {
        vector<string> ids;
        lock_guard<mutex> lock(databaseMutex);

        if (req.has_param("vehicleType")) {
            int vt;
            try {
                vt = stoi(req.get_param_value("vehicleType"));
            } catch (...) {
                return SendError(res, 400, "vehicleType phai la so nguyen");
            }
            if (vt != XE_MAY && vt != O_TO) {
                return SendError(res, 400, "vehicleType chi nhan 0 (XE_MAY) hoac 1 (O_TO)");
            }
            ids = operation.getFreeDrivers(static_cast<VehicleType>(vt));
        } else {
            ids = operation.getFreeDrivers();
        }
        SendJson(res, 200, {{"count", ids.size()}, {"driverIds", ids}});
    });

    /**
     * [ROUTE] POST /drivers/warn-cancellation
     * Chức năng : Cảnh báo các tài xế có tỷ lệ hủy cuốc >= ngưỡng (đơn vị %).
     * Body      : Tùy chọn {"threshold": 30}. Bỏ trống body thì dùng ngưỡng mặc định 30%. threshold phải trong 0..100.
     * Kết quả   : 200 + {"threshold", "warnedCount", "warnedDrivers": [id...]} | 400 sai dữ liệu.
     * Lưu ý     : Gọi SaveTable() vì các trường cancellationWarned và cancellationThreshold của tài xế bị thay đổi.
     * Hàm dùng  : operation.warnHighCancellationDrivers.
     */
    server.Post("/drivers/warn-cancellation", [](const httplib::Request& req, httplib::Response& res) {
        double threshold = 30.0;
        if (!req.body.empty()) {
            json body;
            if (!parseBody(req, res, body)) return;
            try {
                threshold = body.value("threshold", 30.0);
            } catch (const json::exception&) {
                return SendError(res, 400, "threshold phai la so");
            }
        }
        if (threshold < 0.0 || threshold > 100.0) {
            return SendError(res, 400, "threshold phai nam trong khoang 0 den 100");
        }

        lock_guard<mutex> lock(databaseMutex);
        vector<string> warned = operation.warnHighCancellationDrivers(threshold);
        SaveTable();   // vi cancellationWarned va cancellationThreshold da thay doi
        SendJson(res, 200, {{"threshold", threshold}, {"warnedCount", warned.size()}, {"warnedDrivers", warned}});
    });

    /**
     * [ROUTE] PUT /drivers/<id>/info
     * [TRÙNG LẶP] Route này đã được đăng ký ở phía trên với nội dung gần như giống hệt.
     * cpp-httplib khớp route theo thứ tự đăng ký, route khai báo trước được ưu tiên,
     * nên bản này thực tế KHÔNG BAO GIỜ được gọi (code thừa). Có thể xóa để gọn code.
     */
    server.Put(R"(/drivers/([^/]+)/info)", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        lock_guard<mutex> lock(databaseMutex);
        Driver* d = table.findDriver(id);
        if (!d) return SendError(res, 404, "Khong tim thay tai xe");

        string name, phone, plate;
        int vehicleType;
        try {
            name        = body.value("name", d->info.getName());
            phone       = body.value("phone", d->info.getPhone());
            plate       = body.value("licensePlate", d->info.getLicensePlate());
            vehicleType = body.value("vehicleType", static_cast<int>(d->info.vehicleType));
        } catch (const json::exception&) {
            return SendError(res, 400, "name, phone, licensePlate phai la string; vehicleType phai la so nguyen");
        }
        if (name.empty()) return SendError(res, 400, "name khong duoc rong");
        if (vehicleType != XE_MAY && vehicleType != O_TO)
            return SendError(res, 400, "vehicleType chi nhan 0 (XE_MAY) hoac 1 (O_TO)");

        operation.updateDriverInfo(id, name, phone, plate, static_cast<VehicleType>(vehicleType));
        SaveTable();
        SendJson(res, 200, driverToJson(*table.findDriver(id)));
    });

    /**
     * [ROUTE] POST /drivers/<id>/offer
     * [TRÙNG LẶP] Route này đã được đăng ký ở phía trên với nội dung gần như giống hệt.
     * cpp-httplib khớp route theo thứ tự đăng ký, route khai báo trước được ưu tiên,
     * nên bản này thực tế KHÔNG BAO GIỜ được gọi (code thừa). Có thể xóa để gọn code.
     */
    server.Post(R"(/drivers/([^/]+)/offer)", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        bool accepted;
        try {
            accepted = body.at("accepted").get<bool>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can truong boolean: accepted");
        }

        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(id)) return SendError(res, 404, "Khong tim thay tai xe");
        operation.recordTripOffer(id, accepted);
        SaveTable();
        SendJson(res, 200, driverToJson(*table.findDriver(id)));
    });

    /**
     * [ROUTE] POST /drivers/<id>/trip-result
     * [TRÙNG LẶP] Route này đã được đăng ký ở phía trên với nội dung gần như giống hệt.
     * cpp-httplib khớp route theo thứ tự đăng ký, route khai báo trước được ưu tiên,
     * nên bản này thực tế KHÔNG BAO GIỜ được gọi (code thừa). Có thể xóa để gọn code.
     */
    server.Post(R"(/drivers/([^/]+)/trip-result)", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        bool completed;
        float rating = 0.0f;
        try {
            completed = body.at("completed").get<bool>();
            if (completed) rating = body.at("rating").get<float>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can truong boolean: completed; neu completed = true thi can them rating (so)");
        }
        if (completed && (rating < 1.0f || rating > 5.0f))
            return SendError(res, 400, "rating phai tu 1 den 5");

        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(id)) return SendError(res, 404, "Khong tim thay tai xe");

        operation.updateTripResult(id, completed, rating);
        SaveTable();

        Driver* d = table.findDriver(id);   // co the da bi xoa do huy cuoc vuot nguong
        if (!d) {
            return SendJson(res, 200, {{"driverRemoved", true}, {"id", id},
                                       {"reason", "Huy cuoc vuot nguong sau khi bi canh bao"}});
        }
        SendJson(res, 200, driverToJson(*d));
    });

    /**
     * [ROUTE] GET /free-drivers
     * [TRÙNG LẶP] Route này đã được đăng ký ở phía trên với nội dung gần như giống hệt.
     * cpp-httplib khớp route theo thứ tự đăng ký, route khai báo trước được ưu tiên,
     * nên bản này thực tế KHÔNG BAO GIỜ được gọi (code thừa). Có thể xóa để gọn code.
     */
    server.Get("/free-drivers", [](const httplib::Request& req, httplib::Response& res) {
        vector<string> ids;
        lock_guard<mutex> lock(databaseMutex);

        if (req.has_param("vehicleType")) {
            int vt;
            try {
                vt = stoi(req.get_param_value("vehicleType"));
            } catch (...) {
                return SendError(res, 400, "vehicleType phai la so nguyen");
            }
            if (vt != XE_MAY && vt != O_TO)
                return SendError(res, 400, "vehicleType chi nhan 0 (XE_MAY) hoac 1 (O_TO)");
            ids = operation.getFreeDrivers(static_cast<VehicleType>(vt));
        } else {
            ids = operation.getFreeDrivers();
        }
        SendJson(res, 200, {{"count", ids.size()}, {"driverIds", ids}});
    });

    /**
     * [ROUTE] POST /drivers/warn-cancellation
     * [TRÙNG LẶP] Route này đã được đăng ký ở phía trên với nội dung gần như giống hệt.
     * cpp-httplib khớp route theo thứ tự đăng ký, route khai báo trước được ưu tiên,
     * nên bản này thực tế KHÔNG BAO GIỜ được gọi (code thừa). Có thể xóa để gọn code.
     */
    server.Post("/drivers/warn-cancellation", [](const httplib::Request& req, httplib::Response& res) {
        double threshold = 30.0;
        if (!req.body.empty()) {
            json body;
            if (!parseBody(req, res, body)) return;
            try {
                threshold = body.value("threshold", 30.0);
            } catch (const json::exception&) {
                return SendError(res, 400, "threshold phai la so");
            }
        }
        if (threshold < 0.0 || threshold > 100.0)
            return SendError(res, 400, "threshold phai nam trong khoang 0 den 100");

        lock_guard<mutex> lock(databaseMutex);
        vector<string> warned = operation.warnHighCancellationDrivers(threshold);
        SaveTable();
        SendJson(res, 200, {{"threshold", threshold}, {"warnedCount", warned.size()}, {"warnedDrivers", warned}});
    });

        // ===== Route cho DriverMathAndFilterProcessor =====

    /**
     * [ROUTE] GET /math/distance?x1=0&y1=0&x2=3&y2=4
     * Chức năng : Tính khoảng cách giữa hai điểm (x1, y1) và (x2, y2).
     * Query     : x1, y1, x2, y2 (số thực, bắt buộc).
     * Kết quả   : 200 + {"distance": ...} | 400 thiếu hoặc sai tham số.
     * Hàm dùng  : processor.calculateDistance.
     */
    server.Get("/math/distance", [](const httplib::Request& req, httplib::Response& res) {
        double x1, y1, x2, y2;
        if (!getParamDouble(req, res, "x1", x1) || !getParamDouble(req, res, "y1", y1) ||
            !getParamDouble(req, res, "x2", x2) || !getParamDouble(req, res, "y2", y2)) return;

        SendJson(res, 200, {{"distance", processor.calculateDistance(x1, y1, x2, y2)}});
    });

    /**
     * [ROUTE] GET /math/turning-angle?dX=1&dY=0&targetX=0&targetY=5&currX=0&currY=0
     * Chức năng : Tính góc quay (đơn vị độ, từ 0 đến 180) mà tài xế cần quay để đổi từ hướng đang chạy
     *             sang hướng tới điểm đích.
     * Query     : dX, dY       = vector hướng đang chạy của tài xế;
     *             currX, currY = vị trí hiện tại của tài xế;
     *             targetX, targetY = vị trí điểm đích (ví dụ vị trí khách). Tất cả là số thực, bắt buộc.
     * Kết quả   : 200 + {"angleDegrees": ...} | 400 thiếu hoặc sai tham số.
     * Hàm dùng  : processor.calculateTurningAngle.
     */
    server.Get("/math/turning-angle", [](const httplib::Request& req, httplib::Response& res) {
        double dX, dY, targetX, targetY, currX, currY;
        if (!getParamDouble(req, res, "dX", dX) || !getParamDouble(req, res, "dY", dY) ||
            !getParamDouble(req, res, "targetX", targetX) || !getParamDouble(req, res, "targetY", targetY) ||
            !getParamDouble(req, res, "currX", currX) || !getParamDouble(req, res, "currY", currY)) return;

        SendJson(res, 200, {{"angleDegrees",
            processor.calculateTurningAngle(dX, dY, targetX, targetY, currX, currY)}});
    });

    /**
     * [ROUTE] GET /drivers/<id>/score?customerX=1&customerY=2
     * Chức năng : Tính điểm ưu tiên của MỘT tài xế đối với khách ở vị trí (customerX, customerY)
     *             và khoảng cách từ tài xế đến khách. Điểm càng cao thì tài xế càng phù hợp để điều phối.
     * Query     : customerX, customerY (số thực, bắt buộc).
     * Kết quả   : 200 + {"id", "score", "distance"} | 400 sai tham số | 404 không tìm thấy tài xế.
     * Hàm dùng  : processor.calculateScore, processor.calculateDistance.
     */
    server.Get(R"(/drivers/([^/]+)/score)", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        double cx, cy;
        if (!getParamDouble(req, res, "customerX", cx) || !getParamDouble(req, res, "customerY", cy)) return;

        lock_guard<mutex> lock(databaseMutex);
        Driver* d = table.findDriver(id);
        if (!d) return SendError(res, 404, "Khong tim thay tai xe");

        SendJson(res, 200, {
            {"id", id},
            {"score", processor.calculateScore(*d, cx, cy)},
            {"distance", processor.calculateDistance(d->status.x, d->status.y, cx, cy)}
        });
    });

    /**
     * [ROUTE] GET /nearby-drivers?customerX=1&customerY=2&radius=5&vehicleType=0
     * Chức năng : Lọc các tài xế RẢNH, đúng loại xe và nằm trong bán kính `radius` quanh vị trí khách.
     * Query     : customerX, customerY (vị trí khách), radius (> 0), vehicleType (0 = XE_MAY, 1 = O_TO); đều bắt buộc và là số.
     * Kết quả   : 200 + {"count", "drivers": [...]} | 400 sai tham số.
     * Hàm dùng  : processor.filterDriversInRadiusAndType, table.getAllDrivers.
     */
    server.Get("/nearby-drivers", [](const httplib::Request& req, httplib::Response& res) {
        double cx, cy, radius, vt;
        if (!getParamDouble(req, res, "customerX", cx) || !getParamDouble(req, res, "customerY", cy) ||
            !getParamDouble(req, res, "radius", radius) || !getParamDouble(req, res, "vehicleType", vt)) return;

        if (radius <= 0) return SendError(res, 400, "radius phai lon hon 0");
        if (vt != XE_MAY && vt != O_TO)
            return SendError(res, 400, "vehicleType chi nhan 0 (XE_MAY) hoac 1 (O_TO)");

        lock_guard<mutex> lock(databaseMutex);
        vector<Driver> found = processor.filterDriversInRadiusAndType(
            table.getAllDrivers(), cx, cy, radius, static_cast<VehicleType>(vt));

        json arr = json::array();
        for (auto& d : found) arr.push_back(driverToJson(d));
        SendJson(res, 200, {{"count", found.size()}, {"drivers", arr}});
    });

    /**
     * [ROUTE] GET /dispatch/ranking?customerX=1&customerY=2&radius=5&vehicleType=0
     * Chức năng : Xếp hạng các tài xế phù hợp cho một khách. Cùng logic với processDispatch nhưng trả kết quả
     *             dạng JSON thay vì in ra console.
     * Các bước  : (1) lọc tài xế rảnh, đúng loại xe, trong bán kính
     *             (2) tính điểm calculateScore cho từng người, đưa vào priority_queue<HeapNode>
     *             (3) lấy lần lượt từ hàng đợi ra và đánh số hạng 1, 2, 3...
     *             (hạng 1 là tài xế đứng đầu hàng đợi ưu tiên, tức điểm tốt nhất theo cách so sánh của HeapNode).
     * Query     : customerX, customerY, radius (> 0), vehicleType (0 hoặc 1); đều bắt buộc.
     * Kết quả   : 200 + {"count", "ranking": [{"rank","driverId","score","distance"}...]} | 400 sai tham số.
     */
    server.Get("/dispatch/ranking", [](const httplib::Request& req, httplib::Response& res) {
        double cx, cy, radius, vt;
        if (!getParamDouble(req, res, "customerX", cx) || !getParamDouble(req, res, "customerY", cy) ||
            !getParamDouble(req, res, "radius", radius) || !getParamDouble(req, res, "vehicleType", vt)) return;

        if (radius <= 0) return SendError(res, 400, "radius phai lon hon 0");
        if (vt != XE_MAY && vt != O_TO)
            return SendError(res, 400, "vehicleType chi nhan 0 (XE_MAY) hoac 1 (O_TO)");

        lock_guard<mutex> lock(databaseMutex);
        vector<Driver> valid = processor.filterDriversInRadiusAndType(
            table.getAllDrivers(), cx, cy, radius, static_cast<VehicleType>(vt));

        priority_queue<HeapNode> pq;
        for (auto& d : valid) {
            pq.push(HeapNode{d.info.getId(), processor.calculateScore(d, cx, cy)});
        }

        json ranking = json::array();
        int rank = 1;
        while (!pq.empty()) {
            HeapNode top = pq.top();
            pq.pop();
            Driver* d = table.findDriver(top.driverID);
            ranking.push_back({
                {"rank", rank++},
                {"driverId", top.driverID},
                {"score", top.score},
                {"distance", processor.calculateDistance(d->status.x, d->status.y, cx, cy)}
            });
        }
        SendJson(res, 200, {{"count", ranking.size()}, {"ranking", ranking}});
    });

        // ===== Route cho TripManager (stackundo) =====

    /**
     * [Lambda / Luồng nền] Tự động chốt việc hủy chuyến khi hết 5 giây mà không ai hoàn tác.
     * Chức năng : Cứ mỗi 500 ms, khóa databaseMutex rồi duyệt tất cả chuyến trong `trips` và gọi
     *             finalizeCancelIfExpired(). Chuyến nào hết hạn hoàn tác sẽ bị hủy hoàn toàn.
     *             Nếu có chuyến thay đổi thì gọi SaveTable() để lưu lại thay đổi liên quan đến tài xế.
     * Lưu ý     : Dùng detach() nên luồng chạy ngầm suốt vòng đời chương trình (vòng lặp while(true), không bao giờ dừng).
     */
    thread([] {
        while (true) {
            this_thread::sleep_for(chrono::milliseconds(500));
            lock_guard<mutex> lock(databaseMutex);
            bool changed = false;
            for (auto& item : trips)
                if (item.second.manager->finalizeCancelIfExpired()) changed = true;
            if (changed) SaveTable();
        }
    }).detach();

    /**
     * [ROUTE] GET /timestamp
     * Chức năng : Trả về thời điểm hiện tại của server dưới dạng timestamp mili-giây.
     * Kết quả   : 200 + {"timestampMs": ...}
     * Hàm dùng  : getCurrentTimestamp.
     */
    server.Get("/timestamp", [](const httplib::Request&, httplib::Response& res) {
        SendJson(res, 200, {{"timestampMs", getCurrentTimestamp()}});
    });

    /**
     * [ROUTE] POST /trips
     * Chức năng : Tạo một chuyến mới (tạo TripManager) với trạng thái ban đầu TIM_XE (đang tìm xe).
     * Body      : {"driverId": "D001", "tripId": "T001"} (chuỗi, bắt buộc; tripId không được rỗng).
     * Kết quả   : 201 + thông tin chuyến | 400 dữ liệu sai | 404 tài xế không tồn tại | 409 tripId đã tồn tại.
     * Hàm dùng  : table.checkID, make_unique<TripManager>(driverId, tripId, &operation), tripToJson.
     */
    server.Post("/trips", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;

        string driverId, tripId;
        try {
            driverId = body.at("driverId").get<string>();
            tripId   = body.at("tripId").get<string>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can hai truong string: driverId, tripId");
        }
        if (tripId.empty()) return SendError(res, 400, "tripId khong duoc rong");

        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(driverId)) return SendError(res, 404, "Khong tim thay tai xe");
        if (trips.count(tripId)) return SendError(res, 409, "tripId da ton tai");

        TripSession& s = trips[tripId];
        s.driverId = driverId;
        s.manager = make_unique<TripManager>(driverId, tripId, &operation);
        SendJson(res, 201, tripToJson(tripId, s));
    });

    /**
     * [ROUTE] GET /trips
     * Chức năng : Lấy danh sách tất cả chuyến đang hoạt động.
     * Kết quả   : 200 + mảng JSON thông tin các chuyến (xem tripToJson).
     */
    server.Get("/trips", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(databaseMutex);
        json arr = json::array();
        for (auto& item : trips) arr.push_back(tripToJson(item.first, item.second));
        SendJson(res, 200, arr);
    });

    /**
     * [ROUTE] GET /trips/<tripId>
     * Chức năng : Xem trạng thái hiện tại của một chuyến (số và tên trạng thái).
     * Kết quả   : 200 + thông tin chuyến | 404 không tìm thấy chuyến.
     * Hàm dùng  : manager->getCurrentState, stateToString (thông qua tripToJson).
     */
    server.Get(R"(/trips/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        string tripId = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        auto it = trips.find(tripId);
        if (it == trips.end()) return SendError(res, 404, "Khong tim thay chuyen");
        SendJson(res, 200, tripToJson(tripId, it->second));
    });

    /**
     * [ROUTE] POST /trips/<tripId>/state
     * Chức năng : Chuyển chuyến sang trạng thái mới (đẩy trạng thái vào lịch sử của chuyến).
     * Body      : {"state": 0} hoặc {"state": 1}; 0 = TIM_XE, 1 = NHAN_CUOC.
     *             Hai trạng thái còn lại (CHO_XAC_NHAN_HUY, HUY_HOAN_TOAN) do luồng hủy chuyến quản lý, không đặt trực tiếp.
     * Kiểm tra  : Không đổi được nếu chuyến đang CHO_XAC_NHAN_HUY hoặc đã HUY_HOAN_TOAN (trả 409).
     * Tác dụng phụ: Khi chuyển sang NHAN_CUOC thì tài xế được đặt trạng thái CO_KHACH,
     *             gán currentTripID = tripId rồi lưu database.
     * Kết quả   : 200 + thông tin chuyến | 400 state sai | 404 không tìm thấy chuyến | 409 chuyến đang/đã hủy.
     * Hàm dùng  : manager->pushState, SaveTable.
     */
    server.Post(R"(/trips/([^/]+)/state)", [](const httplib::Request& req, httplib::Response& res) {
        string tripId = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        int st;
        try {
            st = body.at("state").get<int>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can truong so nguyen: state");
        }
        if (st != TIM_XE && st != NHAN_CUOC)
            return SendError(res, 400, "state chi nhan 0 (TIM_XE) hoac 1 (NHAN_CUOC); dung /cancel de huy chuyen");

        lock_guard<mutex> lock(databaseMutex);
        auto it = trips.find(tripId);
        if (it == trips.end()) return SendError(res, 404, "Khong tim thay chuyen");
        TripSession& s = it->second;

        State cur = s.manager->getCurrentState();
        if (cur == CHO_XAC_NHAN_HUY || cur == HUY_HOAN_TOAN)
            return SendError(res, 409, "Chuyen dang cho huy hoac da huy");

        s.manager->pushState(static_cast<State>(st), s.driverId, tripId);

        if (st == NHAN_CUOC) {   // tài xế nhận cuốc -> có khách
            Driver* d = table.findDriver(s.driverId);
            if (d) {
                d->status.currentStatus = CO_KHACH;
                d->status.currentTripID = tripId;
                SaveTable();
            }
        }
        SendJson(res, 200, tripToJson(tripId, s));
    });

    /**
     * [ROUTE] POST /trips/<tripId>/cancel
     * Chức năng : Yêu cầu hủy chuyến theo kiểu KHÔNG CHẶN: chuyển chuyến sang CHO_XAC_NHAN_HUY và bắt đầu đếm
     *             5 giây, request trả về ngay chứ không đứng chờ. Trong 5 giây này có thể hoàn tác bằng
     *             /undo-cancel; hết 5 giây luồng nền sẽ chốt hủy hoàn toàn.
     * Kết quả   : 202 (Accepted) + thông tin chuyến | 404 không tìm thấy chuyến
     *             | 409 chuyến đã hủy hoặc đang chờ xác nhận hủy.
     * Hàm dùng  : manager->requestCancel.
     */
    server.Post(R"(/trips/([^/]+)/cancel)", [](const httplib::Request& req, httplib::Response& res) {
        string tripId = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        auto it = trips.find(tripId);
        if (it == trips.end()) return SendError(res, 404, "Khong tim thay chuyen");

        if (!it->second.manager->requestCancel())
            return SendError(res, 409, "Chuyen da huy hoac dang cho xac nhan huy");
        SendJson(res, 202, tripToJson(tripId, it->second));
    });

    /**
     * [ROUTE] POST /trips/<tripId>/undo-cancel
     * Chức năng : Hoàn tác yêu cầu hủy chuyến, chỉ hợp lệ trong vòng 5 giây kể từ lúc gọi /cancel.
     * Kết quả   : 200 + thông tin chuyến (đã khôi phục trạng thái trước đó) | 404 không tìm thấy chuyến
     *             | 409 không có yêu cầu hủy nào đang chờ hoặc đã quá 5 giây.
     * Hàm dùng  : manager->undoCancel.
     */
    server.Post(R"(/trips/([^/]+)/undo-cancel)", [](const httplib::Request& req, httplib::Response& res) {
        string tripId = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        auto it = trips.find(tripId);
        if (it == trips.end()) return SendError(res, 404, "Khong tim thay chuyen");

        if (!it->second.manager->undoCancel())
            return SendError(res, 409, "Khong co yeu cau huy nao dang cho, hoac da qua 5 giay");
        SendJson(res, 200, tripToJson(tripId, it->second));
    });

    /**
     * [ROUTE] POST /trips/<tripId>/complete
     * Chức năng : Hoàn thành chuyến: cập nhật thống kê và rating cho tài xế, rồi xóa lịch sử và gỡ chuyến.
     * Body      : {"rating": 4.5} (số, bắt buộc, từ 1 đến 5).
     * Điều kiện : Chuyến phải đang ở trạng thái NHAN_CUOC (nếu không trả 409).
     * Các bước  : updateDriverStats(driverId, true, rating) -> clearTripData() -> trips.erase() -> SaveTable().
     * Kết quả   : 200 + {"completed": true, "tripId", "driver": {...}} | 400 rating sai
     *             | 404 không tìm thấy chuyến | 409 sai trạng thái.
     */
    server.Post(R"(/trips/([^/]+)/complete)", [](const httplib::Request& req, httplib::Response& res) {
        string tripId = req.matches[1];
        json body;
        if (!parseBody(req, res, body)) return;

        float rating;
        try {
            rating = body.at("rating").get<float>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can truong so: rating");
        }
        if (rating < 1.0f || rating > 5.0f) return SendError(res, 400, "rating phai tu 1 den 5");

        lock_guard<mutex> lock(databaseMutex);
        auto it = trips.find(tripId);
        if (it == trips.end()) return SendError(res, 404, "Khong tim thay chuyen");
        TripSession& s = it->second;

        if (s.manager->getCurrentState() != NHAN_CUOC)
            return SendError(res, 409, "Chi hoan thanh duoc chuyen o trang thai NHAN_CUOC");

        string driverId = s.driverId;
        s.manager->updateDriverStats(driverId, true, rating);
        s.manager->clearTripData();
        trips.erase(tripId);
        SaveTable();

        Driver* d = table.findDriver(driverId);
        SendJson(res, 200, {{"completed", true}, {"tripId", tripId},
                            {"driver", d ? driverToJson(*d) : json(nullptr)}});
    });

    /**
     * [ROUTE] DELETE /trips/<tripId>
     * Chức năng : Xóa lịch sử và gỡ chuyến khỏi hệ thống, KHÔNG tính vào thống kê của tài xế.
     *             Nếu tài xế đang gắn với chuyến này thì trả tài xế về trạng thái rảnh
     *             (RANH, xóa currentTripID, freeTime = 0) và lưu database.
     * Kết quả   : 200 + {"deleted": tripId} | 404 không tìm thấy chuyến.
     * Hàm dùng  : manager->clearTripData, SaveTable.
     */
    server.Delete(R"(/trips/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        string tripId = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        auto it = trips.find(tripId);
        if (it == trips.end()) return SendError(res, 404, "Khong tim thay chuyen");

        Driver* d = table.findDriver(it->second.driverId);
        if (d && d->status.currentTripID == tripId) {   // trả tài xế về trạng thái rảnh
            d->status.currentTripID = "";
            d->status.currentStatus = RANH;
            d->status.freeTime = 0;
            SaveTable();
        }
        it->second.manager->clearTripData();
        trips.erase(it);
        SendJson(res, 200, {{"deleted", tripId}});
    });

        // ===== Route cho Trie (AutoComplete) =====

    /**
     * [ROUTE] POST /locations
     * Chức năng : Thêm một hoặc nhiều địa điểm vào Trie (mỗi lần insert, UpdateSuggestLocation chạy bên trong để
     *             cập nhật danh sách gợi ý), đồng thời lưu locations.json và suggestions.json.
     * Body      : 1 object hoặc 1 mảng object, ví dụ {"name":"Hà Nội","posX":10,"posY":20,"population":8000000}.
     * Cơ chế    : "Tất cả hoặc không có gì": kiểm tra toàn bộ phần tử trước, chỉ khi TẤT CẢ hợp lệ mới thêm,
     *             tránh việc thêm dở dang.
     * Kiểm tra  : mỗi phần tử qua jsonToLocation(); không trùng với địa điểm đã có và không trùng nhau trong cùng lô
     *             (so sánh theo tên đã chuẩn hóa).
     * Kết quả   : 201 + {"inserted", "total", "locations"} | 400 dữ liệu sai (có chỉ rõ số thứ tự mục lỗi)
     *             | 409 địa điểm đã tồn tại.
     * Hàm dùng  : autoComplete.InsertLocation, persistTrie.
     */
    server.Post("/locations", [](const httplib::Request& req, httplib::Response& res) {
        json body = json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !(body.is_object() || body.is_array()))
            return SendError(res, 400, "Body phai la JSON object hoac mang object");
        json items = body.is_array() ? body : json::array({body});
        if (items.empty()) return SendError(res, 400, "Mang dia diem rong");

        lock_guard<mutex> lock(trieMutex);

        // Kiem tra het truoc, hop le moi them (tranh them do dang)
        vector<Location> toAdd;
        unordered_set<string> seen;
        for (size_t i = 0; i < items.size(); i++) {
            Location l; string err;
            if (!jsonToLocation(items[i], l, err))
                return SendError(res, 400, "Muc " + to_string(i) + ": " + err);
            if (findLocationIndex(l.name) >= 0 || !seen.insert(toLower(l.name)).second)
                return SendError(res, 409, "Dia diem da ton tai: " + l.name);
            toAdd.push_back(l);
        }

        json added = json::array();
        for (auto& l : toAdd) {
            autoComplete.InsertLocation(l);
            locationList.push_back(l);
            added.push_back(locationToJson(l));
        }
        persistTrie();   // ghi locations.json va suggestions.json

        SendJson(res, 201, {{"inserted", toAdd.size()}, {"total", locationList.size()}, {"locations", added}});
    });

    /**
     * [ROUTE] GET /locations/search?prefix=ha
     * Chức năng : Gợi ý địa điểm theo tiền tố (tự động hoàn thành / autocomplete).
     * Query     : prefix (bắt buộc), chuỗi người dùng đã gõ.
     * Kết quả   : 200 + {"prefix", "normalizedPrefix", "count", "suggestions": [...]}
     *             (normalizedPrefix là tiền tố sau khi chuẩn hóa: bỏ dấu, chữ thường) | 400 thiếu prefix.
     * Hàm dùng  : autoComplete.SearchPrefix, toLower.
     */
    server.Get("/locations/search", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("prefix")) return SendError(res, 400, "Thieu tham so: prefix");
        string prefix = req.get_param_value("prefix");

        lock_guard<mutex> lock(trieMutex);
        vector<Location> found = autoComplete.SearchPrefix(prefix);

        json arr = json::array();
        for (auto& l : found) arr.push_back(locationToJson(l));
        SendJson(res, 200, {{"prefix", prefix}, {"normalizedPrefix", toLower(prefix)},
                            {"count", found.size()}, {"suggestions", arr}});
    });

    /**
     * [ROUTE] GET /locations/normalize?name=Hà Nội
     * Chức năng : Chuẩn hóa tên địa điểm (bỏ dấu, đổi sang chữ thường) để xem Trie sẽ lưu/tìm tên đó như thế nào.
     * Query     : name (bắt buộc).
     * Kết quả   : 200 + {"original", "normalized"} | 400 thiếu name.
     * Hàm dùng  : autoComplete.ConvertFixed.
     */
    server.Get("/locations/normalize", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("name")) return SendError(res, 400, "Thieu tham so: name");
        Location in{req.get_param_value("name"), 0.0, 0.0, 0};

        lock_guard<mutex> lock(trieMutex);
        Location fixed = autoComplete.ConvertFixed(in);
        SendJson(res, 200, {{"original", in.name}, {"normalized", fixed.name}});
    });

    /**
     * [ROUTE] GET /locations/suggestions
     * Chức năng : Xem kết quả của UpdateSuggestLocation trên từng nút của Trie
     *             (chính là nội dung file suggestions.json). Dùng để kiểm tra/gỡ lỗi cây Trie.
     * Kết quả   : 200 + JSON do buildSuggestionsJson() tạo ra.
     */
    server.Get("/locations/suggestions", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(trieMutex);
        SendJson(res, 200, buildSuggestionsJson());
    });

    /**
     * [ROUTE] GET /locations
     * Chức năng : Lấy danh sách tất cả địa điểm đã thêm.
     * Kết quả   : 200 + {"count", "locations": [...]}
     */
    server.Get("/locations", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(trieMutex);
        json arr = json::array();
        for (auto& l : locationList) arr.push_back(locationToJson(l));
        SendJson(res, 200, {{"count", locationList.size()}, {"locations", arr}});
    });

    /**
     * [ROUTE] DELETE /locations?name=Hà Nội
     * Chức năng : Xóa một địa điểm theo tên. Vì Trie không có hàm xóa nên cách làm là bỏ địa điểm khỏi
     *             locationList rồi dựng lại toàn bộ cây (rebuildTrie) và lưu file (persistTrie).
     * Query     : name (bắt buộc, so khớp theo tên đã chuẩn hóa).
     * Kết quả   : 200 + {"deleted", "total"} | 400 thiếu name | 404 không tìm thấy địa điểm.
     * Hàm dùng  : findLocationIndex, rebuildTrie, persistTrie.
     */
    server.Delete("/locations", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("name")) return SendError(res, 400, "Thieu tham so: name");

        lock_guard<mutex> lock(trieMutex);
        int idx = findLocationIndex(req.get_param_value("name"));
        if (idx < 0) return SendError(res, 404, "Khong tim thay dia diem");

        string removed = locationList[idx].name;
        locationList.erase(locationList.begin() + idx);
        rebuildTrie();
        persistTrie();
        SendJson(res, 200, {{"deleted", removed}, {"total", locationList.size()}});
    });

    /**
     * [ROUTE] PUT /user-position
     * Chức năng : Đổi vị trí người dùng (userPosX, userPosY), là mốc dùng để xếp hạng gợi ý địa điểm.
     *             Điểm gợi ý chỉ được tính lúc insert nên phải dựng lại Trie để thứ hạng mới có hiệu lực.
     * Body      : {"x": 5, "y": 5} (số thực, bắt buộc).
     * Kết quả   : 200 + {"x", "y", "rebuiltLocations"} | 400 thiếu/sai kiểu x, y.
     * Hàm dùng  : rebuildTrie, persistTrie.
     */
    server.Put("/user-position", [](const httplib::Request& req, httplib::Response& res) {
        json body;
        if (!parseBody(req, res, body)) return;
        double x, y;
        try {
            x = body.at("x").get<double>();
            y = body.at("y").get<double>();
        } catch (const json::exception&) {
            return SendError(res, 400, "Can hai truong so: x, y");
        }

        lock_guard<mutex> lock(trieMutex);
        userPosX = x;
        userPosY = y;
        rebuildTrie();
        persistTrie();
        SendJson(res, 200, {{"x", x}, {"y", y}, {"rebuiltLocations", locationList.size()}});
    });

    /**
     * [ROUTE] GET /user-position
     * Chức năng : Lấy vị trí người dùng hiện tại.
     * Kết quả   : 200 + {"x": ..., "y": ...}
     */
    server.Get("/user-position", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(trieMutex);
        SendJson(res, 200, {{"x", userPosX}, {"y", userPosY}});
    });

    // Khởi chạy server tại cổng 8080 (nghe mọi địa chỉ 0.0.0.0).
    // listen() là hàm chặn (blocking): luồng chính dừng ở đây để xử lý request cho đến khi server tắt.
    cout << "Server đang chạy tại http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);
    return 0;
}