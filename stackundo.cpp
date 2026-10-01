#include "stackundo.hpp"
#include "operation.hpp"
#include <iostream>
#include <chrono>
#include <thread>

using namespace std; 

string stateToString(State state){
    switch (state) {
        case TIM_XE: return "Tim xe";
        case NHAN_CUOC: return "Nhan cuoc";
        case CHO_XAC_NHAN_HUY: return "Cho xac nhan huy (Dem 5s)";
        case HUY_HOAN_TOAN: return "Huy hoan toan";
        default: return "Khong xac dinh";
    }
}

long long getCurrentTimestamp() {
    return chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch()
    ).count();
}

TripManager::TripManager(const string& driverID, const string& tripID, Operation* operation) : operation(operation) {
    pushState(TIM_XE, driverID, tripID);
}

void TripManager::pushState(State newState, const string& driverID, const string& tripID) {
    Trip t = { newState, driverID, tripID, getCurrentTimestamp() };
    tripHistory.push(t);
    cout << "[LOG] Trang thai hien tai: " << stateToString(newState) << endl;
}

State TripManager::getCurrentState() const {
    if (!tripHistory.empty()) {
        return tripHistory.top().state;
    }
    return HUY_HOAN_TOAN;
}

void TripManager::cancelTripRequest() {
    if (tripHistory.empty()) return;

    Trip currentTrip = tripHistory.top();

    pushState(CHO_XAC_NHAN_HUY, currentTrip.driverID, currentTrip.tripID);

    bool undoExecuted = false;
    auto startTime = chrono::steady_clock::now();
    int lastDisplayedSecond = 5;

    cout << "Thoi gian con lai: 5s..." << endl;

    while (true) {
        auto currentTime = chrono::steady_clock::now();
        auto elapsedMs = chrono::duration_cast<chrono::milliseconds>(currentTime - startTime).count();

        if (elapsedMs >= 5000) {
            break;
        }

        int remainingSeconds = 5 - static_cast<int>(elapsedMs / 1000);
        if (remainingSeconds < lastDisplayedSecond) {
            lastDisplayedSecond = remainingSeconds;
        }

        this_thread::sleep_for(chrono::milliseconds(50));
    }

    if (undoExecuted) {
        tripHistory.pop();
    }
    else {
        pushState(HUY_HOAN_TOAN, currentTrip.driverID, currentTrip.tripID);

        clearTripData();
        updateDriverStats(currentTrip.driverID, false);
    }
}

void TripManager::clearTripData() {
    while (!tripHistory.empty()) {
        tripHistory.pop();
    }
}

void TripManager::updateDriverStats(const string& driverID, bool completed, float tripRating) {
    if (operation == nullptr) return;
    operation->updateTripResult(driverID, completed, tripRating);
}
