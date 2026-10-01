#include "pch.h"
#include "HashTable.hpp"
#include "operation.hpp"
#include "Trie.hpp"
#include "stackundo.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;
using json = nlohmann::json;

vector<Driver> GetDatabase();
void PushDatabase(vector<Driver> drivers);

HashTable table;
Operation operation(table);
mutex databaseMutex;

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



int main(){
    SetConsoleOutputCP(65001); 
    SetConsoleCP(65001);

    //Nạp dữ liệu từ file cũ vào table
    for(auto& driver : GetDatabase()){
        table.addDriver(driver.info.getId(), driver);
    }

    httplib::Server server;

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

    cout << "Server đang chạy tại http://localhost:8080\n";
    server.listen("0.0.0.0", 8080);
    return 0;
}