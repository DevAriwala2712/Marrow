#include "sysscope/providers/network_provider.hpp"
#include "sysscope/util.hpp"

#include <net/if.h>
#include <net/route.h>
#include <sys/sysctl.h>

#include <cstring>
#include <vector>

namespace sysscope {

void NetworkProvider::tick(MetricsSnapshot& snapshot) {
    int mib[6] = {CTL_NET, AF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
    size_t len = 0;
    if (sysctl(mib, 6, nullptr, &len, nullptr, 0) != 0) return;

    std::vector<char> buf(len);
    if (sysctl(mib, 6, buf.data(), &len, nullptr, 0) != 0) return;

    std::uint64_t total_in = 0;
    std::uint64_t total_out = 0;
    std::string primary;
    std::uint64_t primary_bytes = 0;

    char* next = buf.data();
    char* lim = buf.data() + len;
    while (next < lim) {
        auto* ifm = reinterpret_cast<if_msghdr2*>(next);
        next += ifm->ifm_msglen;
        if (ifm->ifm_type != RTM_IFINFO2) continue;

        char ifname[IFNAMSIZ] = {};
        if (if_indextoname(ifm->ifm_index, ifname) == nullptr) continue;
        if (std::strncmp(ifname, "lo", 2) == 0) continue;
        if ((ifm->ifm_flags & IFF_UP) == 0) continue;

        const std::uint64_t ibytes = ifm->ifm_data.ifi_ibytes;
        const std::uint64_t obytes = ifm->ifm_data.ifi_obytes;
        total_in += ibytes;
        total_out += obytes;

        const std::uint64_t sum = ibytes + obytes;
        if (sum >= primary_bytes) {
            primary_bytes = sum;
            primary = ifname;
        }
    }

    const double now = now_seconds();
    if (has_prev_ && now > prev_at_) {
        const double dt = now - prev_at_;
        snapshot.network.bytes_in_per_sec = static_cast<double>(total_in - prev_in_) / dt;
        snapshot.network.bytes_out_per_sec = static_cast<double>(total_out - prev_out_) / dt;
    }
    prev_in_ = total_in;
    prev_out_ = total_out;
    prev_at_ = now;
    has_prev_ = true;

    snapshot.network.interface_name = primary.empty() ? "unknown" : primary;
    snapshot.has_network = true;
}

}  // namespace sysscope
