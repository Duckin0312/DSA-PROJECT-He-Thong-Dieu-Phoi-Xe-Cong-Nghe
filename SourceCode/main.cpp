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

vector<Driver> GetDatabase();
void PushDatabase(vector<Driver> drivers);

HashTable table;
Operation operation(table);
DriverMathAndFilterProcessor processor;
mutex databaseMutex;

AutoComplete autoComplete;
vector<Location> locationList; 
mutex trieMutex;                

struct TripSession {
    string driverId;
    unique_ptr<TripManager> manager;
};
map<string, TripSession> trips;

//chuyển đổi từ struct driver thành json
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

//chuyển đổi json thành struct driver
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

static void SaveTable(){
    PushDatabase(table.getAllDrivers());
}

static void SendJson(httplib::Response& res, int status, const json& body){
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

static void SendError(httplib::Response& res, int status, const string& msg){
    SendJson(res, status, {{"error", msg}});
}

static bool parseBody(const httplib::Request& req, httplib::Response& res, json& body){
    body = json::parse(req.body, nullptr, false);
    if(body.is_discarded() || !body.is_object()){
        SendError(res, 400, "Body phải là JSON object hợp lệ");
        return false;
    }
    return true;
}

// Đọc tham số số thực từ query string (?name=1.5). Trả false và gửi lỗi 400 nếu thiếu hoặc sai.
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

static json locationToJson(const Location& l) {
    return {{"name", l.name}, {"posX", l.posX}, {"posY", l.posY}, {"population", l.population}};
}

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

// Hai dia diem coi la trung neu ten da chuan hoa giong nhau (vi cung nam tren mot nut cua Trie)
static int findLocationIndex(const string& name) {
    string key = toLower(name);
    for (size_t i = 0; i < locationList.size(); i++)
        if (toLower(locationList[i].name) == key) return static_cast<int>(i);
    return -1;
}

// Dung lai Trie tu locationList (can khi doi userPosX/Y hoac xoa dia diem)
static void rebuildTrie() {
    autoComplete.Clear();
    for (auto& l : locationList) autoComplete.InsertLocation(l);
}

// Cau truc json cua suggestions.json
static json buildSuggestionsJson() {
    json nodes = json::object();
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

// Ghi ca hai file. Goi khi dang giu trieMutex.
static void persistTrie() {
    json arr = json::array();
    for (auto& l : locationList) arr.push_back(locationToJson(l));
    json data = {{"userPosition", {{"x", userPosX}, {"y", userPosY}}}, {"locations", arr}};

    ofstream f1("locations.json");
    if (f1.is_open()) f1 << data.dump(4); else cerr << "Khong the mo locations.json\n";

    ofstream f2("suggestions.json");
    if (f2.is_open()) f2 << buildSuggestionsJson().dump(4); else cerr << "Khong the mo suggestions.json\n";
}

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


int main(){
    SetConsoleOutputCP(65001); 
    SetConsoleCP(65001);

    //Nạp dữ liệu từ file cũ vào table
    for(auto& driver : GetDatabase()){
        table.addDriver(driver.info.getId(), driver);
    }

    loadLocations();

    httplib::Server server;
    server.set_mount_point("/", "./public");

    // CORS
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

    //POST /drivers - Thêm tài xế, tự động lưu vào database.json
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

     // GET /drivers: lấy danh sách tất cả tài xế
    server.Get("/drivers", [](const httplib::Request&, httplib::Response& res){
        lock_guard<mutex> lock(databaseMutex);
        json arr = json::array();
        for(auto& d : table.getAllDrivers()){
            arr.push_back(driverToJson(d));
        }
        SendJson(res, 200, arr);
    });

    // GET /drivers/<id>: lấy một tài xế
    server.Get(R"(/drivers/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        Driver* d = table.findDriver(id);
        if (!d) return SendError(res, 404, "Khong tim thay tai xe");
        SendJson(res, 200, driverToJson(*d));
    });

    // DELETE /drivers/<id>: xóa tài xế, tự động lưu
    server.Delete(R"(/drivers/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        string id = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        if (!table.checkID(id)) return SendError(res, 404, "Khong tim thay tai xe");
        table.deleteDriver(id);
        SaveTable();
        SendJson(res, 200, {{"deleted", id}});
    });

    // PUT /drivers/<id>/location   body: {"x": 1.5, "y": 2.5}
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

    // PUT /drivers/<id>/status   body: {"status": 0}  (0 = RANH, 1 = CO_KHACH, 2 = NGUNG_CHAY)
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

    // PUT /drivers/<id>/free-time   body: {"freeTime": 120}
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

    // PUT /drivers/<id>/info   body: name, phone, licensePlate, vehicleType (truong nao thieu se giu gia tri cu)
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

    // POST /drivers/<id>/offer   body: {"accepted": true}
    // Ghi nhan tai xe duoc moi mot cuoc (va co nhan hay khong)
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

    // POST /drivers/<id>/trip-result
    //   hoan thanh: {"completed": true, "rating": 4.5}
    //   huy cuoc:   {"completed": false}
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

    // GET /free-drivers                 -> tat ca tai xe ranh
    // GET /free-drivers?vehicleType=0   -> tai xe ranh theo loai xe
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

    // POST /drivers/warn-cancellation   body (tuy chon): {"threshold": 30}
    // Canh bao cac tai xe co ty le huy cuoc >= threshold (%)
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

    // recordTripOffer
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

    // updateTripResult
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

    // getFreeDrivers() và getFreeDrivers(vehicleType)
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

    // warnHighCancellationDrivers
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

    // calculateDistance
    // GET /math/distance?x1=0&y1=0&x2=3&y2=4
    server.Get("/math/distance", [](const httplib::Request& req, httplib::Response& res) {
        double x1, y1, x2, y2;
        if (!getParamDouble(req, res, "x1", x1) || !getParamDouble(req, res, "y1", y1) ||
            !getParamDouble(req, res, "x2", x2) || !getParamDouble(req, res, "y2", y2)) return;

        SendJson(res, 200, {{"distance", processor.calculateDistance(x1, y1, x2, y2)}});
    });

    // calculateTurningAngle (đơn vị: độ, từ 0 đến 180)
    // GET /math/turning-angle?dX=1&dY=0&targetX=0&targetY=5&currX=0&currY=0
    server.Get("/math/turning-angle", [](const httplib::Request& req, httplib::Response& res) {
        double dX, dY, targetX, targetY, currX, currY;
        if (!getParamDouble(req, res, "dX", dX) || !getParamDouble(req, res, "dY", dY) ||
            !getParamDouble(req, res, "targetX", targetX) || !getParamDouble(req, res, "targetY", targetY) ||
            !getParamDouble(req, res, "currX", currX) || !getParamDouble(req, res, "currY", currY)) return;

        SendJson(res, 200, {{"angleDegrees",
            processor.calculateTurningAngle(dX, dY, targetX, targetY, currX, currY)}});
    });

    // calculateScore cho một tài xế cụ thể
    // GET /drivers/<id>/score?customerX=1&customerY=2
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

    // filterDriversInRadiusAndType: tài xế RẢNH, đúng loại xe, trong bán kính R
    // GET /nearby-drivers?customerX=1&customerY=2&radius=5&vehicleType=0
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

    // Xếp hạng tài xế cho một khách (lọc + tính điểm + priority_queue, giống processDispatch
    // nhưng trả kết quả qua JSON thay vì in ra console)
    // GET /dispatch/ranking?customerX=1&customerY=2&radius=5&vehicleType=0
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

    // Luồng nền: tự chốt hủy khi hết 5 giây mà không ai hoàn tác
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

    // getCurrentTimestamp
    // GET /timestamp
    server.Get("/timestamp", [](const httplib::Request&, httplib::Response& res) {
        SendJson(res, 200, {{"timestampMs", getCurrentTimestamp()}});
    });

    // Constructor TripManager: tạo chuyến mới, trạng thái TIM_XE
    // POST /trips   body: {"driverId": "D001", "tripId": "T001"}
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

    // GET /trips: danh sách các chuyến
    server.Get("/trips", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(databaseMutex);
        json arr = json::array();
        for (auto& item : trips) arr.push_back(tripToJson(item.first, item.second));
        SendJson(res, 200, arr);
    });

    // getCurrentState + stateToString
    // GET /trips/<tripId>
    server.Get(R"(/trips/([^/]+))", [](const httplib::Request& req, httplib::Response& res) {
        string tripId = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        auto it = trips.find(tripId);
        if (it == trips.end()) return SendError(res, 404, "Khong tim thay chuyen");
        SendJson(res, 200, tripToJson(tripId, it->second));
    });

    // pushState
    // POST /trips/<tripId>/state   body: {"state": 1}   (0 = TIM_XE, 1 = NHAN_CUOC)
    // Hai trạng thái còn lại do luồng hủy cuốc quản lý
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

    // cancelTripRequest (bản không chặn): bắt đầu đếm 5 giây
    // POST /trips/<tripId>/cancel
    server.Post(R"(/trips/([^/]+)/cancel)", [](const httplib::Request& req, httplib::Response& res) {
        string tripId = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        auto it = trips.find(tripId);
        if (it == trips.end()) return SendError(res, 404, "Khong tim thay chuyen");

        if (!it->second.manager->requestCancel())
            return SendError(res, 409, "Chuyen da huy hoac dang cho xac nhan huy");
        SendJson(res, 202, tripToJson(tripId, it->second));
    });

    // Hoàn tác hủy trong vòng 5 giây
    // POST /trips/<tripId>/undo-cancel
    server.Post(R"(/trips/([^/]+)/undo-cancel)", [](const httplib::Request& req, httplib::Response& res) {
        string tripId = req.matches[1];
        lock_guard<mutex> lock(databaseMutex);
        auto it = trips.find(tripId);
        if (it == trips.end()) return SendError(res, 404, "Khong tim thay chuyen");

        if (!it->second.manager->undoCancel())
            return SendError(res, 409, "Khong co yeu cau huy nao dang cho, hoac da qua 5 giay");
        SendJson(res, 200, tripToJson(tripId, it->second));
    });

    // updateDriverStats (completed = true): hoàn thành chuyến, cập nhật rating, rồi xóa lịch sử
    // POST /trips/<tripId>/complete   body: {"rating": 4.5}
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

    // clearTripData: xóa lịch sử và gỡ chuyến (không tính vào thống kê tài xế)
    // DELETE /trips/<tripId>
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

    // InsertLocation (UpdateSuggestLocation chay ben trong). Nhan 1 object hoac 1 mang object.
    // POST /locations   body: {"name":"Hà Nội","posX":10,"posY":20,"population":8000000}
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

    // SearchPrefix
    // GET /locations/search?prefix=ha
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

    // ConvertFixed: bo dau, ve chu thuong
    // GET /locations/normalize?name=Hà Nội
    server.Get("/locations/normalize", [](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("name")) return SendError(res, 400, "Thieu tham so: name");
        Location in{req.get_param_value("name"), 0.0, 0.0, 0};

        lock_guard<mutex> lock(trieMutex);
        Location fixed = autoComplete.ConvertFixed(in);
        SendJson(res, 200, {{"original", in.name}, {"normalized", fixed.name}});
    });

    // Xem ket qua cua UpdateSuggestLocation tren moi nut (cung la noi dung suggestions.json)
    // GET /locations/suggestions
    server.Get("/locations/suggestions", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(trieMutex);
        SendJson(res, 200, buildSuggestionsJson());
    });

    // GET /locations: danh sach tat ca dia diem da insert
    server.Get("/locations", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(trieMutex);
        json arr = json::array();
        for (auto& l : locationList) arr.push_back(locationToJson(l));
        SendJson(res, 200, {{"count", locationList.size()}, {"locations", arr}});
    });

    // Trie khong co ham xoa, nen xoa bang cach bo khoi danh sach roi dung lai cay
    // DELETE /locations?name=Hà Nội
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

    // Doi vi tri nguoi dung (userPosX/Y). Diem goi y chi duoc tinh luc insert,
    // nen phai dung lai cay de thu hang moi co hieu luc.
    // PUT /user-position   body: {"x": 5, "y": 5}
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

    // GET /user-position
    server.Get("/user-position", [](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(trieMutex);
        SendJson(res, 200, {{"x", userPosX}, {"y", userPosY}});
    });

    cout << "Server đang chạy tại http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);
    return 0;
}