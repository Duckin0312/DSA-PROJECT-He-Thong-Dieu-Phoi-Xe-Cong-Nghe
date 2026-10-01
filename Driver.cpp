#include "Driver.hpp"
using namespace std;

string DriverInfo::getId(){
    return id;
}

string DriverInfo::getName() {
    return name;
}

void DriverInfo::setId(string id) {
    this->id = id;
}

void DriverInfo::setName(string name) {
    this->name = name;
}

string DriverInfo::getPhone() {
    return phone;
}

void DriverInfo::setPhone(string phone) {
    this->phone = phone;
}

string DriverInfo::getLicensePlate() {
    return licensePlate;
}

void DriverInfo::setLicensePlate(string licensePlate) {
    this->licensePlate = licensePlate;
}