#include "queue.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
using namespace std;

bool HeapNode::operator<(const HeapNode& other) const {
    return this->score < other.score; 
}

double DriverMathAndFilterProcessor::calculateDistance(double x1, double y1, double x2, double y2) {
    return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

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
