#ifndef BALLISTICS_STORAGE_SOLUTION_H
#define BALLISTICS_STORAGE_SOLUTION_H

#include <optional>

#include <ballistics/solver.h>
#include <ballistics/storage/database.h>
#include <ballistics/storage/records.h>

// Turning stored records into a firing solution: profile + conditions ->
// zeroed Shot -> Trajectory, and angles -> scope clicks.
namespace ballistics::storage {

// A profile with everything it references.
struct LoadedProfile {
    ProfileRecord profile;
    RifleRecord rifle;
    std::optional<ScopeRecord> scope;
    CartridgeRecord cartridge;
    BulletRecord bullet;
    std::optional<DragCurveRecord> curve; // for a bullet with its own Cd curve
};

Result<LoadedProfile> LoadProfile(Database& db, Id profile_id);

// Drag model of a bullet (curve required for kDragKindCurve). Fails on
// inconsistent data (unknown table, missing BC, bad curve).
Result<DragModel> MakeDragModel(const BulletRecord& bullet, const DragCurveRecord* curve);

// Muzzle velocity at a powder temperature (cartridge table or
// coefficient), times the profile's truing scale.
double MuzzleVelocity(const CartridgeRecord& cartridge, double powder_temp_k,
                      double velocity_scale = 1.0);

struct Solution {
    Shot shot;          // as fired: conditions applied, zero angles set
    ZeroResult zero;    // bore angles found at the rifle's zero conditions
    Trajectory trajectory;
};

// Zeroes the rifle at its zero range and conditions (level,
// still air), then flies the shot in `conditions` out to `max_range_m`
// along the LOS.
Result<Solution> Solve(const LoadedProfile& p, const ConditionsRecord& conditions,
                       double max_range_m, const SolverOptions& options = {});

// An angle in scope clicks (rounded to the nearest click when `round`).
double ToClicks(double angle_rad, double click_rad, bool round = true);

} // namespace ballistics::storage

#endif // BALLISTICS_STORAGE_SOLUTION_H
