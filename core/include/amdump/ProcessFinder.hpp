#pragma once
#include <string>

namespace Amdump {

class ProcessFinder {
public:
    static bool IsAllDigits(const std::string& s);
    static int FindByPackage(const std::string& package);
};

}
