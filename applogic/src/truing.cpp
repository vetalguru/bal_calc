#include <ballistics/applogic/truing.h>
#include <ballistics/bc.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>

#include <algorithm>
#include <cmath>
#include <utility>

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

}  // namespace

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
        out.error = "Log at least one hit to true the rifle and cartridge.";
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
namespace {

// Elevation and Mach at a logged shot with the profile as given (its DSF
// table included).
std::optional<std::pair<double, double>> PredictWithMach(const storage::LoadedProfile& p,
                                                         const DopeRecord& d) {
    auto sol = storage::Solve(p, ConditionsOf(d), d.range_m + 1.0);
    if (!sol) {
        return std::nullopt;
    }
    const auto pt = sol.value().trajectory.AtSlantRange(d.range_m);
    if (!pt) {
        return std::nullopt;
    }
    return std::make_pair(pt->hold_elevation_rad, pt->mach);
}

// The sequential fit matches each point's own shot exactly, so close points
// see-saw. This refines all factors at once (Levenberg-Marquardt) on every
// transonic shot, with a mild penalty on jumps between neighbouring points:
// 0.1 of factor weighs like a 0.01 mrad miss.
void RefineDsf(const storage::LoadedProfile& bare, const std::vector<DopeRecord>& log,
               const std::vector<DsfShot>& shots, std::vector<DsfPoint>& points) {
    constexpr double kSmooth = 1e-4;  // rad per unit of factor difference
    std::vector<const DopeRecord*> used;
    for (const DsfShot& s : shots) {
        if (s.mach < kDsfMaxMach) {
            for (const DopeRecord& d : log) {
                if (d.id == s.shot_id) {
                    used.push_back(&d);
                }
            }
        }
    }
    // points[0] is the anchor; the rest are in fitting order (Mach falling).
    const std::size_t n = points.size() - 1;
    auto residuals = [&](const std::vector<DsfPoint>& pts, std::vector<double>& r) {
        storage::LoadedProfile p = bare;
        p.profile.dsf = pts;
        std::sort(p.profile.dsf.begin(), p.profile.dsf.end(),
                  [](const DsfPoint& a, const DsfPoint& b) { return a.mach < b.mach; });
        r.clear();
        for (const DopeRecord* d : used) {
            const auto pred = PredictWithMach(p, *d);
            if (!pred) {
                return false;
            }
            r.push_back(pred->first - d->observed_elevation_rad);
        }
        for (std::size_t k = 1; k < pts.size(); ++k) {
            r.push_back(kSmooth * (pts[k].factor - pts[k - 1].factor));
        }
        return true;
    };
    std::vector<double> r;
    if (!residuals(points, r)) {
        return;
    }
    double cost = 0.0;
    for (double x : r) {
        cost += x * x;
    }
    double lambda = 1e-3;
    for (int iter = 0; iter < 20; ++iter) {
        // Jacobian by forward differences, one column per fitted factor.
        constexpr double kH = 1e-4;
        std::vector<std::vector<double>> jac(n);
        for (std::size_t j = 0; j < n; ++j) {
            std::vector<DsfPoint> q = points;
            q[j + 1].factor += kH;
            std::vector<double> rq;
            if (!residuals(q, rq)) {
                return;
            }
            jac[j].resize(r.size());
            for (std::size_t i = 0; i < r.size(); ++i) {
                jac[j][i] = (rq[i] - r[i]) / kH;
            }
        }
        // Normal equations (J^T J + lambda diag) dx = -J^T r.
        std::vector<std::vector<double>> a(n, std::vector<double>(n + 1, 0.0));
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t k = 0; k < n; ++k) {
                for (std::size_t i = 0; i < r.size(); ++i) {
                    a[j][k] += jac[j][i] * jac[k][i];
                }
            }
            for (std::size_t i = 0; i < r.size(); ++i) {
                a[j][n] -= jac[j][i] * r[i];
            }
        }
        bool improved = false;
        for (int tries = 0; tries < 8 && !improved; ++tries) {
            std::vector<std::vector<double>> m = a;
            for (std::size_t j = 0; j < n; ++j) {
                m[j][j] *= 1.0 + lambda;
            }
            // Gaussian elimination with partial pivoting.
            bool singular = false;
            for (std::size_t c = 0; c < n && !singular; ++c) {
                std::size_t piv = c;
                for (std::size_t row = c + 1; row < n; ++row) {
                    if (std::fabs(m[row][c]) > std::fabs(m[piv][c])) {
                        piv = row;
                    }
                }
                std::swap(m[c], m[piv]);
                if (std::fabs(m[c][c]) < 1e-30) {
                    singular = true;
                    break;
                }
                for (std::size_t row = 0; row < n; ++row) {
                    if (row != c) {
                        const double k = m[row][c] / m[c][c];
                        for (std::size_t col = c; col <= n; ++col) {
                            m[row][col] -= k * m[c][col];
                        }
                    }
                }
            }
            if (singular) {
                lambda *= 10;
                continue;
            }
            std::vector<DsfPoint> trial = points;
            for (std::size_t j = 0; j < n; ++j) {
                trial[j + 1].factor = std::clamp(points[j + 1].factor + m[j][n] / m[j][j],
                                                 kDsfMinFactor, kDsfMaxFactor);
            }
            std::vector<double> rt;
            double c2 = 0.0;
            if (residuals(trial, rt)) {
                for (double x : rt) {
                    c2 += x * x;
                }
            }
            if (!rt.empty() && c2 < cost) {
                points = std::move(trial);
                r = std::move(rt);
                const bool done = cost - c2 < 1e-6 * cost;
                cost = c2;
                lambda = std::max(lambda / 10, 1e-9);
                improved = true;
                if (done) {
                    return;
                }
            } else {
                lambda *= 10;
            }
        }
        if (!improved) {
            return;
        }
    }
}

}  // namespace

DsfResult ComputeDsf(storage::Database& db, Id profile_id) {
    DsfResult out;
    auto loaded = storage::LoadProfile(db, profile_id);
    if (!loaded) {
        out.error = loaded.error().message;
        return out;
    }
    const storage::LoadedProfile& current = loaded.value();
    auto shots = ListShots(db, profile_id);
    if (!shots) {
        out.error = shots.error().message;
        return out;
    }
    // Machs come from the profile without a DSF: the table being fitted
    // must not move them.
    storage::LoadedProfile bare = current;
    bare.profile.dsf.clear();
    for (const DopeRecord& d : shots.value()) {
        if (!d.use_for_truing) {
            continue;
        }
        const auto before = PredictWithMach(current, d);
        const auto plain = PredictWithMach(bare, d);
        if (!before || !plain) {
            out.error = "The bullet does not reach one of the logged ranges.";
            return out;
        }
        out.shots.push_back({d.id, d.range_m, plain->second, d.observed_elevation_rad,
                             before->first, before->first, false});
    }
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < out.shots.size(); ++i) {
        if (out.shots[i].mach < kDsfMaxMach) {
            order.push_back(i);
        }
    }
    if (order.empty()) {
        out.error = "Log hits where the bullet is slower than Mach 1.3 at the target.";
        return out;
    }
    std::sort(order.begin(), order.end(),
              [&](std::size_t a, std::size_t b) { return out.shots[a].mach > out.shots[b].mach; });

    std::vector<DsfPoint> points = {{kDsfAnchorMach, 1.0}};
    for (std::size_t i : order) {
        DsfShot& s = out.shots[i];
        if (points.back().mach - s.mach < kDsfMinMachStep) {
            continue;
        }
        const DopeRecord* d = nullptr;
        for (const DopeRecord& r : shots.value()) {
            if (r.id == s.shot_id) {
                d = &r;
            }
        }
        // More drag, more elevation: bisection on the factor.
        auto miss = [&](double f) {
            storage::LoadedProfile p = bare;
            p.profile.dsf = points;
            p.profile.dsf.push_back({s.mach, f});
            const auto pred = PredictWithMach(p, *d);
            return pred ? pred->first - s.observed_rad : 0.0;
        };
        double lo = kDsfMinFactor, hi = kDsfMaxFactor;
        if (miss(lo) >= 0.0) {
            hi = lo;  // a start for the joint fit; judged after it
        } else if (miss(hi) <= 0.0) {
            lo = hi;
        }
        for (int k = 0; k < 60 && hi - lo > 1e-6; ++k) {
            const double mid = 0.5 * (lo + hi);
            (miss(mid) < 0.0 ? lo : hi) = mid;
        }
        points.push_back({s.mach, 0.5 * (lo + hi)});
        s.used = true;
    }
    RefineDsf(bare, shots.value(), out.shots, points);
    std::sort(points.begin(), points.end(),
              [](const DsfPoint& a, const DsfPoint& b) { return a.mach < b.mach; });

    storage::LoadedProfile fitted = bare;
    fitted.profile.dsf = points;
    std::vector<double> before, after;
    for (DsfShot& s : out.shots) {
        for (const DopeRecord& r : shots.value()) {
            if (r.id == s.shot_id) {
                if (const auto pred = PredictWithMach(fitted, r)) {
                    s.predicted_after_rad = pred->first;
                }
            }
        }
        before.push_back(s.predicted_before_rad - s.observed_rad);
        after.push_back(s.predicted_after_rad - s.observed_rad);
        s.limited = s.mach < kDsfMaxMach &&
                    std::fabs(s.predicted_after_rad - s.observed_rad) > kDsfMissTolerance;
    }
    if (std::all_of(order.begin(), order.end(),
                    [&](std::size_t i) { return out.shots[i].limited; })) {
        out.error = "The DSF alone cannot explain these hits: true the velocity and drag first.";
        return out;
    }
    out.rms_before_rad = Rms(before);
    out.rms_after_rad = Rms(after);
    out.points = std::move(points);
    out.ok = true;
    return out;
}

Status SetDsf(storage::Database& db, Id profile_id, std::vector<DsfPoint> points) {
    std::sort(points.begin(), points.end(),
              [](const DsfPoint& a, const DsfPoint& b) { return a.mach < b.mach; });
    for (std::size_t i = 0; i < points.size(); ++i) {
        const DsfPoint& p = points[i];
        if (!(p.mach > 0.0 && p.mach <= 5.0) ||
            !(p.factor >= kDsfMinFactor && p.factor <= kDsfMaxFactor)) {
            return Error(
                ErrorCode::kConstraint, 0,
                "Each DSF point needs a Mach between 0 and 5 and a factor between 0.5 and 2.");
        }
        if (i > 0 && p.mach - points[i - 1].mach < 1e-3) {
            return Error(ErrorCode::kConstraint, 0, "Two DSF points have the same Mach.");
        }
    }
    auto p = Repository<storage::ProfileRecord>(db).Get(profile_id);
    if (!p) {
        return p.error();
    }
    if (!p.value()) {
        return Error(ErrorCode::kNotFound, 0, "profile not found");
    }
    storage::ProfileRecord r = std::move(*p.value());
    r.dsf = std::move(points);
    if (auto id = Repository<storage::ProfileRecord>(db).Save(r); !id) {
        return id.error();
    }
    return sqlite_manager::Ok();
}
namespace {

std::optional<DragTableId> TableNamed(const std::string& name) {
    for (DragTableId id :
         {DragTableId::kG1, DragTableId::kG2, DragTableId::kG5, DragTableId::kG6, DragTableId::kG7,
          DragTableId::kG8, DragTableId::kGI, DragTableId::kGS, DragTableId::kRA4}) {
        if (name == DragTableName(id)) {
            return id;
        }
    }
    return std::nullopt;
}

BcResult BcOrError(std::optional<double> bc, const char* error) {
    BcResult r;
    if (bc) {
        r.ok = true;
        r.bc = *bc;
    } else {
        r.error = error;
    }
    return r;
}

}  // namespace

BcResult BcFromChronograph(const std::string& table, double v_near_mps, double v_far_mps,
                           double distance_m, const SessionConditions& s) {
    const auto id = TableNamed(table);
    if (!id) {
        return BcOrError(std::nullopt, "Unknown drag table.");
    }
    if (!(distance_m > 0.0) || !(v_far_mps > 0.0) || !(v_near_mps > v_far_mps)) {
        return BcOrError(std::nullopt, "Enter the distance and two velocities, the far one lower.");
    }
    return BcOrError(
        BcFromVelocities(*id, v_near_mps, v_far_mps, distance_m, ToConditions(s).atmosphere),
        "No BC between 0.02 and 2 gives these measurements.");
}

BcResult BcFromHit(const storage::LoadedProfile& profile, const std::string& table, double range_m,
                   double elevation_rad, const SessionConditions& s) {
    const auto id = TableNamed(table);
    if (!id) {
        return BcOrError(std::nullopt, "Unknown drag table.");
    }
    if (!(range_m > 0.0)) {
        return BcOrError(std::nullopt, "Enter a target range.");
    }
    storage::LoadedProfile p = profile;
    p.bullet.drag_kind = storage::kDragKindBc;
    p.bullet.drag_table = DragTableName(*id);
    p.curve.reset();
    const storage::ConditionsRecord c = ToConditions(s);
    // A higher BC needs less elevation: fit minus the correction.
    return BcOrError(FitBc(
                         [&](double bc) -> std::optional<double> {
                             p.bullet.bc = bc;
                             auto sol = storage::Solve(p, c, range_m + 1.0);
                             if (!sol) {
                                 return std::nullopt;
                             }
                             const auto pt = sol.value().trajectory.AtSlantRange(range_m);
                             if (!pt) {
                                 return std::nullopt;
                             }
                             return -pt->hold_elevation_rad;
                         },
                         -elevation_rad),
                     "No BC between 0.02 and 2 gives this correction.");
}

}  // namespace ballistics::applogic
