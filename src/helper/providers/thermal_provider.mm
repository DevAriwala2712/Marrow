#include "sysscope/providers/thermal_provider.hpp"

#import <Foundation/Foundation.h>

namespace sysscope {

void ThermalProvider::tick(MetricsSnapshot& snapshot) {
    @autoreleasepool {
        switch (NSProcessInfo.processInfo.thermalState) {
            case NSProcessInfoThermalStateNominal: snapshot.thermal.pressure = "Nominal"; break;
            case NSProcessInfoThermalStateFair: snapshot.thermal.pressure = "Fair"; break;
            case NSProcessInfoThermalStateSerious: snapshot.thermal.pressure = "Serious"; break;
            case NSProcessInfoThermalStateCritical: snapshot.thermal.pressure = "Critical"; break;
            default: return;
        }
    }
    snapshot.has_thermal = true;
}

}  // namespace sysscope
