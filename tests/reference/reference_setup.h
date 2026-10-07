// Inputs shared by the reference tests; must match CARTRIDGES and
// ATMOSPHERES in generate_reference.py.
#ifndef BALLISTICS_TESTS_REFERENCE_SETUP_H
#define BALLISTICS_TESTS_REFERENCE_SETUP_H

#include <ballistics/solver.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>

#include <string_view>

namespace ballistics::reference {

struct Cartridge {
    const char* name;
    DragTableId table;
    double bc, grains, diameter_in, length_in, v0_mps, sight_cm, zero_m;
};

inline constexpr Cartridge kCartridges[] = {
    {"308_175smk_g7", DragTableId::kG7, 0.243, 175.0, 0.308, 1.240, 800.0, 5.0, 100.0},
    {"65cm_140eldm_g7", DragTableId::kG7, 0.326, 140.0, 0.264, 1.370, 823.0, 4.5, 100.0},
    {"338lm_300hybrid_g7", DragTableId::kG7, 0.419, 300.0, 0.338, 1.700, 838.0, 5.5, 100.0},
    {"50bmg_750amax_g1", DragTableId::kG1, 1.050, 750.0, 0.510, 2.420, 860.0, 7.0, 100.0},
    {"556_m855_g1", DragTableId::kG1, 0.304, 62.0, 0.224, 0.906, 930.0, 6.5, 100.0},
};

inline Atmosphere AtmosphereNamed(std::string_view name) {
    if (name == "mountain_cold_humid") {
        return {1500.0, units::HpaToPa(850.0), units::CToK(-10.0), 0.5};
    }
    if (name == "hot_humid") {
        return {200.0, units::HpaToPa(990.0), units::CToK(35.0), 0.9};
    }
    return {0.0, units::HpaToPa(1013.25), units::CToK(15.0), 0.0};
}

inline const Cartridge& CartridgeNamed(std::string_view name) {
    for (const auto& c : kCartridges) {
        if (name == c.name) {
            return c;
        }
    }
    ADD_FAILURE() << "unknown cartridge " << name;
    return kCartridges[0];
}

// A level shot with the cartridge in the named air, aligned with the
// reference: dry-air speed of sound, no aerodynamic jump.
inline Shot MakeShot(std::string_view cartridge, std::string_view atmosphere) {
    const Cartridge& c = CartridgeNamed(cartridge);
    Shot shot;
    shot.drag = DragModel::FromBc(c.table, c.bc);
    shot.muzzle_velocity_mps = c.v0_mps;
    shot.mass_kg = units::GrainToKg(c.grains);
    shot.bullet_diameter_m = units::InchToM(c.diameter_in);
    shot.bullet_length_m = units::InchToM(c.length_in);
    shot.sight_height_m = c.sight_cm / 100.0;
    shot.atmosphere = AtmosphereNamed(atmosphere);
    shot.sound_speed = SoundSpeedModel::kDryAir;
    shot.aerodynamic_jump = false;  // not modelled by the reference
    return shot;
}

}  // namespace ballistics::reference

#endif  // BALLISTICS_TESTS_REFERENCE_SETUP_H
