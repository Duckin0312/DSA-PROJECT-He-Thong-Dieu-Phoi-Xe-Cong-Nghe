#ifndef OPERATION_H
#define OPERATION_H

#include "HashTable.hpp"
#include <vector>

class Operation {
private:
    HashTable &table;
public:
    explicit Operation(HashTable &hashTable) : table(hashTable) {}

    void recordTripOffer(std::string ID, bool accepted);
    void updateTripResult(std::string ID, bool completed, float tripRating = 0.0f);
    void updateDriverInfo(std::string ID, std::string name, std::string phone,
                          std::string licensePlate, VehicleType vehicleType);
    std::vector<std::string> getFreeDrivers();
    std::vector<std::string> getFreeDrivers(VehicleType vehicleType);
    std::vector<std::string> warnHighCancellationDrivers(double threshold = 30.0);
};

#endif
