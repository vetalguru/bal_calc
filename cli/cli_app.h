#ifndef BALCALC_CLI_APP_H
#define BALCALC_CLI_APP_H

#include <iosfwd>
#include <string>
#include <vector>

namespace balcli {

// Runs bal-cli with `args` (without the program name), writing results to
// `out` and diagnostics to `err`. Returns the process exit code.
int Run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

// Parses a wind spec "SPEED@DIR[:UNTIL]": speed in m/s, direction the wind
// blows from either in degrees ("90") or on the clock ("3h", "10:30h"),
// optional end of the zone in metres. Returns false on bad input.
struct WindSpec {
    double speed_mps = 0.0;
    double from_deg = 0.0;
    double until_m = 1e5;
};
bool ParseWind(const std::string& text, WindSpec& wind);

}  // namespace balcli

#endif  // BALCALC_CLI_APP_H
