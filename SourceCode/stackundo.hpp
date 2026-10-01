#pragma once
#include <string>
#include <stack>

enum State {
    TIM_XE,
    NHAN_CUOC,
    CHO_XAC_NHAN_HUY,
    HUY_HOAN_TOAN
};

std::string stateToString(State state);

struct Trip {
    State state;
    std::string driverID;
    std::string tripID;   
    long long timestamp;
};

long long getCurrentTimestamp();

class Operation;

class TripManager {
private:
    std::stack<Trip> tripHistory; 
    Operation* operation;

public:
    TripManager(const std::string& driverID, const std::string& tripID, Operation* operation);

    void pushState(State newState, const std::string& driverID, const std::string& tripID);

    State getCurrentState() const;

    void cancelTripRequest();

    void clearTripData();

    void updateDriverStats(const std::string& driverID, bool completed, float tripRating = 0.0f);
};