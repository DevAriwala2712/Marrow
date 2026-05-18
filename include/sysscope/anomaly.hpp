#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace sysscope {

struct AnomalyAlert {
    std::string process_name;
    std::int32_t process_pid = 0;
    std::string metric;
    double z_score = 0;
    double observed = 0;
    double baseline = 0;
};

class AnomalyDetector {
public:
    AnomalyDetector(double alpha = 0.05, double z_threshold = 3.0)
        : alpha_(alpha), z_threshold_(z_threshold) {}

    std::optional<AnomalyAlert> observe(double value) {
        ++sample_count_;
        if (sample_count_ == 1) {
            mean_ = value;
            variance_ = 1.0;
            return std::nullopt;
        }
        const double stddev = std::sqrt(variance_);
        const double z = stddev > 0 ? std::abs(value - mean_) / stddev : 0;
        std::optional<AnomalyAlert> alert;
        if (sample_count_ > 10 && z >= z_threshold_) {
            alert = AnomalyAlert{"", 0, "value", z, value, mean_};
        }
        const double delta = value - mean_;
        mean_ += alpha_ * delta;
        variance_ = (1.0 - alpha_) * (variance_ + alpha_ * delta * delta);
        return alert;
    }

    double z_score(double value) const {
        const double stddev = std::sqrt(variance_);
        return stddev > 0 ? std::abs(value - mean_) / stddev : 0;
    }

    int sample_count() const { return sample_count_; }

private:
    double alpha_;
    double z_threshold_;
    double mean_ = 0;
    double variance_ = 1;
    int sample_count_ = 0;
};

class ProcessAnomalyTracker {
public:
    explicit ProcessAnomalyTracker(double z_threshold = 3.0) : z_threshold_(z_threshold) {}

    std::optional<AnomalyAlert> observe(const std::string& process_name, std::int32_t pid,
                                        const std::string& metric, double value) {
        const std::string key = std::to_string(pid) + ":" + metric;
        auto& det = detectors_[key];
        if (det.sample_count() == 0) det = AnomalyDetector(0.05, z_threshold_);
        const double baseline = det.sample_count() > 0 ? value : 0;
        const double z_before = det.sample_count() > 10 ? det.z_score(value) : 0;
        (void)det.observe(value);
        if (det.sample_count() > 10 && z_before >= z_threshold_) {
            return AnomalyAlert{process_name, pid, metric, z_before, value, baseline};
        }
        return std::nullopt;
    }

private:
    double z_threshold_;
    std::unordered_map<std::string, AnomalyDetector> detectors_;
};

}  // namespace sysscope
