#include <ballistics/drag.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <ballistics/units.h>

#include "standard_drag_tables.h"

namespace ballistics {

namespace {

int Sign(double v) { return (v > 0.0) - (v < 0.0); }

// Three-point end slope, limited to keep the end segment monotone.
double EndSlope(double h0, double h1, double d0, double d1) {
    double m = ((2.0 * h0 + h1) * d0 - h0 * d1) / (h0 + h1);
    if (Sign(m) != Sign(d0)) {
        m = 0.0;
    } else if (std::fabs(m) > 3.0 * std::fabs(d0)) {
        m = 3.0 * d0;
    }
    return m;
}

} // namespace

const char* DragTableName(DragTableId id) {
    switch (id) {
        case DragTableId::kG1: return "G1";
        case DragTableId::kG2: return "G2";
        case DragTableId::kG5: return "G5";
        case DragTableId::kG6: return "G6";
        case DragTableId::kG7: return "G7";
        case DragTableId::kG8: return "G8";
        case DragTableId::kGI: return "GI";
        case DragTableId::kGS: return "GS";
        case DragTableId::kRA4: return "RA4";
    }
    return "?";
}

std::vector<DragPoint> StandardDragTable(DragTableId id) {
    const auto t = detail::StandardTable(id);
    return {t.data, t.data + t.size};
}

DragCurve::DragCurve(std::vector<DragPoint> points) : points_(std::move(points)) {
    const std::size_t n = points_.size();
    if (n < 2) {
        throw std::invalid_argument("drag curve needs at least two points");
    }
    std::vector<double> h(n - 1), delta(n - 1);
    for (std::size_t i = 0; i + 1 < n; ++i) {
        h[i] = points_[i + 1].mach - points_[i].mach;
        if (!(h[i] > 0.0)) {
            throw std::invalid_argument("drag curve Mach values must strictly increase");
        }
        delta[i] = (points_[i + 1].cd - points_[i].cd) / h[i];
    }

    std::vector<double> m(n);
    if (n == 2) {
        m[0] = m[1] = delta[0];
    } else {
        for (std::size_t i = 1; i + 1 < n; ++i) {
            const double d0 = delta[i - 1];
            const double d1 = delta[i];
            if (d0 == 0.0 || d1 == 0.0 || Sign(d0) != Sign(d1)) {
                m[i] = 0.0;
            } else {
                const double w1 = 2.0 * h[i] + h[i - 1];
                const double w2 = h[i] + 2.0 * h[i - 1];
                m[i] = (w1 + w2) / (w1 / d0 + w2 / d1);
            }
        }
        m[0] = EndSlope(h[0], h[1], delta[0], delta[1]);
        m[n - 1] = EndSlope(h[n - 2], h[n - 3], delta[n - 2], delta[n - 3]);
    }

    x_.resize(n);
    a_.resize(n - 1);
    b_.resize(n - 1);
    c_.resize(n - 1);
    d_.resize(n - 1);
    for (std::size_t i = 0; i < n; ++i) {
        x_[i] = points_[i].mach;
    }
    for (std::size_t i = 0; i + 1 < n; ++i) {
        a_[i] = points_[i].cd;
        b_[i] = m[i];
        c_[i] = (3.0 * delta[i] - 2.0 * m[i] - m[i + 1]) / h[i];
        d_[i] = (m[i] + m[i + 1] - 2.0 * delta[i]) / (h[i] * h[i]);
    }
}

double DragCurve::Cd(double mach) const {
    const std::size_t n = x_.size();
    if (mach <= x_.front()) {
        return a_.front();
    }
    if (mach >= x_.back()) {
        return points_.back().cd;
    }
    const auto it = std::upper_bound(x_.begin(), x_.end(), mach);
    const std::size_t i = std::min(static_cast<std::size_t>(it - x_.begin()) - 1, n - 2);
    const double dx = mach - x_[i];
    return a_[i] + dx * (b_[i] + dx * (c_[i] + dx * d_[i]));
}

DragModel DragModel::FromBc(DragTableId table, double bc_lb_in2) {
    if (!(bc_lb_in2 > 0.0)) {
        throw std::invalid_argument("ballistic coefficient must be positive");
    }
    DragModel m;
    m.curve_ = DragCurve(StandardDragTable(table));
    m.bc_kg_m2_ = units::BcToSi(bc_lb_in2);
    return m;
}

DragModel DragModel::FromMultiBc(DragTableId table, std::vector<BcPoint> points) {
    if (points.empty()) {
        throw std::invalid_argument("at least one BC point is required");
    }
    // Speed of sound the BC velocities refer to: dry air at 15 C.
    constexpr double kReferenceSoundSpeed = 340.294;
    std::vector<std::pair<double, double>> bc_by_mach;
    for (const BcPoint& p : points) {
        if (!(p.bc_lb_in2 > 0.0) || !(p.velocity_mps > 0.0)) {
            throw std::invalid_argument("BC points need positive BC and velocity");
        }
        bc_by_mach.emplace_back(p.velocity_mps / kReferenceSoundSpeed, p.bc_lb_in2);
    }
    std::sort(bc_by_mach.begin(), bc_by_mach.end());
    auto bc_at = [&](double mach) {
        if (mach <= bc_by_mach.front().first) {
            return bc_by_mach.front().second;
        }
        if (mach >= bc_by_mach.back().first) {
            return bc_by_mach.back().second;
        }
        std::size_t i = 0;
        while (bc_by_mach[i + 1].first < mach) {
            ++i;
        }
        const auto& [m0, b0] = bc_by_mach[i];
        const auto& [m1, b1] = bc_by_mach[i + 1];
        return b0 + (b1 - b0) * (mach - m0) / (m1 - m0);
    };

    // Fold the BC into the curve: Cd_ref(M) / BC(M), with a unit BC.
    std::vector<DragPoint> curve = StandardDragTable(table);
    for (DragPoint& p : curve) {
        p.cd /= bc_at(p.mach);
    }
    DragModel m;
    m.curve_ = DragCurve(std::move(curve));
    m.bc_kg_m2_ = units::BcToSi(1.0);
    return m;
}

DragModel DragModel::FromCurve(std::vector<DragPoint> curve, double mass_kg, double diameter_m,
                               double form_factor) {
    if (!(mass_kg > 0.0) || !(diameter_m > 0.0) || !(form_factor > 0.0)) {
        throw std::invalid_argument("mass, diameter and form factor must be positive");
    }
    DragModel m;
    m.curve_ = DragCurve(std::move(curve));
    m.bc_kg_m2_ = mass_kg / (diameter_m * diameter_m) / form_factor;
    return m;
}

double DragModel::Coefficient(double mach) const {
    return units::kPi / 8.0 * curve_.Cd(mach) / bc_kg_m2_;
}

} // namespace ballistics
