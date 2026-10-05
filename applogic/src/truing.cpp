#include <ballistics/applogic/truing.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>

namespace ballistics::applogic {

namespace {

using sqlite_manager::Error;
using sqlite_manager::ErrorCode;
using storage::Repository;

constexpr double kMinVelocityScale = 0.8;
constexpr double kMaxVelocityScale = 1.2;
constexpr double kMinDragScale = 0.7;
constexpr double kMaxDragScale = 1.4;

storage::ConditionsRecord ConditionsOf(const DopeRecord& d) {
    storage::ConditionsRecord c;
    c.atmosphere = d.atmosphere;
    c.powder_temp_k = d.powder_temp_k;
    c.look_angle_rad = d.look_angle_rad;
    return c;
}

// Elevation correction the model predicts for a logged shot with the given
// scales; nullopt if the solution fails.
std::optional<double> Predict(storage::LoadedProfile p, const DopeRecord& d, double velocity_scale,
                              double drag_scale) {
    p.profile.velocity_scale = velocity_scale;
    p.profile.drag_scale = drag_scale;
    auto sol = storage::Solve(p, ConditionsOf(d), d.range_m + 1.0);
    if (!sol) {
        return std::nullopt;
    }
    const auto pt = sol.value().trajectory.AtSlantRange(d.range_m);
    if (!pt) {
        return std::nullopt;
    }
    return pt->hold_elevation_rad;
}

double Rms(const std::vector<double>& r) {
    double s = 0.0;
    for (double x : r) {
        s += x * x;
    }
    return r.empty() ? 0.0 : std::sqrt(s / static_cast<double>(r.size()));
}

} // namespace

Result<std::vector<DopeRecord>> ListShots(storage::Database& db, Id profile_id) {
    auto all = Repository<DopeRecord>(db).List();
    if (!all) {
        return all.error();
    }
    std::vector<DopeRecord> out;
    for (DopeRecord& d : all.value()) {
        if (d.profile_id == profile_id) {
            out.push_back(std::move(d));
        }
    }
    std::sort(out.begin(), out.end(),
              [](const DopeRecord& a, const DopeRecord& b) { return a.range_m < b.range_m; });
    return out;
}

Result<Id> LogShot(storage::Database& db, Id profile_id, const SessionConditions& s, double range_m,
                   double observed_elevation_rad, std::optional<double> observed_windage_rad,
                   const std::string& notes) {
    if (!(range_m > 0.0)) {
        return Error(ErrorCode::kConstraint, 0, "Enter a target range.");
    }
    auto p = storage::LoadProfile(db, profile_id);
    if (!p) {
        return p.error();
    }
    const storage::ConditionsRecord c = ToConditions(s);
    DopeRecord d;
    d.profile_id = profile_id;
    d.range_m = range_m;
    d.observed_elevation_rad = observed_elevation_rad;
    d.observed_windage_rad = observed_windage_rad;
    d.atmosphere = c.atmosphere;
    d.powder_temp_k = c.powder_temp_k;
    d.look_angle_rad = c.look_angle_rad;
    d.notes = notes;
    if (auto sol = storage::Solve(p.value(), c, range_m + 1.0)) {
        if (const auto pt = sol.value().trajectory.AtSlantRange(range_m)) {
            d.predicted_elevation_rad = pt->hold_elevation_rad;
            d.predicted_windage_rad = pt->hold_windage_rad;
        }
    }
    return Repository<DopeRecord>(db).Save(d);
}

Status DeleteShot(storage::Database& db, Id shot_id) {
    return Repository<DopeRecord>(db).Remove(shot_id);
}

Status SetShotUsedForTruing(storage::Database& db, Id shot_id, bool used) {
    auto d = Repository<DopeRecord>(db).Get(shot_id);
    if (!d) {
        return d.error();
    }
    if (!d.value()) {
        return Error(ErrorCode::kNotFound, 0, "shot not found");
    }
    DopeRecord r = std::move(*d.value());
    r.use_for_truing = used;
    if (auto id = Repository<DopeRecord>(db).Save(r); !id) {
        return id.error();
    }
    return sqlite_manager::Ok();
}

TruingResult ComputeTruing(storage::Database& db, Id profile_id) {
    TruingResult out;
    auto loaded = storage::LoadProfile(db, profile_id);
    if (!loaded) {
        out.error = loaded.error().message;
        return out;
    }
    const storage::LoadedProfile& p = loaded.value();
    auto shots = ListShots(db, profile_id);
    if (!shots) {
        out.error = shots.error().message;
        return out;
    }
    std::vector<DopeRecord> used;
    for (const DopeRecord& d : shots.value()) {
        if (d.use_for_truing) {
            used.push_back(d);
        }
    }
    if (used.empty()) {
        out.error = "Log at least one shot to true the profile.";
        return out;
    }

    const double v0 = p.profile.velocity_scale;
    const double d0 = p.profile.drag_scale;
    auto residuals = [&](double v, double d, std::vector<double>& r) {
        r.clear();
        for (const DopeRecord& s : used) {
            const auto pred = Predict(p, s, v, d);
            if (!pred) {
                return false;
            }
            r.push_back(*pred - s.observed_elevation_rad);
        }
        return true;
    };

    std::vector<double> r;
    if (!residuals(v0, d0, r)) {
        out.error = "The bullet does not reach one of the logged ranges.";
        return out;
    }
    out.rms_before_rad = Rms(r);

    // Jacobian by central differences.
    auto jacobian = [&](double v, double d, std::vector<double>& jv, std::vector<double>& jd) {
        constexpr double kH = 1e-4;
        std::vector<double> a, b;
        if (!residuals(v + kH, d, a) || !residuals(v - kH, d, b)) {
            return false;
        }
        jv.resize(a.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            jv[i] = (a[i] - b[i]) / (2 * kH);
        }
        if (!residuals(v, d + kH, a) || !residuals(v, d - kH, b)) {
            return false;
        }
        jd.resize(a.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            jd[i] = (a[i] - b[i]) / (2 * kH);
        }
        return true;
    };

    // Can velocity and drag be told apart? Not with one shot, nor when their
    // effects on the logged shots are (nearly) proportional.
    std::vector<double> jv, jd;
    if (!jacobian(v0, d0, jv, jd)) {
        out.error = "The bullet does not reach one of the logged ranges.";
        return out;
    }
    double vv = 0, dd = 0, vd = 0;
    for (std::size_t i = 0; i < jv.size(); ++i) {
        vv += jv[i] * jv[i];
        dd += jd[i] * jd[i];
        vd += jv[i] * jd[i];
    }
    const double correlation = (vv > 0 && dd > 0) ? std::fabs(vd) / std::sqrt(vv * dd) : 1.0;
    out.drag_fitted = used.size() >= 2 && correlation < 0.995;

    // Levenberg-Marquardt on (velocity, drag) or velocity alone.
    double v = v0, d = d0, lambda = 1e-3;
    double cost = out.rms_before_rad;
    for (int iter = 0; iter < 40; ++iter) {
        if (!jacobian(v, d, jv, jd) || !residuals(v, d, r)) {
            break;
        }
        double a11 = 0, a12 = 0, a22 = 0, g1 = 0, g2 = 0;
        for (std::size_t i = 0; i < r.size(); ++i) {
            a11 += jv[i] * jv[i];
            a12 += jv[i] * jd[i];
            a22 += jd[i] * jd[i];
            g1 += jv[i] * r[i];
            g2 += jd[i] * r[i];
        }
        double dv = 0, ddrag = 0;
        bool improved = false;
        for (int tries = 0; tries < 10 && !improved; ++tries) {
            if (out.drag_fitted) {
                const double b11 = a11 * (1 + lambda), b22 = a22 * (1 + lambda);
                const double det = b11 * b22 - a12 * a12;
                if (det == 0.0) {
                    break;
                }
                dv = -(b22 * g1 - a12 * g2) / det;
                ddrag = -(b11 * g2 - a12 * g1) / det;
            } else {
                dv = a11 > 0 ? -g1 / (a11 * (1 + lambda)) : 0.0;
                ddrag = 0.0;
            }
            const double nv = std::clamp(v + dv, kMinVelocityScale, kMaxVelocityScale);
            const double nd = std::clamp(d + ddrag, kMinDragScale, kMaxDragScale);
            std::vector<double> nr;
            if (residuals(nv, nd, nr) && Rms(nr) < cost) {
                v = nv;
                d = nd;
                cost = Rms(nr);
                lambda = std::max(lambda / 10, 1e-9);
                improved = true;
            } else {
                lambda *= 10;
            }
        }
        if (!improved || (std::fabs(dv) < 1e-7 && std::fabs(ddrag) < 1e-7)) {
            break;
        }
    }

    std::vector<double> after;
    residuals(v, d, after);
    out.velocity_scale = v;
    out.drag_scale = d;
    out.rms_after_rad = Rms(after);
    out.muzzle_velocity_before_mps = p.cartridge.muzzle_velocity_mps * v0;
    out.muzzle_velocity_after_mps = p.cartridge.muzzle_velocity_mps * v;
    std::vector<double> before;
    residuals(v0, d0, before);
    for (std::size_t i = 0; i < used.size(); ++i) {
        out.points.push_back({used[i].id, used[i].range_m, used[i].observed_elevation_rad,
                              used[i].observed_elevation_rad + before[i],
                              used[i].observed_elevation_rad + after[i]});
    }
    out.ok = true;
    return out;
}

Status ApplyTruing(storage::Database& db, Id profile_id, const TruingResult& result) {
    if (!result.ok) {
        return Error(ErrorCode::kMisuse, 0, "Nothing to apply.");
    }
    auto p = Repository<storage::ProfileRecord>(db).Get(profile_id);
    if (!p) {
        return p.error();
    }
    if (!p.value()) {
        return Error(ErrorCode::kNotFound, 0, "profile not found");
    }
    storage::ProfileRecord r = std::move(*p.value());
    r.velocity_scale = result.velocity_scale;
    r.drag_scale = result.drag_scale;
    if (auto id = Repository<storage::ProfileRecord>(db).Save(r); !id) {
        return id.error();
    }
    return sqlite_manager::Ok();
}

Status ResetTruing(storage::Database& db, Id profile_id) {
    TruingResult neutral;
    neutral.ok = true;
    return ApplyTruing(db, profile_id, neutral);
}

} // namespace ballistics::applogic
