#include <ballistics/applogic/wez.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>
#include <sqlite_manager/transaction.h>

#include <algorithm>
#include <cstdio>
#include <optional>

namespace ballistics::applogic {

namespace {

constexpr const char* kPrefix = "wez.";

struct NumField {
    const char* key;
    double WezSettings::* value;
};

const NumField kNumFields[] = {
    {"range_m", &WezSettings::range_m},
    {"wind_speed_mps", &WezSettings::wind_speed_mps},
    {"wind_direction_deg", &WezSettings::wind_direction_deg},
    {"muzzle_velocity_mps", &WezSettings::muzzle_velocity_mps},
    {"bc_percent", &WezSettings::bc_percent},
    {"temperature_c", &WezSettings::temperature_c},
    {"pressure_hpa", &WezSettings::pressure_hpa},
    {"humidity_pct", &WezSettings::humidity_pct},
    {"look_angle_deg", &WezSettings::look_angle_deg},
    {"cant_deg", &WezSettings::cant_deg},
    {"azimuth_deg", &WezSettings::azimuth_deg},
    {"latitude_deg", &WezSettings::latitude_deg},
    {"group_moa", &WezSettings::group_moa},
    {"target_width_cm", &WezSettings::target_width_cm},
    {"target_height_cm", &WezSettings::target_height_cm},
};

}  // namespace

Result<WezSettings> LoadWezSettings(storage::Database& db) {
    WezSettings w;
    for (const NumField& f : kNumFields) {
        auto v = storage::GetSetting(db, std::string(kPrefix) + f.key);
        if (!v) {
            return v.error();
        }
        if (v.value()) {
            try {
                w.*f.value = std::stod(*v.value());
            } catch (...) {
            }
        }
    }
    auto kind = storage::GetSetting(db, std::string(kPrefix) + "target_kind");
    if (!kind) {
        return kind.error();
    }
    if (kind.value()) {
        w.target_kind = *kind.value();
    }
    return w;
}

Status SaveWezSettings(storage::Database& db, const WezSettings& w) {
    auto txn = sqlite_manager::Transaction::Begin(db.connection());
    if (!txn) {
        return txn.error();
    }
    for (const NumField& f : kNumFields) {
        char buf[40];
        std::snprintf(buf, sizeof(buf), "%.17g", w.*f.value);
        if (Status s = storage::SetSetting(db, std::string(kPrefix) + f.key, buf); !s) {
            return s;
        }
    }
    if (Status s = storage::SetSetting(db, std::string(kPrefix) + "target_kind", w.target_kind);
        !s) {
        return s;
    }
    return txn.value().Commit();
}

ErrorSources ToErrorSources(const WezSettings& w) {
    auto nonneg = [](double v) { return std::max(0.0, v); };
    ErrorSources e;
    e.range_m = nonneg(w.range_m);
    e.wind_speed_mps = nonneg(w.wind_speed_mps);
    e.wind_direction_rad = units::DegToRad(nonneg(w.wind_direction_deg));
    e.muzzle_velocity_mps = nonneg(w.muzzle_velocity_mps);
    e.drag_fraction = nonneg(w.bc_percent) / 100.0;
    e.temperature_k = nonneg(w.temperature_c);
    e.pressure_pa = nonneg(w.pressure_hpa) * 100.0;
    e.humidity = nonneg(w.humidity_pct) / 100.0;
    e.look_angle_rad = units::DegToRad(nonneg(w.look_angle_deg));
    e.cant_rad = units::DegToRad(nonneg(w.cant_deg));
    e.azimuth_rad = units::DegToRad(nonneg(w.azimuth_deg));
    e.latitude_rad = units::DegToRad(nonneg(w.latitude_deg));
    e.dispersion_rad = units::MoaToRad(nonneg(w.group_moa)) / kGroupSpreadPerSigma;
    return e;
}

Target ToTarget(const WezSettings& w) {
    Target t;
    t.kind = w.target_kind == "ellipse"  ? Target::Kind::kEllipse
             : w.target_kind == "figure" ? Target::Kind::kFigure
                                         : Target::Kind::kRectangle;
    t.width_m = std::max(0.001, w.target_width_cm / 100.0);
    t.height_m = std::max(0.001, w.target_height_cm / 100.0);
    return t;
}

WezResult ComputeWez(const storage::LoadedProfile& profile, const SessionConditions& s,
                     const WezSettings& w, double to_m, double step_m) {
    WezResult out;
    if (!(step_m > 0.0) || !(to_m >= step_m) || to_m / step_m > 500.0) {
        out.error = "Check the table range and step.";
        return out;
    }
    // Far enough for the target 4 sigma of range error beyond the farthest row.
    const double reach = std::max(to_m, s.target_range_m) + std::max(0.0, w.range_m) * 4 + 50.0;
    auto sol = storage::Solve(profile, ToConditions(s), reach);
    if (!sol) {
        out.error = sol.error().message;
        return out;
    }
    const WezModel model(sol.value().shot, ToErrorSources(w), reach);
    const Target target = ToTarget(w);
    auto row = [&](double r, Spread* spread) -> std::optional<WezRow> {
        bool ok = false;
        const Spread sp = model.At(r, &ok);
        if (!ok) {
            return std::nullopt;
        }
        if (spread) {
            *spread = sp;
        }
        return WezRow{r, HitProbability(sp, target), sp.sigma_up_m * 100.0,
                      sp.sigma_right_m * 100.0};
    };
    for (double r = step_m; r <= to_m + 1e-9; r += step_m) {
        const auto rr = row(r, nullptr);
        if (!rr) {
            break;
        }
        out.rows.push_back(*rr);
    }
    Spread at;
    const auto here = row(s.target_range_m, &at);
    if (!here) {
        out.error = "The bullet does not reach this range.";
        return out;
    }
    out.at_target = *here;
    out.parts = at.parts;
    out.shots_50 = ShotsToHit(here->probability, 0.5);
    out.shots_80 = ShotsToHit(here->probability, 0.8);
    out.shots_95 = ShotsToHit(here->probability, 0.95);
    out.ok = true;
    return out;
}

}  // namespace ballistics::applogic
