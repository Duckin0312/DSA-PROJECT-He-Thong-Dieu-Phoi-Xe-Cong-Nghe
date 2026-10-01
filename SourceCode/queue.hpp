#ifndef QUEUE_H
#define QUEUE_H

#include "Driver.hpp"
#include <vector>
#include <queue>
#include <string>

using namespace std;

struct HeapNode {
    string driverID;
    double score;

    bool operator<(const HeapNode& other) const;
};

class DriverMathAndFilterProcessor {
private:
    const double PI = 3.14159265358979323846;

public:
    double calculateDistance(double x1, double y1, double x2, double y2);
    double calculateTurningAngle(double dX, double dY, double targetX, double targetY, double currX, double currY);
    double calculateScore(const Driver& driver, double customerX, double customerY);
    vector<Driver> filterDriversInRadiusAndType(const vector<Driver>& freeDriversList, 
                                                double customerX, double customerY, 
                                                double radiusR, VehicleType requiredVehicleType);
};

void processDispatch(vector<Driver>& allDrivers, double customerX, double customerY, double radiusR, VehicleType reqType);

#endif