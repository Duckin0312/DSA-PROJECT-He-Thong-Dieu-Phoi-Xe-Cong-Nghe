#include "HashTable.hpp"
#include <iostream>
#include <vector>
using namespace std;

bool HashTable::checkID(string ID) {
    return data.find(ID) != data.end();
}

void HashTable::addDriver(string ID, Driver driver) {
    if(checkID(ID)) {
        cout << "ID da ton tai!\n";
        return;
    }
    data[ID] = driver;
}

Driver* HashTable::findDriver(string ID) {
    auto it = data.find(ID);
    if(it == data.end()) {
        return nullptr;
    }
    return &(it->second);
}

void HashTable::deleteDriver(string ID) {
    if(!checkID(ID)) {
        cout << "Khong tim thay tai xe!\n";
        return;
    }
    data.erase(ID);
}

void HashTable::updateLocation(string ID, double x, double y) {
    if(!checkID(ID)) {
        cout << "Khong tim thay tai xe!\n";
        return;
    }
    data[ID].status.x = x;
    data[ID].status.y = y;
}

void HashTable::updateStatus(string ID, DriverStatusType status) {
    if(!checkID(ID)) {
        cout << "Khong tim thay tai xe!\n";
        return;
    }
    data[ID].status.currentStatus = status;
}

void HashTable::updateFreeTime(string ID, int time) {
    if(!checkID(ID)) {
        cout << "Khong tim thay tai xe!\n";
        return;
    }
    data[ID].status.freeTime = time;
}

vector<Driver> HashTable::getAllDrivers(){
    vector<Driver> drivers;
    drivers.reserve(data.size());
    for(const auto& driver : data){
        drivers.push_back(driver.second);
    }
    return drivers;
}