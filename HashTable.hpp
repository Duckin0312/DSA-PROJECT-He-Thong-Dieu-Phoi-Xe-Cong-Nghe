#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "Driver.hpp"
#include <unordered_map>
#include <vector>

class Operation;

class HashTable {
    friend class Operation;
private:
    std::unordered_map<std::string, Driver> data;
public:
    bool checkID(std::string ID);

    void addDriver(std::string ID, Driver driver); 

    Driver* findDriver(std::string ID); 

    void deleteDriver(std::string ID);

    void updateLocation(std::string ID, double x, double y);

    void updateStatus(std::string ID, DriverStatusType status);

    void updateFreeTime(std::string ID, int time);

    std::vector<Driver> getAllDrivers();
};

#endif
