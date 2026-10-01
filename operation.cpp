#include "operation.hpp"
using namespace std;

// Moi loi moi cuoc chi duoc ghi nhan mot lan.
void Operation::recordTripOffer(string ID, bool accepted) {
    auto it = table.data.find(ID);
    if(it == table.data.end()) return;
    DriverPerformance &p = it->second.performance;
    p.offeredTrips++;
    if(accepted) p.acceptedTrips++;
    p.acceptanceRate = 100.0f * p.acceptedTrips / p.offeredTrips;
}

// Goi sau khi cuoc da hoan thanh hoac bi huy; rating chi ap dung cho cuoc hoan thanh.
void Operation::updateTripResult(string ID, bool completed, float tripRating) {
    auto it = table.data.find(ID);
    if(it == table.data.end()) return;
    DriverPerformance &p = it->second.performance;
    if(completed) {
        if(tripRating < 1.0f || tripRating > 5.0f) {
            cout << "Rating phai tu 1 den 5!\n";
            return;
        }
        p.rating = (p.rating * p.completedTrips + tripRating) / (p.completedTrips + 1);
        p.completedTrips++;
    } else {
        p.cancelledTrips++;
        int total = p.completedTrips + p.cancelledTrips;
        double rate = 100.0 * p.cancelledTrips / total;
        if(p.cancellationWarned && total >= 3 && rate >= p.cancellationThreshold) {
            table.data.erase(it);
            cout << "Tai xe " << ID << " tiep tuc huy cuoc, da xoa!\n";
            return;
        }
    }
    it->second.status.currentTripID = "";
    it->second.status.currentStatus = RANH;
    it->second.status.freeTime = 0;
}

void Operation::updateDriverInfo(string ID, string name, string phone,
                                 string licensePlate, VehicleType vehicleType) {
    auto it = table.data.find(ID);
    if(it == table.data.end()) return;
    it->second.info.setName(name);
    it->second.info.setPhone(phone);
    it->second.info.setLicensePlate(licensePlate);
    it->second.info.vehicleType = vehicleType;
}

vector<string> Operation::getFreeDrivers() {
    vector<string> result;
    for(const auto &item : table.data)
        if(item.second.status.currentStatus == RANH) result.push_back(item.first);
    return result;
}

vector<string> Operation::getFreeDrivers(VehicleType vehicleType) {
    vector<string> result;
    for(const auto &item : table.data)
        if(item.second.status.currentStatus == RANH &&
           item.second.info.vehicleType == vehicleType) result.push_back(item.first);
    return result;
}

vector<string> Operation::warnHighCancellationDrivers(double threshold) {
    vector<string> result;
    if(threshold < 0.0 || threshold > 100.0) return result;
    for(auto &item : table.data) {
        DriverPerformance &p = item.second.performance;
        int total = p.completedTrips + p.cancelledTrips;
        if(total >= 3 && 100.0 * p.cancelledTrips / total >= threshold) {
            p.cancellationWarned = true;
            p.cancellationThreshold = threshold;
            result.push_back(item.first);
            cout << "Canh bao tai xe " << item.first << ": ty le huy cuoc cao!\n";
        }
    }
    return result;
}
