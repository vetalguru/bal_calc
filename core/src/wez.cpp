#include <ballistics/units.h>
#include <ballistics/wez.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace ballistics {

namespace {

// The air that moves: speed and the direction it blows from, as a vector
// across (right) and along (toward the shooter) the line of fire.
std::pair<double, double> WindVector(double speed, double from_rad) {
    return {speed * std::sin(from_rad), speed * std::cos(from_rad)};
}

// Each zone's wind plus `crosswind` m/s blowing from the right (negative:
// from the left); no wind becomes that crosswind everywhere.
std::vector<WindZone> WithCrosswind(std::vector<WindZone> zones, double crosswind) {
    if (zones.empty()) {
        zones.push_back({1e9, 0.0, 0.0, 0.0});
    }
    for (WindZone& z : zones) {
        auto [x, y] = WindVector(z.speed_mps, z.from_rad);
        x += crosswind;
        z.speed_mps = std::hypot(x, y);
        z.from_rad = std::atan2(x, y);
    }
    return zones;
}

double NormalCdf(double x) { return 0.5 * std::erfc(-x / std::sqrt(2.0)); }

// The target's horizontal extent at height `y` (m from the aim point), as
// intervals; empty above or below it.
std::vector<std::pair<double, double>> Intervals(const Target& t, double y) {
    const double w = t.width_m;
    const double h = t.height_m;
    switch (t.kind) {
        case Target::Kind::kRectangle:
            if (std::abs(y) <= h / 2) {
                return {{-w / 2, w / 2}};
            }
            return {};
        case Target::Kind::kEllipse: {
            const double u = 2 * y / h;
            if (std::abs(u) > 1) {
                return {};
            }
            const double half = w / 2 * std::sqrt(1 - u * u);
            return {{-half, half}};
        }
        case Target::Kind::kFigure:
            // A chest (full width, the lower 70 %) and a head (40 % wide,
            // the upper 30 %), aimed at the centre of the chest.
            if (y >= -0.35 * h && y <= 0.35 * h) {
                return {{-w / 2, w / 2}};
            }
            if (y > 0.35 * h && y <= 0.65 * h) {
                return {{-0.2 * w, 0.2 * w}};
            }
            return {};
    }
    return {};
}

std::pair<double, double> VerticalExtent(const Target& t) {
    if (t.kind == Target::Kind::kFigure) {
        return {-0.35 * t.height_m, 0.65 * t.height_m};
    }
    return {-t.height_m / 2, t.height_m / 2};
}

}  // namespace

WezModel::WezModel(const Shot& shot, const ErrorSources& e, double max_slant_range_m)
    : nominal_(Fly(shot, max_slant_range_m)), errors_(e) {
    // Every source but the range (which needs no flight) and the
    // dispersion: the shot with it at +1 and -1 sigma.
    std::vector<std::pair<std::string, std::function<void(Shot&, double)>>> sources;
    if (e.wind_speed_mps > 0) {
        sources.emplace_back("windSpeed", [&e](Shot& s, double k) {
            s.winds = WithCrosswind(s.winds, k * e.wind_speed_mps);
        });
    }
    if (e.wind_direction_rad > 0 && !shot.winds.empty()) {
        sources.emplace_back("windDirection", [&e](Shot& s, double k) {
            for (WindZone& z : s.winds) {
                z.from_rad += k * e.wind_direction_rad;
            }
        });
    }
    if (e.muzzle_velocity_mps > 0) {
        sources.emplace_back("muzzleVelocity", [&e](Shot& s, double k) {
            s.muzzle_velocity_mps += k * e.muzzle_velocity_mps;
        });
    }
    if (e.drag_fraction > 0) {
        sources.emplace_back(
            "drag", [&e](Shot& s, double k) { s.drag = s.drag.Scaled(1.0 + k * e.drag_fraction); });
    }
    if (e.temperature_k > 0) {
        sources.emplace_back("temperature", [&e](Shot& s, double k) {
            s.atmosphere.temperature_k += k * e.temperature_k;
        });
    }
    if (e.pressure_pa > 0) {
        sources.emplace_back(
            "pressure", [&e](Shot& s, double k) { s.atmosphere.pressure_pa += k * e.pressure_pa; });
    }
    if (e.humidity > 0) {
        sources.emplace_back("humidity", [&e](Shot& s, double k) {
            s.atmosphere.humidity = std::clamp(s.atmosphere.humidity + k * e.humidity, 0.0, 1.0);
        });
    }
    if (e.look_angle_rad > 0) {
        sources.emplace_back("lookAngle",
                             [&e](Shot& s, double k) { s.look_angle_rad += k * e.look_angle_rad; });
    }
    if (e.cant_rad > 0) {
        sources.emplace_back("cant", [&e](Shot& s, double k) { s.cant_rad += k * e.cant_rad; });
    }
    if (e.azimuth_rad > 0 && shot.azimuth_rad) {
        sources.emplace_back("azimuth",
                             [&e](Shot& s, double k) { *s.azimuth_rad += k * e.azimuth_rad; });
    }
    if (e.latitude_rad > 0 && shot.latitude_rad) {
        sources.emplace_back("latitude",
                             [&e](Shot& s, double k) { *s.latitude_rad += k * e.latitude_rad; });
    }
    for (auto& [name, apply] : sources) {
        Shot plus = shot;
        Shot minus = shot;
        apply(plus, 1.0);
        apply(minus, -1.0);
        perturbed_.push_back({name, Fly(plus, max_slant_range_m), Fly(minus, max_slant_range_m)});
    }
}

Spread WezModel::At(double slant_range_m, bool* ok) const {
    Spread out;
    const auto here = nominal_.AtSlantRange(slant_range_m);
    if (ok) {
        *ok = here.has_value();
    }
    if (!here) {
        return out;
    }
    // Aimed for the range the shooter believes; the target stands 1 sigma
    // further: the path's slope over that distance.
    if (errors_.range_m > 0) {
        const double h = std::min(errors_.range_m, 0.5 * slant_range_m);
        const auto far = nominal_.AtSlantRange(slant_range_m + h);
        const auto near = nominal_.AtSlantRange(slant_range_m - h);
        if (far && near) {
            const double k = errors_.range_m / (2 * h);
            out.parts.push_back({"range", (far->drop_m - near->drop_m) * k,
                                 (far->windage_m - near->windage_m) * k});
        }
    }
    for (const Perturbed& p : perturbed_) {
        const auto a = p.plus.AtSlantRange(slant_range_m);
        const auto b = p.minus.AtSlantRange(slant_range_m);
        if (a && b) {
            out.parts.push_back(
                {p.source, (a->drop_m - b->drop_m) / 2, (a->windage_m - b->windage_m) / 2});
        }
    }
    double uu = 0, rr = 0, ur = 0;
    for (const Spread::Part& part : out.parts) {
        uu += part.up_m * part.up_m;
        rr += part.right_m * part.right_m;
        ur += part.up_m * part.right_m;
    }
    const double d = errors_.dispersion_rad * slant_range_m;
    uu += d * d;
    rr += d * d;
    if (errors_.dispersion_rad > 0) {
        out.parts.push_back({"dispersion", d, d});
    }
    out.sigma_up_m = std::sqrt(uu);
    out.sigma_right_m = std::sqrt(rr);
    out.correlation = (uu > 0 && rr > 0) ? ur / std::sqrt(uu * rr) : 0.0;
    std::sort(out.parts.begin(), out.parts.end(), [](const Spread::Part& a, const Spread::Part& b) {
        return std::hypot(a.up_m, a.right_m) > std::hypot(b.up_m, b.right_m);
    });
    return out;
}

double HitProbability(const Spread& s, const Target& target) {
    // Exact across (the normal of the horizontal error given the vertical
    // one), Simpson's rule up and down, fine enough for a narrow spread.
    constexpr double kTiny = 1e-9;
    const double su = std::max(s.sigma_up_m, kTiny);
    const double sr = std::max(s.sigma_right_m, kTiny);
    const double rho = std::clamp(s.correlation, -0.999999, 0.999999);
    const double sx = sr * std::sqrt(1 - rho * rho);
    const auto [y0, y1] = VerticalExtent(target);
    // Only where the vertical density is not negligible.
    const double lo = std::max(y0, -8 * su);
    const double hi = std::min(y1, 8 * su);
    if (hi <= lo) {
        return 0.0;
    }
    int n = static_cast<int>(std::ceil((hi - lo) / (su / 16)));
    n = std::clamp(n + (n % 2), 200, 20000);
    const double step = (hi - lo) / n;
    auto f = [&](double y) {
        const double mean = rho * sr / su * y;
        double across = 0.0;
        for (const auto& [x0, x1] : Intervals(target, y)) {
            across += NormalCdf((x1 - mean) / sx) - NormalCdf((x0 - mean) / sx);
        }
        return across * std::exp(-0.5 * (y / su) * (y / su)) / (su * std::sqrt(2 * units::kPi));
    };
    double sum = f(lo) + f(hi);
    for (int i = 1; i < n; ++i) {
        sum += f(lo + i * step) * (i % 2 ? 4 : 2);
    }
    return std::clamp(sum * step / 3, 0.0, 1.0);
}

int ShotsToHit(double p, double confidence) {
    if (!(p > 0.0)) {
        return 0;
    }
    if (p >= 1.0) {
        return 1;
    }
    return static_cast<int>(std::ceil(std::log(1.0 - confidence) / std::log(1.0 - p) - 1e-9));
}

}  // namespace ballistics
