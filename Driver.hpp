#ifndef DRIVER_H
#define DRIVER_H

#include <iostream>

enum VehicleType {
    XE_MAY,
    O_TO
};

enum DriverStatusType {
    RANH,
    CO_KHACH,
    NGUNG_CHAY
};

class DriverInfo {
private:
    std::string name;
    std::string phone;
    std::string licensePlate;    
    std::string id;
public:
    VehicleType vehicleType;
    std::string getName();
    void setName(std::string name);
    std::string getPhone();
    void setPhone(std::string phone);
    std::string getLicensePlate();
    void setLicensePlate(std::string licensePlate);
    std::string getId();
    void setId(std::string id);
};

struct DriverStatus {
    double x, y;
    double diractionX, diractionY;
    DriverStatusType currentStatus;
    int freeTime;
    std::string currentTripID;
};

struct DriverPerformance {
    float rating;
    int completedTrips;
    int acceptedTrips;
    float acceptanceRate;
    int offeredTrips = 0;
    int cancelledTrips = 0;
    bool cancellationWarned = false;
    double cancellationThreshold = 30.0;
};

struct Driver {
    DriverInfo info;
    DriverStatus status;
    DriverPerformance performance;

    Driver() = default;

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
