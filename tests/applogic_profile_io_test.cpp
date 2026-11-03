// Rifle / cartridge share files (profile_io) and the third-party file
// readers (importers): every field, unit and format variant, malformed
// input, values the database refuses, and storage failures.
#include <ballistics/applogic/importers.h>
#include <ballistics/applogic/library.h>
#include <ballistics/applogic/profile_io.h>
#include <ballistics/storage/repository.h>
#include <ballistics/units.h>
#include <gtest/gtest.h>
#include <sqlite_manager/transaction.h>

#include <filesystem>
#include <nlohmann/json.hpp>
#include <random>
#include <string>
#include <vector>

namespace ballistics::applogic {
namespace {

using nlohmann::json;
using sqlite_manager::ErrorCode;
using storage::BulletRecord;
using storage::CartridgeRecord;
using storage::DragCurveRecord;
using storage::ProfileRecord;
using storage::Repository;
using storage::ReticleRecord;
using storage::RifleRecord;
using storage::ScopeRecord;

bool Contains(const std::string& s, const std::string& part) {
    return s.find(part) != std::string::npos;
}

class ProfileIo : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_TRUE(db_.Open(":memory:").ok()); }

    template <typename T>
    std::size_t Count() {
        return Repository<T>(db_).List().value().size();
    }

    template <typename T>
    T Load(Id id) {
        return *Repository<T>(db_).Get(id).value();
    }

    template <typename T>
    Id Store(T record) {
        auto id = Repository<T>(db_).Save(record);
        EXPECT_TRUE(id.ok()) << id.error().message;
        return id.ok() ? id.value() : 0;
    }

    // Records whose foreign keys point nowhere, as an old or damaged
    // database may hold.
    void ForeignKeysOff() {
        ASSERT_TRUE(db_.connection().Execute("PRAGMA foreign_keys = OFF").ok());
    }

    static ReticleRecord Reticle() {
        ReticleRecord t;
        t.name = "Mil-Dot";
        t.units = "moa";
        t.focal_plane = "sfp";
        t.reference_magnification = 12.0;
        t.definition = R"({"size":[10,10]})";
        t.source = "import:reticle";
        return t;
    }

    static ScopeRecord Scope(std::optional<Id> reticle) {
        ScopeRecord s;
        s.name = "PMII";
        s.click_units = "moa";
        s.click_vertical_rad = 0.0001;
        s.click_horizontal_rad = 0.00015;
        s.min_magnification = 5.0;
        s.max_magnification = 25.0;
        s.notes = "scope notes";
        s.focal_plane = "sfp";
        s.sfp_reference_magnification = 25.0;
        s.reticle_id = reticle;
        return s;
    }

    static RifleRecord Rifle(std::optional<Id> scope) {
        RifleRecord r;
        r.name = "Tikka";
        r.caliber = ".308 Win";
        r.barrel_length_m = 0.61;
        r.twist_m = -0.279;
        r.sight_height_m = 0.045;
        r.notes = "rifle notes";
        r.scope_id = scope;
        r.zero_range_m = 200.0;
        r.zero_atmosphere = {350.0, 97000.0, 275.0, 0.4};
        r.zero_powder_temp_k = 280.0;
        return r;
    }

    static BulletRecord Bullet() {
        BulletRecord b;
        b.name = "SMK 175";
        b.manufacturer = "Sierra";
        b.caliber = ".308";
        b.diameter_m = 0.00782;
        b.mass_kg = 0.01134;
        b.length_m = 0.0315;
        b.drag_kind = storage::kDragKindBc;
        b.drag_table = "G7";
        b.bc = 0.243;
        b.form_factor = 1.01;
        b.source = kSourcePublished;
        b.notes = "bullet notes";
        return b;
    }

    static CartridgeRecord Cartridge(Id bullet) {
        CartridgeRecord c;
        c.name = "Match load";
        c.caliber = ".308 Win";
        c.bullet_id = bullet;
        c.muzzle_velocity_mps = 790.0;
        c.reference_powder_temp_k = 293.15;
        c.powder_sensitivity_per_k = 0.0012;
        c.barrel_length_m = 0.66;
        c.source = kSourceUser;
        c.notes = "cartridge notes";
        return c;
    }

    // Everything the share files can hold, counted.
    std::size_t AllRecords() {
        return Count<BulletRecord>() + Count<CartridgeRecord>() + Count<RifleRecord>() +
               Count<ScopeRecord>() + Count<ReticleRecord>() + Count<DragCurveRecord>() +
               Count<ProfileRecord>();
    }

    storage::Database db_;
};

// ---- Export ---------------------------------------------------------------

TEST_F(ProfileIo, RifleExportHoldsEveryFieldOfRifleScopeAndReticle) {
    const Id reticle = Store(Reticle());
    const Id rifle = Store(Rifle(Store(Scope(reticle))));
    const auto text = ExportRifleJson(db_, rifle);
    ASSERT_TRUE(text.ok()) << text.error().message;
    const json doc = json::parse(text.value());

    EXPECT_EQ(doc.at("format"), "balcalc-rifle");
    EXPECT_EQ(doc.at("version"), 1);
    const json& r = doc.at("rifle");
    EXPECT_EQ(r.at("name"), "Tikka");
    EXPECT_EQ(r.at("caliber"), ".308 Win");
    EXPECT_DOUBLE_EQ(r.at("barrel_length_m").get<double>(), 0.61);
    EXPECT_DOUBLE_EQ(r.at("twist_m").get<double>(), -0.279);
    EXPECT_DOUBLE_EQ(r.at("sight_height_m").get<double>(), 0.045);
    EXPECT_EQ(r.at("notes"), "rifle notes");
    EXPECT_DOUBLE_EQ(r.at("zero_range_m").get<double>(), 200.0);
    EXPECT_DOUBLE_EQ(r.at("zero_atmosphere").at("altitude_m").get<double>(), 350.0);
    EXPECT_DOUBLE_EQ(r.at("zero_atmosphere").at("pressure_pa").get<double>(), 97000.0);
    EXPECT_DOUBLE_EQ(r.at("zero_atmosphere").at("temperature_k").get<double>(), 275.0);
    EXPECT_DOUBLE_EQ(r.at("zero_atmosphere").at("humidity").get<double>(), 0.4);
    EXPECT_DOUBLE_EQ(r.at("zero_powder_temp_k").get<double>(), 280.0);

    const json& s = doc.at("scope");
    EXPECT_EQ(s.at("name"), "PMII");
    EXPECT_EQ(s.at("click_units"), "moa");
    EXPECT_DOUBLE_EQ(s.at("click_vertical_rad").get<double>(), 0.0001);
    EXPECT_DOUBLE_EQ(s.at("click_horizontal_rad").get<double>(), 0.00015);
    EXPECT_DOUBLE_EQ(s.at("min_magnification").get<double>(), 5.0);
    EXPECT_DOUBLE_EQ(s.at("max_magnification").get<double>(), 25.0);
    EXPECT_EQ(s.at("notes"), "scope notes");
    EXPECT_EQ(s.at("focal_plane"), "sfp");
    EXPECT_DOUBLE_EQ(s.at("sfp_reference_magnification").get<double>(), 25.0);

    const json& t = s.at("reticle");
    EXPECT_EQ(t.at("name"), "Mil-Dot");
    EXPECT_EQ(t.at("units"), "moa");
    EXPECT_EQ(t.at("focal_plane"), "sfp");
    EXPECT_DOUBLE_EQ(t.at("reference_magnification").get<double>(), 12.0);
    EXPECT_EQ(t.at("definition"), R"({"size":[10,10]})");
    EXPECT_EQ(t.at("source"), "import:reticle");
}

TEST_F(ProfileIo, RifleWithScopeAndReticleRoundTrips) {
    const Id rifle = Store(Rifle(Store(Scope(Store(Reticle())))));
    const Imported im = ImportShareJson(db_, ExportRifleJson(db_, rifle).value()).value();
    ASSERT_NE(im.rifle_id, 0);
    EXPECT_EQ(im.cartridge_id, 0);
    EXPECT_EQ(im.profile_id, 0);

    const auto copy = Load<RifleRecord>(im.rifle_id);
    EXPECT_EQ(copy.name, "Tikka (2)");
    EXPECT_EQ(copy.caliber, ".308 Win");
    EXPECT_DOUBLE_EQ(copy.barrel_length_m, 0.61);
    EXPECT_DOUBLE_EQ(copy.twist_m, -0.279);
    EXPECT_DOUBLE_EQ(copy.sight_height_m, 0.045);
    EXPECT_EQ(copy.notes, "rifle notes");
    EXPECT_DOUBLE_EQ(copy.zero_range_m, 200.0);
    EXPECT_DOUBLE_EQ(copy.zero_atmosphere.altitude_m, 350.0);
    EXPECT_DOUBLE_EQ(copy.zero_atmosphere.pressure_pa, 97000.0);
    EXPECT_DOUBLE_EQ(copy.zero_atmosphere.temperature_k, 275.0);
    EXPECT_DOUBLE_EQ(copy.zero_atmosphere.humidity, 0.4);
    EXPECT_DOUBLE_EQ(copy.zero_powder_temp_k, 280.0);

    // The scope and its reticle come as new records.
    ASSERT_TRUE(copy.scope_id.has_value());
    EXPECT_EQ(Count<ScopeRecord>(), 2U);
    EXPECT_EQ(Count<ReticleRecord>(), 2U);
    const auto s = Load<ScopeRecord>(*copy.scope_id);
    EXPECT_EQ(s.name, "PMII");
    EXPECT_EQ(s.click_units, "moa");
    EXPECT_DOUBLE_EQ(s.click_vertical_rad, 0.0001);
    EXPECT_DOUBLE_EQ(s.click_horizontal_rad, 0.00015);
    EXPECT_DOUBLE_EQ(s.min_magnification, 5.0);
    EXPECT_DOUBLE_EQ(s.max_magnification, 25.0);
    EXPECT_EQ(s.notes, "scope notes");
    EXPECT_EQ(s.focal_plane, "sfp");
    EXPECT_DOUBLE_EQ(s.sfp_reference_magnification, 25.0);
    ASSERT_TRUE(s.reticle_id.has_value());
    const auto t = Load<ReticleRecord>(*s.reticle_id);
    EXPECT_EQ(t.name, "Mil-Dot");
    EXPECT_EQ(t.units, "moa");
    EXPECT_EQ(t.focal_plane, "sfp");
    EXPECT_DOUBLE_EQ(t.reference_magnification, 12.0);
    EXPECT_EQ(t.definition, R"({"size":[10,10]})");
    EXPECT_EQ(t.source, "import:reticle");
}

TEST_F(ProfileIo, RifleWithoutScopeExportsNullAndImportsWithoutOne) {
    const Id rifle = Store(Rifle(std::nullopt));
    const std::string text = ExportRifleJson(db_, rifle).value();
    EXPECT_TRUE(json::parse(text).at("scope").is_null());
    const Imported im = ImportShareJson(db_, text).value();
    EXPECT_FALSE(Load<RifleRecord>(im.rifle_id).scope_id.has_value());
    EXPECT_EQ(Count<ScopeRecord>(), 0U);
}

TEST_F(ProfileIo, ScopeWithoutReticleRoundTrips) {
    const Id rifle = Store(Rifle(Store(Scope(std::nullopt))));
    const std::string text = ExportRifleJson(db_, rifle).value();
    EXPECT_TRUE(json::parse(text).at("scope").at("reticle").is_null());
    const Imported im = ImportShareJson(db_, text).value();
    const auto s = Load<ScopeRecord>(*Load<RifleRecord>(im.rifle_id).scope_id);
    EXPECT_FALSE(s.reticle_id.has_value());
    EXPECT_EQ(Count<ReticleRecord>(), 0U);
}

TEST_F(ProfileIo, CartridgeExportHoldsEveryFieldOfCartridgeAndBullet) {
    BulletRecord b = Bullet();
    b.drag_kind = storage::kDragKindMultiBc;
    b.bc.reset();
    b.bc_bands = {{800.0, 0.25}, {600.0, 0.24}};
    CartridgeRecord c = Cartridge(Store(b));
    c.velocity_points = {{263.15, 770.0}, {303.15, 800.0}};
    const json doc = json::parse(ExportCartridgeJson(db_, Store(c)).value());

    EXPECT_EQ(doc.at("format"), "balcalc-cartridge");
    EXPECT_EQ(doc.at("version"), 1);
    const json& jc = doc.at("cartridge");
    EXPECT_EQ(jc.at("name"), "Match load");
    EXPECT_EQ(jc.at("caliber"), ".308 Win");
    EXPECT_DOUBLE_EQ(jc.at("muzzle_velocity_mps").get<double>(), 790.0);
    EXPECT_DOUBLE_EQ(jc.at("reference_powder_temp_k").get<double>(), 293.15);
    EXPECT_DOUBLE_EQ(jc.at("powder_sensitivity_per_k").get<double>(), 0.0012);
    EXPECT_DOUBLE_EQ(jc.at("barrel_length_m").get<double>(), 0.66);
    EXPECT_EQ(jc.at("source"), "user");
    EXPECT_EQ(jc.at("notes"), "cartridge notes");
    EXPECT_EQ(jc.at("velocity_points"), json::parse("[[263.15, 770.0], [303.15, 800.0]]"));

    const json& jb = doc.at("bullet");
    EXPECT_EQ(jb.at("name"), "SMK 175");
    EXPECT_EQ(jb.at("manufacturer"), "Sierra");
    EXPECT_EQ(jb.at("caliber"), ".308");
    EXPECT_DOUBLE_EQ(jb.at("diameter_m").get<double>(), 0.00782);
    EXPECT_DOUBLE_EQ(jb.at("mass_kg").get<double>(), 0.01134);
    EXPECT_DOUBLE_EQ(jb.at("length_m").get<double>(), 0.0315);
    EXPECT_EQ(jb.at("drag_kind"), "multi_bc");
    EXPECT_EQ(jb.at("drag_table"), "G7");
    EXPECT_TRUE(jb.at("bc").is_null());
    EXPECT_DOUBLE_EQ(jb.at("form_factor").get<double>(), 1.01);
    EXPECT_EQ(jb.at("source"), "published");
    EXPECT_EQ(jb.at("notes"), "bullet notes");
    ASSERT_EQ(jb.at("bc_bands").size(), 2U);
    // Bands come back sorted by the repository; both are there.
    double sum_v = 0.0;
    for (const json& band : jb.at("bc_bands")) {
        sum_v += band.at(0).get<double>();
    }
    EXPECT_DOUBLE_EQ(sum_v, 1400.0);
    EXPECT_TRUE(jb.at("curve").is_null());
}

TEST_F(ProfileIo, CartridgeWithBandsAndVelocityPointsRoundTripsAndReusesTheBullet) {
    BulletRecord b = Bullet();
    b.drag_kind = storage::kDragKindMultiBc;
    b.bc.reset();
    b.bc_bands = {{800.0, 0.25}, {600.0, 0.24}};
    const Id bullet = Store(b);
    CartridgeRecord c = Cartridge(bullet);
    c.velocity_points = {{263.15, 770.0}, {303.15, 800.0}};
    const Id cartridge = Store(c);

    const Imported im = ImportShareJson(db_, ExportCartridgeJson(db_, cartridge).value()).value();
    ASSERT_NE(im.cartridge_id, 0);
    EXPECT_EQ(im.rifle_id, 0);
    const auto copy = Load<CartridgeRecord>(im.cartridge_id);
    EXPECT_EQ(copy.name, "Match load (2)");
    EXPECT_EQ(copy.caliber, ".308 Win");
    EXPECT_DOUBLE_EQ(copy.muzzle_velocity_mps, 790.0);
    EXPECT_DOUBLE_EQ(copy.reference_powder_temp_k, 293.15);
    EXPECT_DOUBLE_EQ(copy.powder_sensitivity_per_k, 0.0012);
    EXPECT_DOUBLE_EQ(copy.barrel_length_m, 0.66);
    EXPECT_EQ(copy.source, "user");
    EXPECT_EQ(copy.notes, "cartridge notes");
    EXPECT_EQ(copy.velocity_points.size(), 2U);
    // The library bullet with identical bands is shared, not duplicated.
    EXPECT_EQ(copy.bullet_id, bullet);
    EXPECT_EQ(Count<BulletRecord>(), 1U);
}

TEST_F(ProfileIo, BulletWithOtherBandsIsNotReused) {
    BulletRecord b = Bullet();
    b.drag_kind = storage::kDragKindMultiBc;
    b.bc.reset();
    b.bc_bands = {{800.0, 0.25}, {600.0, 0.24}};
    const Id bullet = Store(b);
    json doc = json::parse(ExportCartridgeJson(db_, Store(Cartridge(bullet))).value());
    doc["bullet"]["bc_bands"] = json::parse("[[800.0, 0.25], [600.0, 0.23]]");
    const Imported im = ImportShareJson(db_, doc.dump()).value();
    const auto copy = Load<BulletRecord>(Load<CartridgeRecord>(im.cartridge_id).bullet_id);
    EXPECT_NE(copy.id, bullet);
    EXPECT_EQ(Count<BulletRecord>(), 2U);
    ASSERT_EQ(copy.bc_bands.size(), 2U);
}

TEST_F(ProfileIo, BulletDifferingInAnyIdentityFieldIsNotReused) {
    const Id bullet = Store(Bullet());
    const json base = json::parse(ExportCartridgeJson(db_, Store(Cartridge(bullet))).value());
    const std::vector<std::pair<std::string, json>> changes = {
        {"drag_table", "G1"}, {"mass_kg", 0.0113}, {"diameter_m", 0.0078},
        {"bc", 0.244},        {"bc", nullptr},
    };
    for (const auto& [key, value] : changes) {
        SCOPED_TRACE(key + " = " + value.dump());
        json doc = base;
        doc["bullet"][key] = value;
        if (value.is_null()) {
            doc["bullet"]["drag_kind"] = "multi_bc";
            doc["bullet"]["bc_bands"] = json::parse("[[800.0, 0.25]]");
        }
        const auto im = ImportShareJson(db_, doc.dump());
        ASSERT_TRUE(im.ok()) << im.error().message;
        EXPECT_NE(Load<CartridgeRecord>(im.value().cartridge_id).bullet_id, bullet);
    }
    EXPECT_EQ(Count<BulletRecord>(), 1U + changes.size());
}

TEST_F(ProfileIo, UserBulletsAreNeverReused) {
    // A bullet typed in by the user belongs to their cartridge only.
    BulletRecord b = Bullet();
    b.source = kSourceUser;
    const Id bullet = Store(b);
    const Imported im =
        ImportShareJson(db_, ExportCartridgeJson(db_, Store(Cartridge(bullet))).value()).value();
    EXPECT_NE(Load<CartridgeRecord>(im.cartridge_id).bullet_id, bullet);
    EXPECT_EQ(Count<BulletRecord>(), 2U);
}

TEST_F(ProfileIo, CurveBulletRoundTripsAsACopyOfBulletAndCurve) {
    DragCurveRecord curve;
    curve.name = "Radar 175";
    curve.source = "import:drg";
    curve.points = {{0.5, 0.2}, {1.0, 0.4}, {2.0, 0.3}};
    BulletRecord b = Bullet();
    b.drag_kind = storage::kDragKindCurve;
    b.bc.reset();
    b.drag_table = "";
    b.curve_id = Store(curve);
    const Id bullet = Store(b);
    const std::string text = ExportCartridgeJson(db_, Store(Cartridge(bullet))).value();

    const json jcurve = json::parse(text).at("bullet").at("curve");
    EXPECT_EQ(jcurve.at("name"), "Radar 175");
    EXPECT_EQ(jcurve.at("source"), "import:drg");
    EXPECT_EQ(jcurve.at("points"), json::parse("[[0.5, 0.2], [1.0, 0.4], [2.0, 0.3]]"));

    const Imported im = ImportShareJson(db_, text).value();
    const auto copy = Load<BulletRecord>(Load<CartridgeRecord>(im.cartridge_id).bullet_id);
    // Curves are compared by id only: the import always brings its own.
    EXPECT_NE(copy.id, bullet);
    EXPECT_EQ(copy.drag_kind, "curve");
    ASSERT_TRUE(copy.curve_id.has_value());
    EXPECT_NE(*copy.curve_id, *b.curve_id);
    const auto curve_copy = Load<DragCurveRecord>(*copy.curve_id);
    EXPECT_EQ(curve_copy.name, "Radar 175");
    EXPECT_EQ(curve_copy.source, "import:drg");
    ASSERT_EQ(curve_copy.points.size(), 3U);
    EXPECT_DOUBLE_EQ(curve_copy.points[1].mach, 1.0);
    EXPECT_DOUBLE_EQ(curve_copy.points[1].cd, 0.4);
}

TEST_F(ProfileIo, ImportedLibrarySourceBecomesTheUsers) {
    // A cartridge read from an .ammo file and shared is the user's now.
    CartridgeRecord c = Cartridge(Store(Bullet()));
    c.source = kSourceAmmoFile;
    const Imported im = ImportShareJson(db_, ExportCartridgeJson(db_, Store(c)).value()).value();
    EXPECT_EQ(Load<CartridgeRecord>(im.cartridge_id).source, kSourceUser);

    c.id = 0;
    c.source = "factory";
    const Imported other = ImportShareJson(db_, ExportCartridgeJson(db_, Store(c)).value()).value();
    EXPECT_EQ(Load<CartridgeRecord>(other.cartridge_id).source, "factory");
}

TEST_F(ProfileIo, TakenNamesGetTheNextFreeSuffix) {
    const std::string text = ExportRifleJson(db_, Store(Rifle(std::nullopt))).value();
    std::vector<std::string> names;
    names.reserve(3);
    for (int i = 0; i < 3; ++i) {
        names.push_back(Load<RifleRecord>(ImportShareJson(db_, text).value().rifle_id).name);
    }
    EXPECT_EQ(names, (std::vector<std::string>{"Tikka (2)", "Tikka (3)", "Tikka (4)"}));
}

TEST_F(ProfileIo, ExportOfMissingRecordsIsNotFound) {
    const auto rifle = ExportRifleJson(db_, 42);
    ASSERT_FALSE(rifle.ok());
    EXPECT_EQ(rifle.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(rifle.error().message, "rifle not found");
    const auto cartridge = ExportCartridgeJson(db_, 42);
    ASSERT_FALSE(cartridge.ok());
    EXPECT_EQ(cartridge.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(cartridge.error().message, "cartridge not found");
}

TEST_F(ProfileIo, ExportReportsDanglingReferences) {
    ForeignKeysOff();
    // Rifle -> missing scope.
    const Id scope = Store(Scope(std::nullopt));
    const Id rifle = Store(Rifle(scope));
    ASSERT_TRUE(Repository<ScopeRecord>(db_).Remove(scope).ok());
    EXPECT_EQ(ExportRifleJson(db_, rifle).error().message, "scope not found");

    // Scope -> missing reticle.
    const Id reticle = Store(Reticle());
    const Id rifle2 = Store(Rifle(Store(Scope(reticle))));
    ASSERT_TRUE(Repository<ReticleRecord>(db_).Remove(reticle).ok());
    EXPECT_EQ(ExportRifleJson(db_, rifle2).error().message, "reticle not found");

    // Cartridge -> missing bullet.
    const Id bullet = Store(Bullet());
    const Id cartridge = Store(Cartridge(bullet));
    ASSERT_TRUE(Repository<BulletRecord>(db_).Remove(bullet).ok());
    EXPECT_EQ(ExportCartridgeJson(db_, cartridge).error().message, "bullet not found");

    // Bullet -> missing drag curve.
    DragCurveRecord curve;
    curve.name = "c";
    curve.points = {{0.5, 0.2}, {1.0, 0.4}};
    BulletRecord b = Bullet();
    b.drag_kind = storage::kDragKindCurve;
    b.curve_id = Store(curve);
    const Id cartridge2 = Store(Cartridge(Store(b)));
    ASSERT_TRUE(Repository<DragCurveRecord>(db_).Remove(*b.curve_id).ok());
    const auto r = ExportCartridgeJson(db_, cartridge2);
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::kNotFound);
    EXPECT_EQ(r.error().message, "drag curve not found");
}

TEST_F(ProfileIo, ClosedDatabaseFailsExportAndImport) {
    storage::Database closed;
    EXPECT_FALSE(ExportRifleJson(closed, 1).ok());
    EXPECT_FALSE(ExportCartridgeJson(closed, 1).ok());
    // The header is fine; the transaction cannot start.
    const auto im = ImportShareJson(closed, R"({"format":"balcalc-rifle","version":1})");
    ASSERT_FALSE(im.ok());
    EXPECT_NE(im.error().code, ErrorCode::kFormat);
}

// ---- Import of hand-written and damaged files ------------------------------

TEST_F(ProfileIo, MinimalFilesTakeTheDefaults) {
    const Imported rifle = ImportShareJson(db_, R"({"format":"balcalc-rifle",
        "rifle": {"name": "R", "sight_height_m": 0.04, "zero_range_m": 100.0,
                  "zero_atmosphere": {"altitude_m": 0, "pressure_pa": 101325,
                                      "temperature_k": 290.0, "humidity": 0}},
        "scope": {"name": "S", "click_vertical_rad": 0.0001, "click_horizontal_rad": 0.0001,
                  "reticle": {"name": "T"}}})")
                               .value();
    const auto r = Load<RifleRecord>(rifle.rifle_id);
    EXPECT_EQ(r.caliber, "");
    EXPECT_DOUBLE_EQ(r.barrel_length_m, 0.0);
    EXPECT_DOUBLE_EQ(r.twist_m, 0.0);
    EXPECT_EQ(r.notes, "");
    // Powder at the air temperature of the zero.
    EXPECT_DOUBLE_EQ(r.zero_powder_temp_k, 290.0);
    const auto s = Load<ScopeRecord>(*r.scope_id);
    EXPECT_EQ(s.click_units, "mrad");
    EXPECT_EQ(s.focal_plane, "ffp");
    EXPECT_DOUBLE_EQ(s.max_magnification, 0.0);
    const auto t = Load<ReticleRecord>(*s.reticle_id);
    EXPECT_EQ(t.units, "mrad");
    EXPECT_EQ(t.focal_plane, "ffp");
    EXPECT_EQ(t.definition, "");

    // No version: taken as the first one.
    const Imported cart = ImportShareJson(db_, R"({"format":"balcalc-cartridge",
        "cartridge": {"name": "C", "muzzle_velocity_mps": 800.0},
        "bullet": {"name": "B", "caliber": ".224", "diameter_m": 0.0057, "mass_kg": 0.005,
                   "drag_kind": "bc", "bc": 0.2,
                   "curve": null}})")
                              .value();
    const auto c = Load<CartridgeRecord>(cart.cartridge_id);
    EXPECT_EQ(c.caliber, ".224");  // from the bullet
    EXPECT_DOUBLE_EQ(c.reference_powder_temp_k, 288.15);
    EXPECT_DOUBLE_EQ(c.powder_sensitivity_per_k, 0.0);
    EXPECT_TRUE(c.velocity_points.empty());
    const auto b = Load<BulletRecord>(c.bullet_id);
    EXPECT_EQ(b.drag_table, "G7");
    EXPECT_DOUBLE_EQ(b.form_factor, 1.0);
    EXPECT_DOUBLE_EQ(b.length_m, 0.0);
    EXPECT_FALSE(b.curve_id.has_value());
}

TEST_F(ProfileIo, CurveWithoutANameIsNamedAfterTheBullet) {
    const Imported im = ImportShareJson(db_, R"({"format":"balcalc-cartridge","version":1,
        "cartridge": {"name": "C", "muzzle_velocity_mps": 800.0},
        "bullet": {"name": "Radar B", "diameter_m": 0.0078, "mass_kg": 0.011,
                   "drag_kind": "curve", "drag_table": "",
                   "curve": {"points": [[0.5, 0.2], [1.0, 0.4]]}}})")
                            .value();
    const auto b = Load<BulletRecord>(Load<CartridgeRecord>(im.cartridge_id).bullet_id);
    EXPECT_FALSE(b.bc.has_value());
    EXPECT_EQ(Load<DragCurveRecord>(*b.curve_id).name, "Radar B");
    EXPECT_EQ(Load<DragCurveRecord>(*b.curve_id).source, "");
}

TEST_F(ProfileIo, LegacyProfileWithoutRifleCaliberTakesTheBullets) {
    const Imported im = ImportShareJson(db_, R"({"format":"balcalc-profile","version":1,
      "profile": {"zero_range_m": 100.0,
                  "zero_atmosphere": {"altitude_m": 0, "pressure_pa": 101325,
                                      "temperature_k": 288.15, "humidity": 0}},
      "rifle": {"name": "R", "sight_height_m": 0.05},
      "cartridge": {"name": "C", "muzzle_velocity_mps": 800.0},
      "bullet": {"name": "B", "caliber": "6.5mm", "diameter_m": 0.0067, "mass_kg": 0.009,
                 "drag_kind": "bc", "bc": 0.3}})")
                            .value();
    ASSERT_NE(im.profile_id, 0);
    EXPECT_EQ(Load<CartridgeRecord>(im.cartridge_id).caliber, "6.5mm");
    EXPECT_FALSE(Load<RifleRecord>(im.rifle_id).scope_id.has_value());
    const auto p = Load<ProfileRecord>(im.profile_id);
    EXPECT_EQ(p.rifle_id, im.rifle_id);
    EXPECT_EQ(p.cartridge_id, im.cartridge_id);
    EXPECT_EQ(p.name, "");
    EXPECT_DOUBLE_EQ(p.zero_offset_up_m, 0.0);
    EXPECT_DOUBLE_EQ(p.zero_offset_right_m, 0.0);
    EXPECT_DOUBLE_EQ(p.velocity_scale, 1.0);
    EXPECT_DOUBLE_EQ(p.drag_scale, 1.0);
}

TEST_F(ProfileIo, NotAShareFileIsRefusedWithAReason) {
    struct Case {
        const char* text;
        const char* reason;
    };
    const std::vector<Case> cases = {
        {"", "it is not JSON"},
        {"[1, 2]", "it is not JSON"},
        {"42", "it is not JSON"},
        {"{}", "unknown format"},
        {R"({"format": "balcalc-bullets"})", "unknown format"},
        {R"({"format": "balcalc-cartridge", "version": 2})", "made by a newer version"},
    };
    for (const Case& c : cases) {
        SCOPED_TRACE(c.text);
        const auto r = ImportShareJson(db_, c.text);
        ASSERT_FALSE(r.ok());
        EXPECT_EQ(r.error().code, ErrorCode::kFormat);
        EXPECT_TRUE(Contains(r.error().message, "Not a valid Holdmark file: "));
        EXPECT_TRUE(Contains(r.error().message, c.reason)) << r.error().message;
    }
}

TEST_F(ProfileIo, IncompleteOrMistypedDocumentsAreRefusedAndLeaveNothing) {
    const std::string atm =
        R"({"altitude_m": 0, "pressure_pa": 101325, "temperature_k": 288.15, "humidity": 0})";
    const std::string bullet =
        R"({"name": "B", "diameter_m": 0.0078, "mass_kg": 0.011, "drag_kind": "bc", "bc": 0.3})";
    const std::string cart = R"({"name": "C", "muzzle_velocity_mps": 800.0})";
    const std::string rifle = R"({"name": "R", "sight_height_m": 0.05, "zero_range_m": 100.0,
        "zero_atmosphere": )" +
                              atm + "}";
    const std::string scope =
        R"({"name": "S", "click_vertical_rad": 0.0001, "click_horizontal_rad": 0.0001})";
    const std::vector<std::pair<std::string, std::string>> cases = {
        // Missing objects and keys.
        {R"({"format":"balcalc-rifle"})", "rifle"},
        {R"({"format":"balcalc-rifle","scope":)" + scope +
             R"(,"rifle":{"name":"R","sight_height_m":0.05}})",
         "zero_range_m"},
        {R"({"format":"balcalc-rifle","rifle":{"name":"R","sight_height_m":0.05,
            "zero_range_m":100,"zero_atmosphere":{"altitude_m":0}}})",
         "pressure_pa"},
        {R"({"format":"balcalc-rifle","rifle":)" + rifle +
             R"(,"scope":{"name":"S","click_horizontal_rad":0.0001}})",
         "click_vertical_rad"},
        {R"({"format":"balcalc-rifle","rifle":)" + rifle +
             R"(,"scope":{"name":"S","click_vertical_rad":0.0001,"click_horizontal_rad":0.0001,
                "reticle":{"units":"mrad"}}})",
         "name"},
        {R"({"format":"balcalc-cartridge","cartridge":)" + cart + "}", "bullet"},
        {R"({"format":"balcalc-cartridge","bullet":)" + bullet + "}", "cartridge"},
        {R"({"format":"balcalc-cartridge","cartridge":{"name":"C"},"bullet":)" + bullet + "}",
         "muzzle_velocity_mps"},
        {R"({"format":"balcalc-cartridge","cartridge":)" + cart +
             R"(,"bullet":{"name":"B","mass_kg":0.011,"drag_kind":"bc"}})",
         "diameter_m"},
        {R"({"format":"balcalc-cartridge","cartridge":)" + cart +
             R"(,"bullet":{"name":"B","diameter_m":0.0078,"mass_kg":0.011,"drag_kind":"curve",
                "curve":{"name":"c"}}})",
         "points"},
        {R"({"format":"balcalc-profile","rifle":{"name":"R","sight_height_m":0.05},
            "cartridge":)" +
             cart + R"(,"bullet":)" + bullet + "}",
         "profile"},
        // Wrong types.
        {R"({"format":"balcalc-cartridge","cartridge":)" + cart +
             R"(,"bullet":{"name":"B","diameter_m":"7.8mm","mass_kg":0.011,"drag_kind":"bc"}})",
         "type"},
        {R"({"format":"balcalc-cartridge","cartridge":)" + cart +
             R"(,"bullet":{"name":"B","diameter_m":0.0078,"mass_kg":0.011,"drag_kind":"multi_bc",
                "bc_bands":[[800.0]]}})",
         "out_of_range"},
        {R"({"format":"balcalc-cartridge","cartridge":{"name":"C","muzzle_velocity_mps":800,
            "velocity_points":[["cold", 780]]},"bullet":)" +
             bullet + "}",
         "type"},
    };
    for (const auto& [text, reason] : cases) {
        SCOPED_TRACE(text);
        const auto r = ImportShareJson(db_, text);
        ASSERT_FALSE(r.ok());
        EXPECT_EQ(r.error().code, ErrorCode::kFormat);
        EXPECT_TRUE(Contains(r.error().message, "Not a valid Holdmark file: "));
        EXPECT_TRUE(Contains(r.error().message, reason)) << r.error().message;
        EXPECT_EQ(AllRecords(), 0U);
    }
}

TEST_F(ProfileIo, ValuesTheDatabaseRefusesFailTheWholeImport) {
    const std::string atm =
        R"({"altitude_m": 0, "pressure_pa": 101325, "temperature_k": 288.15, "humidity": 0})";
    const std::string bullet =
        R"({"name": "B", "diameter_m": 0.0078, "mass_kg": 0.011, "drag_kind": "bc", "bc": 0.3})";
    const std::string cart = R"({"name": "C", "muzzle_velocity_mps": 800.0})";
    const std::string rifle = R"({"name": "R", "sight_height_m": 0.05, "zero_range_m": 100.0,
        "zero_atmosphere": )" +
                              atm + "}";
    const std::string profile = R"({"zero_range_m": 100.0, "zero_atmosphere": )" + atm + "}";
    const std::string scope =
        R"({"name": "S", "click_vertical_rad": 0.0001, "click_horizontal_rad": 0.0001})";
    const std::vector<std::pair<const char*, std::string>> cases = {
        {"reticle units",
         R"({"format":"balcalc-rifle","rifle":)" + rifle +
             R"(,"scope":{"name":"S","click_vertical_rad":0.0001,"click_horizontal_rad":0.0001,
                "reticle":{"name":"T","units":"furlong"}}})"},
        {"zero click",
         R"({"format":"balcalc-rifle","rifle":)" + rifle +
             R"(,"scope":{"name":"S","click_vertical_rad":0,"click_horizontal_rad":0.0001}})"},
        {"zero range", R"({"format":"balcalc-rifle","scope":)" + scope +
                           R"(,"rifle":{"name":"R","sight_height_m":0.05,"zero_range_m":0,
                "zero_atmosphere":)" +
                           atm + "}}"},
        {"weightless bullet",
         R"({"format":"balcalc-cartridge","cartridge":)" + cart +
             R"(,"bullet":{"name":"B","diameter_m":0.0078,"mass_kg":0,"drag_kind":"bc","bc":0.3}})"},
        {"negative drag",
         R"({"format":"balcalc-cartridge","cartridge":)" + cart +
             R"(,"bullet":{"name":"B","diameter_m":0.0078,"mass_kg":0.011,"drag_kind":"curve",
                "curve":{"points":[[0.5,-0.2],[1.0,0.4]]}}})"},
        {"standing cartridge",
         R"({"format":"balcalc-cartridge","cartridge":{"name":"C","muzzle_velocity_mps":0},
            "bullet":)" +
             bullet + "}"},
        {"legacy scope",
         R"({"format":"balcalc-profile","profile":)" + profile + R"(,"rifle":)" + rifle +
             R"(,"cartridge":)" + cart + R"(,"bullet":)" + bullet +
             R"(,"scope":{"name":"S","click_units":"clicks","click_vertical_rad":0.0001,
                "click_horizontal_rad":0.0001}})"},
        {"legacy zero",
         R"({"format":"balcalc-profile","profile":{"zero_range_m":-5,"zero_atmosphere":)" + atm +
             R"(},"rifle":)" + rifle + R"(,"cartridge":)" + cart + R"(,"bullet":)" + bullet + "}"},
        {"legacy truing",
         R"({"format":"balcalc-profile","profile":{"zero_range_m":100,"velocity_scale":0,
            "zero_atmosphere":)" +
             atm + R"(},"rifle":)" + rifle + R"(,"cartridge":)" + cart + R"(,"bullet":)" + bullet +
             "}"},
    };
    for (const auto& [what, text] : cases) {
        SCOPED_TRACE(what);
        ASSERT_TRUE(json::accept(text)) << text;
        const auto r = ImportShareJson(db_, text);
        ASSERT_FALSE(r.ok());
        EXPECT_NE(r.error().code, ErrorCode::kFormat) << r.error().message;
        // Rolled back: no half of the file is left in the library.
        EXPECT_EQ(AllRecords(), 0U);
    }
}

// A file whose header fields have the wrong JSON types is refused like any
// other malformed file, not thrown out of ImportShareJson.
TEST_F(ProfileIo, MistypedHeaderIsRefusedNotThrown) {
    for (const char* text :
         {R"({"format": 1})", R"({"format": "balcalc-rifle", "version": "1"})"}) {
        SCOPED_TRACE(text);
        Result<Imported> r = Imported{};
        ASSERT_NO_THROW(r = ImportShareJson(db_, text));
        ASSERT_FALSE(r.ok());
        EXPECT_EQ(r.error().code, ErrorCode::kFormat);
    }
}

// ---- Importers: parsing ----------------------------------------------------

std::string Ammo(const std::string& attrs) { return "<ammo-info-ex " + attrs + " />"; }

TEST(ImportParse, AmmoWeightUnits) {
    const std::vector<std::pair<std::string, double>> cases = {
        {"175gr", units::GrainToKg(175.0)},
        {"175 grain", units::GrainToKg(175.0)},
        {"11.3g", 0.0113},
        {"0.0113kg", 0.0113},
        {"0.025lb", 0.025 * 0.45359237},
        {"11.3 G", 0.0113},
    };
    for (const auto& [weight, kg] : cases) {
        SCOPED_TRACE(weight);
        const auto a = ParseAmmo(Ammo(R"(name="x" bc="0.3" muzzle-velocity="800m/s"
            bullet-diameter="7.8mm" bullet-weight=")" +
                                      weight + "\""));
        ASSERT_TRUE(a.ok()) << a.error().message;
        EXPECT_NEAR(a.value().bullet.mass_kg, kg, 1e-12);
    }
    for (const char* weight : {"11.3oz", "heavy", "", "inf g"}) {
        SCOPED_TRACE(weight);
        const auto a = ParseAmmo(Ammo(R"(name="x" bc="0.3" muzzle-velocity="800m/s"
            bullet-diameter="7.8mm" bullet-weight=")" +
                                      std::string(weight) + "\""));
        ASSERT_FALSE(a.ok());
        EXPECT_EQ(a.error().code, ErrorCode::kFormat);
        EXPECT_EQ(a.error().message, "x: bullet weight is missing or has unknown units.");
    }
}

TEST(ImportParse, AmmoLengthUnits) {
    const std::vector<std::pair<std::string, double>> cases = {
        {"0.308in", units::InchToM(0.308)},
        {"0.308 inch", units::InchToM(0.308)},
        {"7.82mm", 0.00782},
        {"0.782cm", 0.00782},
        {"0.00782m", 0.00782},
        {"0.0257ft", units::FootToM(0.0257)},
        {"0.00855yd", units::YardToM(0.00855)},
    };
    for (const auto& [length, m] : cases) {
        SCOPED_TRACE(length);
        std::string attrs = R"(name="x" bc="0.3" bullet-weight="10g"
            muzzle-velocity="800m/s" bullet-diameter=")";
        attrs += length;
        attrs += R"(" bullet-length=")";
        attrs += length;
        attrs += R"(" barrel-length=")";
        attrs += length;
        attrs += "\"";
        const auto a = ParseAmmo(Ammo(attrs));
        ASSERT_TRUE(a.ok()) << a.error().message;
        EXPECT_NEAR(a.value().bullet.diameter_m, m, 1e-12);
        EXPECT_NEAR(a.value().bullet.length_m, m, 1e-12);
        EXPECT_NEAR(a.value().cartridge.barrel_length_m, m, 1e-12);
    }
    // Unreadable optional lengths are left at 0.
    const auto a = ParseAmmo(Ammo(R"(name="x" bc="0.3" bullet-weight="10g"
        muzzle-velocity="800m/s" bullet-diameter="7.8mm" bullet-length="long"
        barrel-length="24 furlongs")"));
    ASSERT_TRUE(a.ok());
    EXPECT_DOUBLE_EQ(a.value().bullet.length_m, 0.0);
    EXPECT_DOUBLE_EQ(a.value().cartridge.barrel_length_m, 0.0);
}

TEST(ImportParse, AmmoVelocityUnits) {
    const std::vector<std::pair<std::string, double>> cases = {
        {"2600ft/s", units::FpsToMps(2600.0)},
        {"2600 fps", units::FpsToMps(2600.0)},
        {"800m/s", 800.0},
        {"800 MPS", 800.0},
    };
    for (const auto& [mv, mps] : cases) {
        SCOPED_TRACE(mv);
        const auto a = ParseAmmo(Ammo(R"(name="x" bc="0.3" bullet-weight="10g"
            bullet-diameter="7.8mm" muzzle-velocity=")" +
                                      mv + "\""));
        ASSERT_TRUE(a.ok()) << a.error().message;
        EXPECT_NEAR(a.value().cartridge.muzzle_velocity_mps, mps, 1e-9);
    }
    for (const char* mv : {"800km/h", "fast", ""}) {
        SCOPED_TRACE(mv);
        const auto a = ParseAmmo(Ammo(R"(name="x" bc="0.3" bullet-weight="10g"
            bullet-diameter="7.8mm" muzzle-velocity=")" +
                                      std::string(mv) + "\""));
        ASSERT_FALSE(a.ok());
        EXPECT_EQ(a.error().message, "x: muzzle velocity is missing or has unknown units.");
    }
    const auto none = ParseAmmo(Ammo(R"(name="x" bc="0.3" bullet-weight="10g"
        bullet-diameter="7.8mm")"));
    EXPECT_EQ(none.error().message, "x: muzzle velocity is missing or has unknown units.");
}

TEST(ImportParse, AmmoDiameterFromTheCaliberLabel) {
    const std::vector<std::pair<std::string, double>> cases = {
        {".308 Win", units::InchToM(0.308)},
        {" .224 Valkyrie", units::InchToM(0.224)},
        {"0.338 Lapua", units::InchToM(0.338)},
        {"6.5 Creedmoor", 0.0065},
        {"9mm Luger", 0.009},
        {"45 ACP", units::InchToM(0.45)},
        {"50 BMG", units::InchToM(0.50)},
        {"308 Win", units::InchToM(0.308)},
        {"12 GA", units::InchToM(0.729)},
        {"10 ga", units::InchToM(0.775)},
        {"16 GA", units::InchToM(0.662)},
        {"20 GA", units::InchToM(0.615)},
        {"28 GA", units::InchToM(0.550)},
    };
    for (const auto& [caliber, m] : cases) {
        SCOPED_TRACE(caliber);
        const auto a = ParseAmmo(Ammo(R"(name="x" bc="0.3" bullet-weight="10g"
            muzzle-velocity="800m/s" caliber=")" +
                                      caliber + "\""));
        ASSERT_TRUE(a.ok()) << a.error().message;
        EXPECT_NEAR(a.value().bullet.diameter_m, m, 1e-12);
        EXPECT_EQ(a.value().bullet.caliber, caliber);
    }
    // Labels that tell nothing, and a missing caliber.
    for (const char* caliber : {"14 GA", "Magnum", ".abc", "0", "-5mm", ""}) {
        SCOPED_TRACE(caliber);
        const auto a = ParseAmmo(Ammo(R"(name="x" bc="0.3" bullet-weight="10g"
            muzzle-velocity="800m/s" caliber=")" +
                                      std::string(caliber) + "\""));
        ASSERT_FALSE(a.ok());
        EXPECT_EQ(a.error().message, "x: bullet diameter is missing.");
    }
    const auto unknown_unit = ParseAmmo(Ammo(R"(name="x" bc="0.3" bullet-weight="10g"
        muzzle-velocity="800m/s" bullet-diameter="0.3 cubits")"));
    EXPECT_EQ(unknown_unit.error().message, "x: bullet diameter is missing.");
}

TEST(ImportParse, AmmoFieldsAndRequiredValues) {
    const auto a = ParseAmmo(Ammo(R"(name="Gold Medal" caliber=".308" source="Federal"
        bullet-type="HPBT" bc="0.475" bullet-weight="175gr" muzzle-velocity="2600ft/s"
        bullet-diameter="0.308in")"));
    ASSERT_TRUE(a.ok());
    const BulletRecord& b = a.value().bullet;
    EXPECT_EQ(b.manufacturer, "Federal");
    EXPECT_EQ(b.notes, "Type: HPBT");
    EXPECT_EQ(b.drag_table, "G1");  // no table: G1
    EXPECT_EQ(b.source, kSourceAmmoFile);
    EXPECT_EQ(b.id, 0);
    EXPECT_EQ(a.value().cartridge.name, "Gold Medal");
    EXPECT_EQ(a.value().cartridge.source, kSourceAmmoFile);
    EXPECT_EQ(a.value().cartridge.bullet_id, 0);

    const auto empty_type = ParseAmmo(Ammo(R"(name="x" bullet-type="" bc="0.3"
        bullet-weight="10g" muzzle-velocity="800m/s" bullet-diameter="7.8mm")"));
    EXPECT_EQ(empty_type.value().bullet.notes, "");

    struct Case {
        std::string xml;
        std::string message;
    };
    const std::vector<Case> cases = {
        {"<ammo-info-ex name=", "Not an .ammo file: invalid XML."},
        {"", "Not an .ammo file: invalid XML."},
        {"<reticle/>", "Not an .ammo file: no <ammo-info-ex> element."},
        {Ammo(R"(bc="0.3" bullet-weight="10g")"), "The .ammo file has no name."},
        {Ammo(R"(name="" bc="0.3" bullet-weight="10g")"), "The .ammo file has no name."},
        {Ammo(R"(name="x" bullet-weight="10g" bullet-diameter="7.8mm" muzzle-velocity="800m/s")"),
         "x: BC is missing."},
        {Ammo(R"(name="x" bc="0" bullet-weight="10g" bullet-diameter="7.8mm")"),
         "x: BC is missing."},
        {Ammo(R"(name="x" bc="high" bullet-weight="10g" bullet-diameter="7.8mm")"),
         "x: BC is missing."},
        {Ammo(R"(name="x" bc="0.3" bullet-diameter="7.8mm")"),
         "x: bullet weight is missing or has unknown units."},
    };
    for (const Case& c : cases) {
        SCOPED_TRACE(c.xml);
        const auto r = ParseAmmo(c.xml);
        ASSERT_FALSE(r.ok());
        EXPECT_EQ(r.error().code, ErrorCode::kFormat);
        EXPECT_EQ(r.error().message, c.message);
    }
}

TEST(ImportParse, DrgPointsAreSortedAndDeduplicated) {
    const auto d = ParseDrg(
        "BRL, Test bullet,, 0.0113, 0.00782, 0.031\n"
        "0.30 1.50\n"
        "comment line\n"
        "\n"
        "0.20 0.50\n"
        "0.25 1.50\n"  // the same Mach again: one of the two stays
        "0.40 2.50\n");
    ASSERT_TRUE(d.ok()) << d.error().message;
    EXPECT_EQ(d.value().kind, "BRL");
    EXPECT_EQ(d.value().name, "Test bullet");  // trailing commas trimmed
    EXPECT_DOUBLE_EQ(d.value().mass_kg, 0.0113);
    EXPECT_DOUBLE_EQ(d.value().diameter_m, 0.00782);
    EXPECT_DOUBLE_EQ(d.value().length_m, 0.031);
    ASSERT_EQ(d.value().points.size(), 3U);
    EXPECT_DOUBLE_EQ(d.value().points[0].mach, 0.5);
    EXPECT_DOUBLE_EQ(d.value().points[0].cd, 0.20);
    EXPECT_DOUBLE_EQ(d.value().points[1].mach, 1.5);
    EXPECT_DOUBLE_EQ(d.value().points[2].mach, 2.5);

    // Exponents in the header numbers.
    const auto e = ParseDrg("CFM, Exp, 1.13e-2, 7.82E-3, 3.1e-2\n0.3 0.5\n0.4 1.0\n");
    ASSERT_TRUE(e.ok()) << e.error().message;
    EXPECT_DOUBLE_EQ(e.value().mass_kg, 0.0113);
    EXPECT_DOUBLE_EQ(e.value().diameter_m, 0.00782);
}

TEST(ImportParse, DrgHeaderErrors) {
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"", "Empty .drg file."},
        {"CFM name 0.01 0.0078 0.03\n0.3 0.5\n0.4 1.0\n", "Not a .drg file: unexpected header."},
        {"CFM, name, 0.01, 0.0078\n0.3 0.5\n0.4 1.0\n", "Not a .drg file: unexpected header."},
        {"CFM, 0.01, 0.0078, 0.03\n0.3 0.5\n0.4 1.0\n",
         "Not a .drg file: name, mass and diameter are required."},
        {"CFM, , , 0.01, 0.0078, 0.03\n0.3 0.5\n0.4 1.0\n",
         "Not a .drg file: name, mass and diameter are required."},
        {"CFM, name, 0, 0.0078, 0.03\n0.3 0.5\n0.4 1.0\n",
         "Not a .drg file: name, mass and diameter are required."},
        {"CFM, name, 0.01, 0, 0.03\n0.3 0.5\n0.4 1.0\n",
         "Not a .drg file: name, mass and diameter are required."},
        {"CFM, name, 0.01, 0.0078, 0.03\n", "name: not enough drag points."},
        {"CFM, name, 0.01, 0.0078, 0.03\n0.3 0.5\n0.4 0.5\n", "name: not enough drag points."},
        {"Nennstiel, Ball, 0.01, 0.0078, 0.03, Encoded Data\n",
         "This .drg file is encoded and cannot be read."},
    };
    for (const auto& [text, message] : cases) {
        SCOPED_TRACE(text);
        const auto d = ParseDrg(text);
        ASSERT_FALSE(d.ok());
        EXPECT_EQ(d.error().code, ErrorCode::kFormat);
        EXPECT_EQ(d.error().message, message);
    }
}

TEST(ImportParse, ReticleElementsInEveryAngleUnit) {
    const auto r = ParseReticle(R"(<reticle name="Test" size-x="20mil" size-y="10mil"
            zero-x="10mil" zero-y="5mil">
        <elements>
            <reticle-line start-x="-1mil" start-y="0mil" end-x="1mrad" end-y="0mil"
                          line-width="0.05mil" />
            <reticle-circle center-x="0mil" center-y="0mil" radius="3.438moa"
                            line-width="0.1mil" fill="True" />
            <reticle-circle center-x="1mil" center-y="1mil" radius="0.001rad" />
            <reticle-text position-x="2mil" position-y="-2mil" text-height="0.5mil" text="5" />
            <reticle-text position-x="0deg" position-y="0mil" text-height="1mil" />
            <reticle-path fill="true" line-width="0.2mil">
                <elements>
                    <reticle-path-move-to position-x="0mil" position-y="0mil" />
                    <reticle-path-line-to position-x="1mil" position-y="0mil" />
                    <reticle-path-arc position-x="1mil" position-y="1mil" radius="1mil"
                                      clockwise="true" major-arc="false" />
                    <reticle-path-curve position-x="9mil" position-y="9mil" />
                </elements>
            </reticle-path>
            <reticle-path />
            <reticle-rectangle x="1mil" />
        </elements>
        <bdc>
            <bdc position-x="0mil" position-y="-2mil" text-offset="0.5mil" text-height="0.3mil" />
            <bdc position-x="0.0573deg" position-y="-4mil" />
            <other position-x="1mil" />
        </bdc>
    </reticle>)");
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r.value().name, "Test");
    EXPECT_EQ(r.value().units, "mrad");
    EXPECT_EQ(r.value().focal_plane, "ffp");
    EXPECT_EQ(r.value().source, kSourceReticleFile);
    const json def = json::parse(r.value().definition);
    EXPECT_EQ(def.at("size"), json::parse("[20.0, 10.0]"));
    EXPECT_EQ(def.at("zero"), json::parse("[10.0, 5.0]"));

    const json& el = def.at("elements");
    ASSERT_EQ(el.size(), 7U);  // unknown <reticle-rectangle> is ignored
    EXPECT_EQ(el[0].at("t"), "line");
    EXPECT_DOUBLE_EQ(el[0].at("x1").get<double>(), -1.0);
    EXPECT_DOUBLE_EQ(el[0].at("x2").get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(el[0].at("w").get<double>(), 0.05);

    EXPECT_EQ(el[1].at("t"), "circle");
    EXPECT_NEAR(el[1].at("r").get<double>(), 1.0, 1e-3);  // 3.438 MOA = 1 mrad
    EXPECT_DOUBLE_EQ(el[1].at("w").get<double>(), 0.1);
    EXPECT_TRUE(el[1].at("fill").get<bool>());
    EXPECT_NEAR(el[2].at("r").get<double>(), 1.0, 1e-12);  // 0.001 rad
    EXPECT_DOUBLE_EQ(el[2].at("w").get<double>(), 0.0);
    EXPECT_FALSE(el[2].at("fill").get<bool>());

    EXPECT_EQ(el[3].at("t"), "text");
    EXPECT_DOUBLE_EQ(el[3].at("x").get<double>(), 2.0);
    EXPECT_DOUBLE_EQ(el[3].at("y").get<double>(), -2.0);
    EXPECT_DOUBLE_EQ(el[3].at("h").get<double>(), 0.5);
    EXPECT_EQ(el[3].at("s"), "5");
    EXPECT_EQ(el[4].at("s"), "");

    EXPECT_EQ(el[5].at("t"), "path");
    EXPECT_TRUE(el[5].at("fill").get<bool>());
    EXPECT_DOUBLE_EQ(el[5].at("w").get<double>(), 0.2);
    // The unknown step <reticle-path-curve> is dropped.
    EXPECT_EQ(el[5].at("d"), json::parse(R"([["M", 0.0, 0.0], ["L", 1.0, 0.0],
                                             ["A", 1.0, 1.0, 1.0, true, false]])"));
    // A bare path: no steps, the default width.
    EXPECT_FALSE(el[6].at("fill").get<bool>());
    EXPECT_DOUBLE_EQ(el[6].at("w").get<double>(), 0.05);
    EXPECT_TRUE(el[6].at("d").empty());

    const json& bdc = def.at("bdc");
    ASSERT_EQ(bdc.size(), 2U);
    EXPECT_DOUBLE_EQ(bdc[0].at("y").get<double>(), -2.0);
    EXPECT_DOUBLE_EQ(bdc[0].at("offset").get<double>(), 0.5);
    EXPECT_DOUBLE_EQ(bdc[0].at("h").get<double>(), 0.3);
    EXPECT_NEAR(bdc[1].at("x").get<double>(), 1.0, 1e-3);  // 0.0573 deg = 1 mrad
    EXPECT_DOUBLE_EQ(bdc[1].at("h").get<double>(), 0.0);
}

TEST(ImportParse, ReticleDefaultsAndUnreadableAngles) {
    // No name, no sizes, no elements; MOA sizes make an MOA reticle.
    const auto bare = ParseReticle("<reticle />");
    ASSERT_TRUE(bare.ok());
    EXPECT_EQ(bare.value().name, "Reticle");
    EXPECT_EQ(bare.value().units, "mrad");
    EXPECT_EQ(json::parse(bare.value().definition),
              json::parse(R"({"size":[0.0,0.0],"zero":[0.0,0.0],"elements":[],"bdc":[]})"));

    const auto moa = ParseReticle(R"(<reticle size-x="60MOA" size-y="30moa" zero-x="1 furlong"
        zero-y="wide" />)");
    ASSERT_TRUE(moa.ok());
    EXPECT_EQ(moa.value().units, "moa");
    const json def = json::parse(moa.value().definition);
    EXPECT_NEAR(def.at("size")[0].get<double>(), units::RadToMrad(units::MoaToRad(60.0)), 1e-9);
    // Unknown units and non-numbers fall back to 0.
    EXPECT_EQ(def.at("zero"), json::parse("[0.0, 0.0]"));

    for (const char* xml : {"<reticle", "", "<ammo-info-ex />"}) {
        SCOPED_TRACE(xml);
        const auto r = ParseReticle(xml);
        ASSERT_FALSE(r.ok());
        EXPECT_EQ(r.error().code, ErrorCode::kFormat);
        EXPECT_TRUE(Contains(r.error().message, "Not a .reticle file: ")) << r.error().message;
    }
    EXPECT_EQ(ParseReticle("<x/>").error().message, "Not a .reticle file: no <reticle> element.");
    EXPECT_EQ(ParseReticle("<<").error().message, "Not a .reticle file: invalid XML.");
}

// ---- Importers: storing ----------------------------------------------------

class Importers : public ProfileIo {};

const char* const kAmmo =
    R"(<ammo-info-ex name="Gold Medal" caliber=".308" bc="0.475" table="G1"
        bullet-weight="175gr" muzzle-velocity="2600ft/s" bullet-diameter="0.308in" />)";
const char* const kDrg =
    "CFM, .224 Lapua Scenar 4.5g, 0.0045, 0.0057, 0.024, Radar Data\n"
    "0.30 0.5\n0.40 1.0\n0.35 2.0\n";

TEST_F(Importers, AmmoStoresTheBulletAndItsCartridge) {
    const auto id = ImportAmmo(db_, kAmmo);
    ASSERT_TRUE(id.ok()) << id.error().message;
    const auto b = Load<BulletRecord>(id.value());
    EXPECT_EQ(b.name, "Gold Medal");
    EXPECT_DOUBLE_EQ(b.bc.value(), 0.475);
    const auto carts = Repository<CartridgeRecord>(db_).List().value();
    ASSERT_EQ(carts.size(), 1U);
    EXPECT_EQ(carts[0].bullet_id, id.value());
    EXPECT_NEAR(carts[0].muzzle_velocity_mps, units::FpsToMps(2600.0), 1e-9);
}

TEST_F(Importers, AmmoFailuresLeaveNothing) {
    EXPECT_EQ(ImportAmmo(db_, "<nope/>").error().code, ErrorCode::kFormat);
    // Parsed, but the database refuses a weightless bullet ...
    const auto weightless = ImportAmmo(db_, Ammo(R"(name="x" bc="0.3" bullet-weight="0g"
        muzzle-velocity="800m/s" bullet-diameter="7.8mm")"));
    ASSERT_FALSE(weightless.ok());
    EXPECT_NE(weightless.error().code, ErrorCode::kFormat);
    // ... and a cartridge standing still (its bullet is rolled back too).
    const auto still = ImportAmmo(db_, Ammo(R"(name="x" bc="0.3" bullet-weight="10g"
        muzzle-velocity="0m/s" bullet-diameter="7.8mm")"));
    ASSERT_FALSE(still.ok());
    EXPECT_EQ(AllRecords(), 0U);
}

TEST_F(Importers, DrgStoresTheCurveAndItsBullet) {
    const auto id = ImportDrg(db_, kDrg);
    ASSERT_TRUE(id.ok()) << id.error().message;
    const auto b = Load<BulletRecord>(id.value());
    EXPECT_EQ(b.name, ".224 Lapua Scenar 4.5g");
    EXPECT_EQ(b.manufacturer, "Lapua");
    EXPECT_EQ(b.caliber, ".224");  // from 5.7 mm
    EXPECT_DOUBLE_EQ(b.mass_kg, 0.0045);
    EXPECT_DOUBLE_EQ(b.diameter_m, 0.0057);
    EXPECT_DOUBLE_EQ(b.length_m, 0.024);
    EXPECT_EQ(b.drag_kind, "curve");
    EXPECT_EQ(b.drag_table, "");
    EXPECT_EQ(b.source, kSourceDrgFile);
    EXPECT_EQ(b.notes, "Doppler radar drag curve (3 points)");
    const auto c = Load<DragCurveRecord>(*b.curve_id);
    EXPECT_EQ(c.name, b.name);
    EXPECT_EQ(c.notes, "CFM radar drag function");
    EXPECT_EQ(c.source, kSourceDrgFile);
    ASSERT_EQ(c.points.size(), 3U);
    EXPECT_DOUBLE_EQ(c.points[1].mach, 1.0);

    const auto other = ImportDrg(db_, "BRL, Sierra 168, 0.0109, 0.00782, 0.031\n0.3 0.5\n0.4 1\n");
    ASSERT_TRUE(other.ok());
    EXPECT_EQ(Load<BulletRecord>(other.value()).manufacturer, "");
    EXPECT_EQ(Load<BulletRecord>(other.value()).caliber, ".308");
}

TEST_F(Importers, DrgFailuresLeaveNothing) {
    EXPECT_EQ(ImportDrg(db_, "").error().message, "Empty .drg file.");
    // Negative drag: the curve is refused.
    EXPECT_FALSE(ImportDrg(db_, "CFM, neg, 0.01, 0.0078, 0.03\n-0.3 0.5\n0.4 1.0\n").ok());
    // Negative length: the bullet is refused, its curve rolled back.
    EXPECT_FALSE(ImportDrg(db_, "CFM, short, 0.01, 0.0078, -0.03\n0.3 0.5\n0.4 1.0\n").ok());
    EXPECT_EQ(AllRecords(), 0U);
}

TEST_F(Importers, ReticleIsStored) {
    const auto id = ImportReticle(db_, R"(<reticle name="Dot" size-x="10mil" />)");
    ASSERT_TRUE(id.ok());
    EXPECT_EQ(Load<ReticleRecord>(id.value()).name, "Dot");
    EXPECT_EQ(ImportReticle(db_, "<x/>").error().code, ErrorCode::kFormat);
    EXPECT_EQ(Count<ReticleRecord>(), 1U);
}

TEST_F(Importers, ImportsJoinTheCallersTransaction) {
    // With a transaction already open the importers do not start their own:
    // the caller commits or rolls back everything.
    {
        auto txn = sqlite_manager::Transaction::Begin(db_.connection());
        ASSERT_TRUE(txn.ok());
        ASSERT_TRUE(ImportAmmo(db_, kAmmo).ok());
        ASSERT_TRUE(ImportDrg(db_, kDrg).ok());
        EXPECT_TRUE(db_.connection().InTransaction());
        EXPECT_EQ(Count<BulletRecord>(), 2U);
    }  // rolled back
    EXPECT_EQ(AllRecords(), 0U);
}

TEST_F(Importers, ClosedDatabaseFailsEveryImporter) {
    storage::Database closed;
    EXPECT_FALSE(ImportAmmo(closed, kAmmo).ok());
    EXPECT_FALSE(ImportDrg(closed, kDrg).ok());
    EXPECT_FALSE(ImportReticle(closed, "<reticle/>").ok());
    EXPECT_FALSE(ImportFile(closed, "b.json", R"({"format":"balcalc-bullets","bullets":[
        {"name":"b","weight_gr":175,"diameter_in":0.308,"g1":0.5}]})")
                     .ok());
    EXPECT_FALSE(SeededVersion(closed).ok());
    EXPECT_FALSE(SeedLibrary(closed, {}, 1).ok());
    // Parse errors come first.
    EXPECT_EQ(ImportAmmo(closed, "x").error().code, ErrorCode::kFormat);
    EXPECT_EQ(ImportDrg(closed, "").error().code, ErrorCode::kFormat);
}

TEST_F(Importers, PublishedBulletListVariants) {
    const auto last = ImportFile(db_, "Bullets.JSON", R"({"format":"balcalc-bullets","bullets":[
        {"name":"G7 bands","manufacturer":"Berger","caliber":".284","weight_gr":180,
         "diameter_in":0.284,"length_in":1.5,"reference":"catalog",
         "g7_bands":[[2800,0.33],[2000,0.31]]},
        {"name":"G1 bands","weight_gr":175,"diameter_in":0.308,"g1_bands":[[2800,0.505]]},
        {"name":"G7 and G1","weight_gr":140,"diameter_in":0.264,"g7":0.3,"g1":0.61,
         "reference":"web"},
        {"name":"G1 only","weight_gr":55,"diameter_in":0.224,"g1":0.25}]})");
    ASSERT_TRUE(last.ok()) << last.error().message;
    EXPECT_EQ(Load<BulletRecord>(last.value()).name, "G1 only");

    const auto all = Repository<BulletRecord>(db_).List().value();
    ASSERT_EQ(all.size(), 4U);
    for (const BulletRecord& b : all) {
        SCOPED_TRACE(b.name);
        EXPECT_EQ(b.source, kSourcePublished);
        if (b.name == "G7 bands") {
            EXPECT_EQ(b.drag_kind, "multi_bc");
            EXPECT_EQ(b.drag_table, "G7");
            EXPECT_EQ(b.manufacturer, "Berger");
            EXPECT_EQ(b.caliber, ".284");
            EXPECT_EQ(b.notes, "catalog");
            EXPECT_NEAR(b.mass_kg, units::GrainToKg(180.0), 1e-12);
            EXPECT_NEAR(b.diameter_m, units::InchToM(0.284), 1e-12);
            EXPECT_NEAR(b.length_m, units::InchToM(1.5), 1e-12);
            ASSERT_EQ(b.bc_bands.size(), 2U);
        } else if (b.name == "G1 bands") {
            EXPECT_EQ(b.drag_kind, "multi_bc");
            EXPECT_EQ(b.drag_table, "G1");
            ASSERT_EQ(b.bc_bands.size(), 1U);
            EXPECT_NEAR(b.bc_bands[0].velocity_mps, units::FpsToMps(2800.0), 1e-9);
            EXPECT_DOUBLE_EQ(b.length_m, 0.0);
        } else if (b.name == "G7 and G1") {
            // The G7 BC is used, the G1 one kept in the notes.
            EXPECT_EQ(b.drag_kind, "bc");
            EXPECT_EQ(b.drag_table, "G7");
            EXPECT_DOUBLE_EQ(b.bc.value(), 0.3);
            EXPECT_EQ(b.notes, "web\nG1 BC 0.610000");
        } else {
            EXPECT_EQ(b.drag_table, "G1");
            EXPECT_DOUBLE_EQ(b.bc.value(), 0.25);
            EXPECT_EQ(b.notes, "");
        }
    }

    // A G7 and a G1 BC without a reference: the note stands alone.
    ASSERT_TRUE(ImportFile(db_, "x.json", R"({"format":"balcalc-bullets","bullets":[
        {"name":"Both","weight_gr":140,"diameter_in":0.264,"g7":0.3,"g1":0.6}]})")
                    .ok());
    EXPECT_EQ(Repository<BulletRecord>(db_).List("Both").value().at(0).notes, "G1 BC 0.600000");

    // Already there: skipped, nothing new (and no id to return).
    const auto again = ImportFile(db_, "x.json", R"({"format":"balcalc-bullets","bullets":[
        {"name":"G1 only","weight_gr":55,"diameter_in":0.224,"g1":0.25}]})");
    ASSERT_TRUE(again.ok());
    EXPECT_EQ(again.value(), 0);
    EXPECT_EQ(Count<BulletRecord>(), 5U);
}

TEST_F(Importers, MalformedPublishedBulletListIsRefused) {
    const std::vector<const char*> cases = {
        R"({"format":"balcalc-bullets"})",
        R"({"format":"balcalc-bullets","bullets":[{"weight_gr":55,"diameter_in":0.224,"g1":0.25}]})",
        R"({"format":"balcalc-bullets","bullets":[{"name":"x","diameter_in":0.224,"g1":0.25}]})",
        R"({"format":"balcalc-bullets","bullets":[{"name":"x","weight_gr":55,"diameter_in":0.224}]})",
        R"({"format":"balcalc-bullets","bullets":[{"name":"x","weight_gr":"55gr",
            "diameter_in":0.224,"g1":0.25}]})",
        R"({"format":"balcalc-bullets","bullets":[{"name":"x","weight_gr":55,
            "diameter_in":0.224,"g7_bands":[[2800]]}]})",
    };
    for (const char* text : cases) {
        SCOPED_TRACE(text);
        const auto r = ImportFile(db_, "b.json", text);
        ASSERT_FALSE(r.ok());
        EXPECT_EQ(r.error().code, ErrorCode::kFormat);
        EXPECT_TRUE(Contains(r.error().message, "Library list: ")) << r.error().message;
    }
    // The database refuses a weightless bullet.
    const auto weightless = ImportFile(db_, "b.json", R"({"format":"balcalc-bullets","bullets":[
        {"name":"x","weight_gr":0,"diameter_in":0.224,"g1":0.25}]})");
    ASSERT_FALSE(weightless.ok());
    EXPECT_NE(weightless.error().code, ErrorCode::kFormat);
    EXPECT_EQ(Count<BulletRecord>(), 0U);
}

TEST_F(Importers, ShareFilesImportThroughImportFile) {
    const Id rifle = Store(Rifle(std::nullopt));
    const Id cartridge = Store(Cartridge(Store(Bullet())));
    const auto r = ImportFile(db_, "rifle.json", ExportRifleJson(db_, rifle).value());
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(Load<RifleRecord>(r.value()).name, "Tikka (2)");
    const auto c = ImportFile(db_, "load.json", ExportCartridgeJson(db_, cartridge).value());
    ASSERT_TRUE(c.ok()) << c.error().message;
    EXPECT_EQ(Load<CartridgeRecord>(c.value()).name, "Match load (2)");

    // Not a share file either: the share importer's reason.
    for (const char* text : {"garbage", "[]", R"({"format":"other"})"}) {
        SCOPED_TRACE(text);
        const auto bad = ImportFile(db_, "x.json", text);
        ASSERT_FALSE(bad.ok());
        EXPECT_TRUE(Contains(bad.error().message, "Not a valid Holdmark file"))
            << bad.error().message;
    }
    const auto unknown = ImportFile(db_, "notes.TXT", "hello");
    EXPECT_EQ(unknown.error().message, "Unknown file type: notes.TXT");
    EXPECT_EQ(ImportFile(db_, "json", "{}").error().message, "Unknown file type: json");
    EXPECT_TRUE(ImportFile(db_, "drag.DRG", kDrg).ok());
}

// Same for ImportFile: {"format": 5} fails, it does not throw.
TEST_F(Importers, ImportFileRefusesMistypedFormat) {
    Result<Id> r = Id{0};
    ASSERT_NO_THROW(r = ImportFile(db_, "x.json", R"({"format": 5})"));
    EXPECT_FALSE(r.ok());
}

// ---- Seeding ---------------------------------------------------------------

TEST_F(Importers, SeedReportsWhatItCouldNotRead) {
    const std::vector<SeedFile> files = {
        {"a.ammo", kAmmo},
        {"broken.ammo", "<ammo-info-ex name=\"broken\" />"},
        {"encoded.drg", "Nennstiel, Ball, 0.01, 0.0078, 0.03, Encoded Data\n"},
        {"scenar.drg", kDrg},
        {"dot.reticle", R"(<reticle name="Dot" />)"},
        {"bad.reticle", "<x/>"},
        {"bullets.json", R"({"format":"balcalc-bullets","bullets":[
            {"name":"P","weight_gr":55,"diameter_in":0.224,"g1":0.25},
            {"name":"Zero","weight_gr":0,"diameter_in":0.224,"g1":0.25}]})"},
        {"typo.json", R"({"format":"balcalc-bullets","bullets":[{"name":"Q"}]})"},
        {"readme.txt", "hello"},
    };
    const auto report = SeedLibrary(db_, files, 3);
    ASSERT_TRUE(report.ok()) << report.error().message;
    // a.ammo, scenar.drg, dot.reticle and bullet P.
    EXPECT_EQ(report.value().imported, 4);
    // broken.ammo, encoded.drg, bad.reticle, readme.txt (the encoded .drg
    // without a problem: it is expected).
    EXPECT_EQ(report.value().skipped, 4);
    const std::vector<std::string>& problems = report.value().problems;
    ASSERT_EQ(problems.size(), 5U);
    EXPECT_EQ(problems[0], "broken.ammo: broken: bullet weight is missing or has unknown units.");
    EXPECT_EQ(problems[1], "bad.reticle: Not a .reticle file: no <reticle> element.");
    EXPECT_TRUE(Contains(problems[2], "bullets.json: ")) << problems[2];
    EXPECT_TRUE(Contains(problems[3], "typo.json: ")) << problems[3];
    EXPECT_TRUE(Contains(problems[3], "weight_gr")) << problems[3];
    EXPECT_EQ(problems[4], "readme.txt: Unknown file type: readme.txt");
    EXPECT_EQ(SeededVersion(db_).value(), 3);
    EXPECT_EQ(Count<BulletRecord>(), 3U);

    // An older version is not seeded again.
    const auto older = SeedLibrary(db_, files, 2);
    EXPECT_EQ(older.value().imported, 0);
    EXPECT_TRUE(older.value().problems.empty());
}

TEST_F(Importers, SeedFailsWholeWhenTheVersionCannotBeSaved) {
    ASSERT_TRUE(db_.connection()
                    .Execute("CREATE TEMP TRIGGER no_settings BEFORE INSERT ON main.app_setting "
                             "BEGIN SELECT RAISE(ABORT, 'disk full'); END;")
                    .ok());
    const auto report = SeedLibrary(db_, {{"a.ammo", kAmmo}}, 1);
    ASSERT_FALSE(report.ok());
    EXPECT_TRUE(Contains(report.error().message, "disk full")) << report.error().message;
    // Rolled back: the next start tries again.
    EXPECT_EQ(AllRecords(), 0U);
    EXPECT_EQ(SeededVersion(db_).value(), 0);
}

TEST_F(Importers, SeededVersionReadsTheSetting) {
    ASSERT_TRUE(storage::SetSetting(db_, "seed.version", "7").ok());
    EXPECT_EQ(SeededVersion(db_).value(), 7);
    const auto report = SeedLibrary(db_, {{"a.ammo", kAmmo}}, 7);
    EXPECT_EQ(report.value().imported, 0);
    EXPECT_EQ(Count<BulletRecord>(), 0U);
}

// A database file another connection is reading: writes go through, but
// the commit cannot take the file and fails (kBusy) - everything is rolled
// back.
class LockedDatabase : public ::testing::Test {
   protected:
    void SetUp() override {
        path_ = std::filesystem::temp_directory_path() /
                ("holdmark_profile_io_" + std::to_string(std::random_device{}()) + ".db");
        ASSERT_TRUE(db_.Open(path_.string()).ok());
        ASSERT_TRUE(db_.connection().BusyTimeout(0).ok());  // fail at once
        ASSERT_TRUE(reader_.Open(path_.string()).ok());
    }
    void TearDown() override {
        static_cast<void>(reader_.Close());
        static_cast<void>(db_.connection().Close());
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    void StartReading() {
        ASSERT_TRUE(reader_.Execute("BEGIN; SELECT count(*) FROM bullet;").ok());
    }
    std::size_t BulletsAfterReading() {
        EXPECT_TRUE(reader_.Execute("COMMIT").ok());
        return Repository<BulletRecord>(db_).List().value().size();
    }

    std::filesystem::path path_;
    storage::Database db_;
    sqlite_manager::Connection reader_;
};

TEST_F(LockedDatabase, ImportersFailWhenTheCommitCannotTakeTheFile) {
    StartReading();
    const auto ammo = ImportAmmo(db_, kAmmo);
    ASSERT_FALSE(ammo.ok());
    EXPECT_EQ(ammo.error().code, ErrorCode::kBusy);
    const auto drg = ImportDrg(db_, kDrg);
    ASSERT_FALSE(drg.ok());
    EXPECT_EQ(drg.error().code, ErrorCode::kBusy);
    const auto seed = SeedLibrary(db_, {{"a.ammo", kAmmo}}, 1);
    ASSERT_FALSE(seed.ok());
    EXPECT_EQ(seed.error().code, ErrorCode::kBusy);
    EXPECT_FALSE(db_.connection().InTransaction());
    EXPECT_EQ(BulletsAfterReading(), 0U);
    EXPECT_EQ(SeededVersion(db_).value(), 0);
    // Once the reader is done the same imports succeed.
    EXPECT_TRUE(ImportAmmo(db_, kAmmo).ok());
}

TEST_F(LockedDatabase, ShareImportFailsWhenTheCommitCannotTakeTheFile) {
    const std::string text = R"({"format":"balcalc-cartridge","version":1,
        "cartridge": {"name": "C", "muzzle_velocity_mps": 800.0},
        "bullet": {"name": "B", "diameter_m": 0.0078, "mass_kg": 0.011,
                   "drag_kind": "bc", "bc": 0.3}})";
    StartReading();
    const auto im = ImportShareJson(db_, text);
    ASSERT_FALSE(im.ok());
    EXPECT_EQ(im.error().code, ErrorCode::kBusy);
    EXPECT_FALSE(db_.connection().InTransaction());
    EXPECT_EQ(BulletsAfterReading(), 0U);
    EXPECT_TRUE(ImportShareJson(db_, text).ok());
}

// A rifle or cartridge share file among the seed files imports like it does
// through ImportFile, inside the seed's transaction.
TEST_F(Importers, SeedImportsShareFiles) {
    const std::string rifle = ExportRifleJson(db_, Store(Rifle(std::nullopt))).value();
    const auto report = SeedLibrary(db_, {{"rifle.json", rifle}}, 1);
    ASSERT_TRUE(report.ok()) << report.error().message;
    EXPECT_TRUE(report.value().problems.empty()) << report.value().problems.front();
    EXPECT_EQ(report.value().imported, 1);
    EXPECT_EQ(Count<RifleRecord>(), 2U);
}

// A share file that fails half-way in the seed leaves none of itself behind
// (its savepoint is rolled back); the seed's other files still import.
TEST_F(Importers, SeedRollsBackAShareFileThatFails) {
    const std::string rifle = ExportRifleJson(db_, Store(Rifle(std::nullopt))).value();
    // The bullet is stored first, then the cartridge standing still is refused.
    const std::string standing =
        R"({"format":"balcalc-cartridge","cartridge":{"name":"C","muzzle_velocity_mps":0},
            "bullet":{"name":"B","diameter_m":0.0078,"mass_kg":0.011,"drag_kind":"bc","bc":0.3}})";
    const std::size_t bullets = Count<BulletRecord>();
    const auto report = SeedLibrary(db_, {{"standing.json", standing}, {"rifle.json", rifle}}, 1);
    ASSERT_TRUE(report.ok()) << report.error().message;
    EXPECT_EQ(report.value().imported, 1);
    EXPECT_EQ(report.value().skipped, 1);
    ASSERT_EQ(report.value().problems.size(), 1U);
    EXPECT_EQ(report.value().problems.front().rfind("standing.json: ", 0), 0U);
    EXPECT_EQ(Count<BulletRecord>(), bullets);
    EXPECT_EQ(Count<CartridgeRecord>(), 0U);
    EXPECT_EQ(Count<RifleRecord>(), 2U);
    EXPECT_FALSE(db_.connection().InTransaction());
}

// Any .json seed file is either imported or reported, even one that is
// not a JSON object.
TEST_F(Importers, SeedReportsJsonThatIsNotAnObject) {
    Result<SeedReport> report = SeedReport{};
    ASSERT_NO_THROW(report = SeedLibrary(db_, {{"list.json", "[1, 2]"}}, 1));
    ASSERT_TRUE(report.ok());
    EXPECT_EQ(report.value().skipped, 1);
    EXPECT_EQ(report.value().problems.size(), 1U);
}

}  // namespace
}  // namespace ballistics::applogic
