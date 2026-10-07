#ifndef BALLISTICS_UNITS_H
#define BALLISTICS_UNITS_H

// Unit conversions. The engine works in SI throughout (m, s, kg, K, Pa,
// rad); these helpers convert at the boundary (UI, import, tests).
namespace ballistics::units {

inline constexpr double kPi = 3.14159265358979323846;

// Length
inline constexpr double kMetersPerInch = 0.0254;
inline constexpr double kMetersPerFoot = 0.3048;
inline constexpr double kMetersPerYard = 0.9144;
constexpr double InchToM(double in) { return in * kMetersPerInch; }
constexpr double MToInch(double m) { return m / kMetersPerInch; }
constexpr double FootToM(double ft) { return ft * kMetersPerFoot; }
constexpr double MToFoot(double m) { return m / kMetersPerFoot; }
constexpr double YardToM(double yd) { return yd * kMetersPerYard; }
constexpr double MToYard(double m) { return m / kMetersPerYard; }

// Mass
inline constexpr double kKgPerGrain = 64.79891e-6;
inline constexpr double kKgPerPound = 0.45359237;
constexpr double GrainToKg(double gr) { return gr * kKgPerGrain; }
constexpr double KgToGrain(double kg) { return kg / kKgPerGrain; }

// Velocity
constexpr double FpsToMps(double fps) { return fps * kMetersPerFoot; }
constexpr double MpsToFps(double mps) { return mps / kMetersPerFoot; }

// Temperature
inline constexpr double kZeroCelsiusK = 273.15;
constexpr double CToK(double c) { return c + kZeroCelsiusK; }
constexpr double KToC(double k) { return k - kZeroCelsiusK; }
constexpr double FToK(double f) { return (f - 32.0) * 5.0 / 9.0 + kZeroCelsiusK; }
constexpr double KToF(double k) { return (k - kZeroCelsiusK) * 9.0 / 5.0 + 32.0; }

// Pressure
inline constexpr double kPaPerHpa = 100.0;
inline constexpr double kPaPerInHg = 3386.389;
inline constexpr double kPaPerMmHg = 133.322387415;
constexpr double HpaToPa(double hpa) { return hpa * kPaPerHpa; }
constexpr double InHgToPa(double inhg) { return inhg * kPaPerInHg; }
constexpr double MmHgToPa(double mmhg) { return mmhg * kPaPerMmHg; }
constexpr double PaToMmHg(double pa) { return pa / kPaPerMmHg; }

// Ballistic coefficient: lb/in^2 (as published) <-> kg/m^2 (sectional
// density form used by the engine).
inline constexpr double kKgM2PerLbIn2 = kKgPerPound / (kMetersPerInch * kMetersPerInch);
constexpr double BcToSi(double bc_lb_in2) { return bc_lb_in2 * kKgM2PerLbIn2; }
constexpr double BcFromSi(double bc_kg_m2) { return bc_kg_m2 / kKgM2PerLbIn2; }

// Angles
constexpr double DegToRad(double deg) { return deg * kPi / 180.0; }
constexpr double RadToDeg(double rad) { return rad * 180.0 / kPi; }
constexpr double MradToRad(double mrad) { return mrad * 1e-3; }
constexpr double RadToMrad(double rad) { return rad * 1e3; }
constexpr double MoaToRad(double moa) { return DegToRad(moa / 60.0); }
constexpr double RadToMoa(double rad) { return RadToDeg(rad) * 60.0; }

// Energy
inline constexpr double kJoulesPerFootPound = 1.3558179483314004;

}  // namespace ballistics::units

#endif  // BALLISTICS_UNITS_H
