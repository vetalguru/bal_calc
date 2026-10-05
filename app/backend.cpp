#include "backend.h"

#include <algorithm>

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QStandardPaths>

#include <ballistics/applogic/importers.h>
#include <ballistics/applogic/library.h>
#include <ballistics/applogic/profile_form.h>
#include <ballistics/applogic/profile_io.h>
#include <ballistics/applogic/reticle.h>
#include <ballistics/atmosphere.h>
#include <ballistics/storage/repository.h>
#include <ballistics/storage/solution.h>
#include <ballistics/units.h>
#include <ballistics/version.h>

namespace al = ballistics::applogic;
namespace bs = ballistics::storage;

namespace {

constexpr const char* kCurrentProfileKey = "ui.current_profile";
constexpr const char* kAngleUnitKey = "ui.angle_unit";
constexpr const char* kLanguageKey = "ui.language";
constexpr const char* kHoldModeKey = "ui.hold_mode";
// Bump when data/seed gains files; existing records are kept.
constexpr int kSeedVersion = 1;
constexpr const char* kTableFromKey = "ui.table.from_m";
constexpr const char* kTableToKey = "ui.table.to_m";
constexpr const char* kTableStepKey = "ui.table.step_m";

QString Q(const std::string& s) { return QString::fromStdString(s); }

// Messages produced by the toolkit-free layers, listed so lupdate picks
// them up; shown through Tr().
[[maybe_unused]] constexpr const char* kLogicMessages[] = {
    QT_TRANSLATE_NOOP("Logic", "Enter a profile name."),
    QT_TRANSLATE_NOOP("Logic", "Muzzle velocity must be between 50 and 2000 m/s."),
    QT_TRANSLATE_NOOP("Logic", "Ballistic coefficient must be between 0 and 2."),
    QT_TRANSLATE_NOOP("Logic", "Enter the bullet weight."),
    QT_TRANSLATE_NOOP("Logic", "Bullet diameter must be between 0 and 1 inch."),
    QT_TRANSLATE_NOOP("Logic", "Bullet length and twist cannot be negative."),
    QT_TRANSLATE_NOOP("Logic", "Zero range must be between 10 and 1000 m."),
    QT_TRANSLATE_NOOP("Logic", "Enter the scope click value."),
    QT_TRANSLATE_NOOP("Logic", "Zero pressure must be between 300 and 1200 hPa."),
    QT_TRANSLATE_NOOP("Logic", "Humidity must be between 0 and 100 %."),
    QT_TRANSLATE_NOOP("Logic", "Enter a target range."),
    QT_TRANSLATE_NOOP("Logic", "The bullet does not reach this range."),
    QT_TRANSLATE_NOOP("Logic", "Check the table range and step."),
    QT_TRANSLATE_NOOP("Logic", "Check the scope magnification range."),
    QT_TRANSLATE_NOOP("Logic", "Enter the bullet name."),
    QT_TRANSLATE_NOOP("Logic", "Log at least one shot to true the profile."),
    QT_TRANSLATE_NOOP("Logic", "The bullet does not reach one of the logged ranges."),
    QT_TRANSLATE_NOOP("Logic", "Nothing to apply."),
    QT_TRANSLATE_NOOP("Logic", "Each BC band needs a velocity and a BC between 0 and 2."),
    QT_TRANSLATE_NOOP("Logic", "This bullet is used by a cartridge and cannot be deleted."),
};

// A message from applogic/storage in the UI language (unknown ones as is).
QString Tr(const std::string& message) {
    return QCoreApplication::translate("Logic", message.c_str());
}
std::string S(const QVariant& v) { return v.toString().toStdString(); }

QVariantMap ToMap(const al::ProfileForm& f) {
    return {
        {"profileId", static_cast<qlonglong>(f.profile_id)},
        {"libraryBulletId", static_cast<qlonglong>(f.library_bullet_id)},
        {"name", Q(f.name)},
        {"caliber", Q(f.caliber)},
        {"sightHeightCm", f.sight_height_cm},
        {"twistIn", f.twist_in},
        {"twistLeft", f.twist_left},
        {"clickUnits", Q(f.click_units)},
        {"clickValue", f.click_value},
        {"reticleId", static_cast<qlonglong>(f.reticle_id)},
        {"focalPlane", Q(f.focal_plane)},
        {"sfpReferenceMagnification", f.sfp_reference_magnification},
        {"minMagnification", f.min_magnification},
        {"maxMagnification", f.max_magnification},
        {"bulletName", Q(f.bullet_name)},
        {"dragTable", Q(f.drag_table)},
        {"bc", f.bc},
        {"massGr", f.mass_gr},
        {"diameterIn", f.diameter_in},
        {"lengthIn", f.length_in},
        {"muzzleVelocity", f.muzzle_velocity_mps},
        {"powderReferenceC", f.powder_reference_c},
        {"powderSensitivity", f.powder_sensitivity_pct_per_c},
        {"zeroRangeM", f.zero_range_m},
        {"zeroOffsetUpCm", f.zero_offset_up_cm},
        {"zeroOffsetRightCm", f.zero_offset_right_cm},
        {"zeroTemperatureC", f.zero_temperature_c},
        {"zeroPressureHpa", f.zero_pressure_hpa},
        {"zeroAltitudeM", f.zero_altitude_m},
        {"zeroHumidityPct", f.zero_humidity_pct},
        {"zeroPowderC", f.zero_powder_c},
    };
}

al::ProfileForm FromMap(const QVariantMap& m) {
    al::ProfileForm f;
    f.profile_id = m.value("profileId").toLongLong();
    f.library_bullet_id = m.value("libraryBulletId").toLongLong();
    f.name = S(m.value("name"));
    f.caliber = S(m.value("caliber"));
    f.sight_height_cm = m.value("sightHeightCm").toDouble();
    f.twist_in = m.value("twistIn").toDouble();
    f.twist_left = m.value("twistLeft").toBool();
    f.click_units = S(m.value("clickUnits"));
    f.click_value = m.value("clickValue").toDouble();
    f.reticle_id = m.value("reticleId").toLongLong();
    f.focal_plane = m.contains("focalPlane") ? S(m.value("focalPlane")) : std::string("ffp");
    f.sfp_reference_magnification = m.value("sfpReferenceMagnification").toDouble();
    f.min_magnification = m.value("minMagnification").toDouble();
    f.max_magnification = m.value("maxMagnification").toDouble();
    f.bullet_name = S(m.value("bulletName"));
    f.drag_table = S(m.value("dragTable"));
    f.bc = m.value("bc").toDouble();
    f.mass_gr = m.value("massGr").toDouble();
    f.diameter_in = m.value("diameterIn").toDouble();
    f.length_in = m.value("lengthIn").toDouble();
    f.muzzle_velocity_mps = m.value("muzzleVelocity").toDouble();
    f.powder_reference_c = m.value("powderReferenceC").toDouble();
    f.powder_sensitivity_pct_per_c = m.value("powderSensitivity").toDouble();
    f.zero_range_m = m.value("zeroRangeM").toDouble();
    f.zero_offset_up_cm = m.value("zeroOffsetUpCm").toDouble();
    f.zero_offset_right_cm = m.value("zeroOffsetRightCm").toDouble();
    f.zero_temperature_c = m.value("zeroTemperatureC").toDouble();
    f.zero_pressure_hpa = m.value("zeroPressureHpa").toDouble();
    f.zero_altitude_m = m.value("zeroAltitudeM").toDouble();
    f.zero_humidity_pct = m.value("zeroHumidityPct").toDouble();
    f.zero_powder_c = m.value("zeroPowderC").toDouble();
    return f;
}

QVariantMap ToMap(const al::BulletForm& f) {
    QVariantList bands;
    for (const al::BcBand& b : f.bands) {
        bands.push_back(QVariantMap{{"velocity", b.velocity_mps}, {"bc", b.bc}});
    }
    return {{"id", static_cast<qlonglong>(f.id)},
            {"name", Q(f.name)},
            {"manufacturer", Q(f.manufacturer)},
            {"caliber", Q(f.caliber)},
            {"massGr", f.mass_gr},
            {"diameterIn", f.diameter_in},
            {"lengthIn", f.length_in},
            {"dragTable", Q(f.drag_table)},
            {"bc", f.bc},
            {"bands", bands},
            {"notes", Q(f.notes)},
            {"source", Q(f.source)},
            {"hasCustomCurve", f.has_custom_curve}};
}

al::BulletForm BulletFromMap(const QVariantMap& m) {
    al::BulletForm f;
    f.id = m.value("id").toLongLong();
    f.name = S(m.value("name"));
    f.manufacturer = S(m.value("manufacturer"));
    f.caliber = S(m.value("caliber"));
    f.mass_gr = m.value("massGr").toDouble();
    f.diameter_in = m.value("diameterIn").toDouble();
    f.length_in = m.value("lengthIn").toDouble();
    f.drag_table = S(m.value("dragTable"));
    f.bc = m.value("bc").toDouble();
    for (const QVariant& v : m.value("bands").toList()) {
        const QVariantMap b = v.toMap();
        f.bands.push_back({b.value("velocity").toDouble(), b.value("bc").toDouble()});
    }
    f.notes = S(m.value("notes"));
    f.source = m.contains("source") ? S(m.value("source")) : std::string(al::kSourceLibrary);
    f.has_custom_curve = m.value("hasCustomCurve").toBool();
    return f;
}

// QFile understands both local paths and Android content:// URLs.
QString FilePath(const QUrl& url) { return url.isLocalFile() ? url.toLocalFile() : url.toString(); }

} // namespace

Backend::Backend(QObject* parent) : QObject(parent) {
    recompute_timer_.setSingleShot(true);
    recompute_timer_.setInterval(0);
    connect(&recompute_timer_, &QTimer::timeout, this, &Backend::Recompute);
    save_timer_.setSingleShot(true);
    save_timer_.setInterval(500);
    connect(&save_timer_, &QTimer::timeout, this, [this] { al::SaveSession(db_, Session()).ok(); });
    connect(this, &Backend::conditionsChanged, this, [this] {
        recompute_timer_.start();
        save_timer_.start();
    });

    // BALCALC_DB points the app at another database file (tests, demos).
    db_path_ = qEnvironmentVariable("BALCALC_DB");
    if (db_path_.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        db_path_ = QDir(dir).filePath("balcalc.db");
    }
    if (auto s = db_.Open(db_path_.toStdString()); !s) {
        db_error_ = Q(s.error().message);
        return;
    }
    SeedStarterLibrary();
    if (auto session = al::LoadSession(db_); session) {
        ApplySession(session.value());
    }
    if (auto unit = bs::GetSetting(db_, kAngleUnitKey); unit && unit.value()) {
        angle_unit_ = Q(*unit.value());
    }
    if (auto mode = bs::GetSetting(db_, kHoldModeKey); mode && mode.value()) {
        hold_mode_ = Q(*mode.value());
    }
    if (auto lang = bs::GetSetting(db_, kLanguageKey); lang && lang.value()) {
        language_ = Q(*lang.value());
    }
    InstallTranslator();
    for (const auto& [key, value] : {std::pair{kTableFromKey, &table_from_m_},
                                     std::pair{kTableToKey, &table_to_m_},
                                     std::pair{kTableStepKey, &table_step_m_}}) {
        if (auto v = bs::GetSetting(db_, key); v && v.value()) {
            bool ok = false;
            const double d = QString::fromStdString(*v.value()).toDouble(&ok);
            if (ok) {
                *value = d;
            }
        }
    }
    connect(this, &Backend::tableSpecChanged, this, [this] {
        bs::SetSetting(db_, kTableFromKey, std::to_string(table_from_m_)).ok();
        bs::SetSetting(db_, kTableToKey, std::to_string(table_to_m_)).ok();
        bs::SetSetting(db_, kTableStepKey, std::to_string(table_step_m_)).ok();
    });
    ReloadProfiles();
    if (auto cur = bs::GetSetting(db_, kCurrentProfileKey); cur && cur.value()) {
        current_profile_id_ = QString::fromStdString(*cur.value()).toInt();
    }
    bool found = false;
    for (const QVariant& p : profiles_) {
        found = found || p.toMap().value("id").toInt() == current_profile_id_;
    }
    if (!found) {
        current_profile_id_ = profiles_.isEmpty() ? 0 : profiles_.front().toMap().value("id").toInt();
    }
    Recompute();
}

QString Backend::engineVersion() const { return QString::fromLatin1(ballistics::version()); }

QString Backend::sqliteVersion() const { return QString::fromLatin1(bs::SqliteVersion()); }

void Backend::setCurrentProfileId(int id) {
    if (id == current_profile_id_) {
        return;
    }
    current_profile_id_ = id;
    bs::SetSetting(db_, kCurrentProfileKey, std::to_string(id)).ok();
    emit currentProfileIdChanged();
    recompute_timer_.start();
}

void Backend::setAngleUnit(const QString& unit) {
    if (unit == angle_unit_ || (unit != "mrad" && unit != "moa")) {
        return;
    }
    angle_unit_ = unit;
    bs::SetSetting(db_, kAngleUnitKey, unit.toStdString()).ok();
    emit angleUnitChanged();
    recompute_timer_.start();
}

void Backend::setHoldMode(const QString& mode) {
    if (mode == hold_mode_) {
        return;
    }
    hold_mode_ = QString::fromLatin1(al::ToString(al::HoldModeFromString(mode.toStdString())));
    bs::SetSetting(db_, kHoldModeKey, hold_mode_.toStdString()).ok();
    emit holdModeChanged();
    recompute_timer_.start();
}

QVariantList Backend::shots() {
    QVariantList out;
    if (current_profile_id_ == 0) {
        return out;
    }
    auto list = al::ListShots(db_, current_profile_id_);
    if (!list) {
        return out;
    }
    const auto unit = angle_unit_ == "moa" ? al::AngleUnit::kMoa : al::AngleUnit::kMrad;
    for (const auto& d : list.value()) {
        out.push_back(QVariantMap{
            {"id", static_cast<qlonglong>(d.id)},
            {"rangeM", d.range_m},
            {"observed", al::FromRad(d.observed_elevation_rad, unit)},
            {"predicted", d.predicted_elevation_rad ? al::FromRad(*d.predicted_elevation_rad, unit)
                                                    : QVariant()},
            {"hasWindage", d.observed_windage_rad.has_value()},
            {"observedWindage", al::FromRad(d.observed_windage_rad.value_or(0.0), unit)},
            {"shotAt", Q(d.shot_at)},
            {"used", d.use_for_truing},
            {"notes", Q(d.notes)},
            {"temperatureC", ballistics::units::KToC(d.atmosphere.temperature_k)}});
    }
    return out;
}

QString Backend::logShot(double range_m, double elevation, bool has_windage, double windage,
                         const QString& notes) {
    if (current_profile_id_ == 0) {
        return tr("Create a profile to get a solution.");
    }
    const double unit_rad = angle_unit_ == "moa" ? ballistics::units::MoaToRad(1.0)
                                                  : ballistics::units::MradToRad(1.0);
    std::optional<double> wind;
    if (has_windage) {
        wind = windage * unit_rad;
    }
    al::SessionConditions s = Session();
    auto id = al::LogShot(db_, current_profile_id_, s, range_m, elevation * unit_rad, wind,
                          notes.toStdString());
    if (!id) {
        return Tr(id.error().message);
    }
    emit shotsChanged();
    return {};
}

QString Backend::deleteShot(int id) {
    if (auto s = al::DeleteShot(db_, id); !s) {
        return Q(s.error().message);
    }
    emit shotsChanged();
    return {};
}

QString Backend::setShotUsed(int id, bool used) {
    if (auto s = al::SetShotUsedForTruing(db_, id, used); !s) {
        return Q(s.error().message);
    }
    emit shotsChanged();
    return {};
}

QVariantMap Backend::computeTruing() {
    last_truing_ = al::ComputeTruing(db_, current_profile_id_);
    const al::TruingResult& r = last_truing_;
    const auto unit = angle_unit_ == "moa" ? al::AngleUnit::kMoa : al::AngleUnit::kMrad;
    QVariantList points;
    for (const auto& p : r.points) {
        points.push_back(QVariantMap{{"rangeM", p.range_m},
                                     {"observed", al::FromRad(p.observed_rad, unit)},
                                     {"before", al::FromRad(p.predicted_before_rad, unit)},
                                     {"after", al::FromRad(p.predicted_after_rad, unit)}});
    }
    return {{"ok", r.ok},
            {"error", Tr(r.error)},
            {"velocityScale", r.velocity_scale},
            {"dragScale", r.drag_scale},
            {"dragFitted", r.drag_fitted},
            {"rmsBefore", al::FromRad(r.rms_before_rad, unit)},
            {"rmsAfter", al::FromRad(r.rms_after_rad, unit)},
            {"velocityBefore", r.muzzle_velocity_before_mps},
            {"velocityAfter", r.muzzle_velocity_after_mps},
            {"points", points}};
}

QString Backend::applyTruing() {
    if (auto s = al::ApplyTruing(db_, current_profile_id_, last_truing_); !s) {
        return Tr(s.error().message);
    }
    last_truing_ = {};
    emit shotsChanged();
    recompute_timer_.start();
    return {};
}

QString Backend::resetTruing() {
    if (auto s = al::ResetTruing(db_, current_profile_id_); !s) {
        return Tr(s.error().message);
    }
    emit shotsChanged();
    recompute_timer_.start();
    return {};
}

QVariantList Backend::reticles() {
    QVariantList out;
    auto list = bs::Repository<bs::ReticleRecord>(db_).List();
    if (list) {
        for (const auto& r : list.value()) {
            out.push_back(QVariantMap{{"id", static_cast<qlonglong>(r.id)},
                                      {"name", Q(r.name)},
                                      {"units", Q(r.units)}});
        }
    }
    return out;
}

void Backend::setLanguage(const QString& language) {
    if (language == language_) {
        return;
    }
    language_ = language;
    bs::SetSetting(db_, kLanguageKey, language.toStdString()).ok();
    InstallTranslator();
    emit languageChanged();
}

void Backend::InstallTranslator() {
    QCoreApplication::removeTranslator(&translator_);
    QString lang = language_;
    if (lang.isEmpty()) {
        // System language: Ukrainian or Russian when the UI languages ask
        // for them, English otherwise.
        for (const QString& ui : QLocale::system().uiLanguages()) {
            if (ui.startsWith("uk") || ui.startsWith("ru") || ui.startsWith("en")) {
                lang = ui.left(2);
                break;
            }
        }
    }
    if (lang != "en" && !lang.isEmpty() &&
        translator_.load(QStringLiteral(":/i18n/balcalc_%1.qm").arg(lang))) {
        QCoreApplication::installTranslator(&translator_);
    }
    // Re-evaluate qsTr() bindings once QML has finished loading.
    QTimer::singleShot(0, this, [this] {
        if (QQmlEngine* engine = qmlEngine(this)) {
            engine->retranslate();
        }
        Recompute(); // messages from C++ are translated too
    });
}

void Backend::ReloadProfiles() {
    profiles_.clear();
    auto list = bs::Repository<bs::ProfileRecord>(db_).List();
    if (list) {
        for (const auto& p : list.value()) {
            profiles_.push_back(QVariantMap{{"id", static_cast<int>(p.id)},
                                            {"name", Q(p.name)},
                                            {"zeroRangeM", p.zero_range_m}});
        }
    }
    emit profilesChanged();
}

QVariantMap Backend::profileForm(int id) {
    if (id == 0) {
        return ToMap(al::ProfileForm{});
    }
    auto f = al::LoadProfileForm(db_, id);
    return f ? ToMap(f.value()) : ToMap(al::ProfileForm{});
}

QString Backend::saveProfile(const QVariantMap& form) {
    auto id = al::SaveProfileForm(db_, FromMap(form));
    if (!id) {
        return Tr(id.error().message);
    }
    ReloadProfiles();
    setCurrentProfileId(static_cast<int>(id.value()));
    recompute_timer_.start();
    return {};
}

QString Backend::deleteProfile(int id) {
    if (auto s = al::DeleteProfile(db_, id); !s) {
        return Q(s.error().message);
    }
    ReloadProfiles();
    if (id == current_profile_id_) {
        setCurrentProfileId(profiles_.isEmpty() ? 0 : profiles_.front().toMap().value("id").toInt());
    }
    recompute_timer_.start();
    return {};
}

QVariantMap Backend::Table(double from_m, double to_m, double step_m) {
    QVariantMap out;
    if (current_profile_id_ == 0) {
        out["ok"] = false;
        out["error"] = tr("Create a profile to get a solution.");
        return out;
    }
    auto p = bs::LoadProfile(db_, current_profile_id_);
    if (!p) {
        out["ok"] = false;
        out["error"] = Q(p.error().message);
        return out;
    }
    const auto unit = angle_unit_ == "moa" ? al::AngleUnit::kMoa : al::AngleUnit::kMrad;
    const al::RangeTable t = al::BuildRangeTable(p.value(), Session(), unit, from_m, to_m, step_m);
    QVariantList rows;
    for (const al::RangeRow& r : t.rows) {
        rows.push_back(QVariantMap{{"rangeM", r.range_m},
                                   {"elevation", r.elevation},
                                   {"windage", r.windage},
                                   {"elevationClicks", r.elevation_clicks},
                                   {"windageClicks", r.windage_clicks},
                                   {"dropCm", r.drop_cm},
                                   {"windageCm", r.windage_cm},
                                   {"velocity", r.velocity_mps},
                                   {"mach", r.mach},
                                   {"energy", r.energy_j},
                                   {"time", r.time_s}});
    }
    out["ok"] = t.ok;
    out["error"] = Tr(t.error);
    out["hasScope"] = p.value().scope.has_value();
    out["rows"] = rows;
    return out;
}

QVariantMap Backend::rangeTable() { return Table(table_from_m_, table_to_m_, table_step_m_); }

QVariantMap Backend::trajectoryCurve(double max_range_m, int points) {
    points = std::clamp(points, 10, 1000);
    return Table(0.0, max_range_m, max_range_m / points);
}

QVariantMap Backend::profileFormWithBullet(const QVariantMap& form, int bullet_id) {
    auto f = al::WithLibraryBullet(db_, FromMap(form), bullet_id);
    return f ? ToMap(f.value()) : form;
}

QVariantList Backend::libraryBullets(const QString& filter) {
    QVariantList out;
    auto list = al::ListLibraryBullets(db_, filter.toStdString());
    if (!list) {
        return out;
    }
    for (const al::BulletSummary& b : list.value()) {
        out.push_back(QVariantMap{{"id", static_cast<qlonglong>(b.id)},
                                  {"name", Q(b.name)},
                                  {"manufacturer", Q(b.manufacturer)},
                                  {"caliber", Q(b.caliber)},
                                  {"massGr", b.mass_gr},
                                  {"diameterIn", b.diameter_in},
                                  {"dragKind", Q(b.drag_kind)},
                                  {"dragTable", Q(b.drag_table)},
                                  {"bc", b.bc},
                                  {"bcBands", b.bc_bands},
                                  {"source", Q(b.source)}});
    }
    return out;
}

QVariantMap Backend::bulletForm(int id) {
    if (id == 0) {
        return ToMap(al::BulletForm{});
    }
    auto f = al::LoadBulletForm(db_, id);
    return f ? ToMap(f.value()) : ToMap(al::BulletForm{});
}

QString Backend::saveBullet(const QVariantMap& form) {
    auto id = al::SaveBulletForm(db_, BulletFromMap(form));
    if (!id) {
        return Tr(id.error().message);
    }
    emit libraryChanged();
    recompute_timer_.start(); // a profile may use this bullet
    return {};
}

QString Backend::deleteBullet(int id) {
    if (auto s = al::DeleteBullet(db_, id); !s) {
        return Tr(s.error().message);
    }
    emit libraryChanged();
    return {};
}

QString Backend::profileFileName(int id) const {
    for (const QVariant& p : profiles_) {
        const QVariantMap m = p.toMap();
        if (m.value("id").toInt() == id) {
            QString name = m.value("name").toString();
            static const QRegularExpression kUnsafe(QStringLiteral("[\\\\/:*?\"<>|]+"));
            name.replace(kUnsafe, QStringLiteral("_"));
            return name + QStringLiteral(".balcalc.json");
        }
    }
    return QStringLiteral("profile.balcalc.json");
}

QString Backend::exportProfile(int id, const QUrl& file) {
    auto json = al::ExportProfileJson(db_, id);
    if (!json) {
        return Q(json.error().message);
    }
    QFile f(FilePath(file));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return tr("Cannot write %1: %2").arg(file.toDisplayString(), f.errorString());
    }
    f.write(QByteArray::fromStdString(json.value()));
    return {};
}

QString Backend::ImportJson(const std::string& json) {
    auto id = al::ImportProfileJson(db_, json);
    if (!id) {
        return Tr(id.error().message);
    }
    ReloadProfiles();
    emit libraryChanged();
    setCurrentProfileId(static_cast<int>(id.value()));
    recompute_timer_.start();
    return {};
}

void Backend::SeedStarterLibrary() {
    std::vector<al::SeedFile> files;
    QDirIterator it(QStringLiteral(":/seed"), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFile f(it.next());
        if (f.open(QIODevice::ReadOnly)) {
            files.push_back({QFileInfo(f.fileName()).fileName().toStdString(), f.readAll().toStdString()});
        }
    }
    auto report = al::SeedLibrary(db_, files, kSeedVersion);
    if (!report) {
        seed_report_ = Q(report.error().message);
        return;
    }
    if (report.value().imported > 0) {
        seed_report_ = tr("Starter library: %1 records imported.").arg(report.value().imported);
    }
}

QString Backend::importFiles(const QList<QUrl>& files) {
    int imported = 0;
    QStringList problems;
    for (const QUrl& url : files) {
        QFile f(FilePath(url));
        if (!f.open(QIODevice::ReadOnly)) {
            problems << tr("Cannot read %1: %2").arg(url.toDisplayString(), f.errorString());
            continue;
        }
        const QString name = QFileInfo(url.path()).fileName();
        auto id = al::ImportFile(db_, name.toStdString(), f.readAll().toStdString());
        if (id) {
            ++imported;
        } else {
            problems << name + ": " + Tr(id.error().message);
        }
    }
    ReloadProfiles();
    emit libraryChanged();
    recompute_timer_.start();
    QString summary = tr("%n file(s) imported.", nullptr, imported);
    if (!problems.isEmpty()) {
        summary += "\n" + problems.join("\n");
    }
    return summary;
}

QString Backend::importProfile(const QUrl& file) {
    QFile f(FilePath(file));
    if (!f.open(QIODevice::ReadOnly)) {
        return tr("Cannot read %1: %2").arg(file.toDisplayString(), f.errorString());
    }
    return ImportJson(f.readAll().toStdString());
}

QString Backend::copyProfileToClipboard(int id) {
    auto json = al::ExportProfileJson(db_, id);
    if (!json) {
        return Q(json.error().message);
    }
    QGuiApplication::clipboard()->setText(Q(json.value()));
    return {};
}

QString Backend::importProfileFromClipboard() {
    return ImportJson(QGuiApplication::clipboard()->text().toStdString());
}

double Backend::stationPressure(double qnh_hpa, double altitude_m) const {
    return ballistics::StationPressureFromSeaLevel(qnh_hpa * 100.0, altitude_m) / 100.0;
}

al::SessionConditions Backend::Session() const {
    al::SessionConditions s;
    s.temperature_c = temperature_c_;
    s.pressure_hpa = pressure_hpa_;
    s.altitude_m = altitude_m_;
    s.humidity_pct = humidity_pct_;
    if (!powder_follows_air_) {
        s.powder_c = powder_c_;
    }
    if (wind_speed_ > 0.0) {
        s.winds.push_back({wind_speed_, wind_from_deg_, 0.0});
    }
    s.look_angle_deg = look_angle_deg_;
    s.cant_deg = cant_deg_;
    if (coriolis_) {
        s.latitude_deg = latitude_deg_;
        if (use_azimuth_) {
            s.azimuth_deg = azimuth_deg_;
        }
    }
    s.target_range_m = target_range_m_;
    s.magnification = magnification_;
    return s;
}

void Backend::ApplySession(const al::SessionConditions& s) {
    temperature_c_ = s.temperature_c;
    pressure_hpa_ = s.pressure_hpa;
    altitude_m_ = s.altitude_m;
    humidity_pct_ = s.humidity_pct;
    powder_follows_air_ = !s.powder_c.has_value();
    powder_c_ = s.powder_c.value_or(s.temperature_c);
    if (!s.winds.empty()) {
        wind_speed_ = s.winds.front().speed_mps;
        wind_from_deg_ = s.winds.front().from_deg;
    }
    look_angle_deg_ = s.look_angle_deg;
    cant_deg_ = s.cant_deg;
    coriolis_ = s.latitude_deg.has_value();
    latitude_deg_ = s.latitude_deg.value_or(latitude_deg_);
    use_azimuth_ = s.azimuth_deg.has_value();
    azimuth_deg_ = s.azimuth_deg.value_or(azimuth_deg_);
    target_range_m_ = s.target_range_m;
    magnification_ = s.magnification;
    emit conditionsChanged();
}

void Backend::AddReticle(const bs::LoadedProfile& p, const al::SolutionSummary& r,
                         QVariantMap& out) {
    out["hasReticle"] = false;
    if (!r.ok || !p.scope) {
        return;
    }
    const bs::ScopeRecord& scope = *p.scope;
    const double unit_rad = angle_unit_ == "moa" ? ballistics::units::MoaToRad(1.0)
                                                  : ballistics::units::MradToRad(1.0);
    const double magnification = magnification_ > 0.0 ? magnification_ : scope.max_magnification;
    const al::ReticleHold hold =
        al::ComputeReticleHold(r.elevation * unit_rad, r.windage * unit_rad, scope, magnification,
                               al::HoldModeFromString(hold_mode_.toStdString()));
    out["holdMode"] = hold_mode_;
    out["dialElevationClicks"] = hold.dial_elevation_clicks;
    out["dialWindageClicks"] = hold.dial_windage_clicks;
    out["targetX"] = hold.target_x;
    out["targetY"] = hold.target_y;
    out["subtensionScale"] = hold.scale;
    out["focalPlane"] = Q(scope.focal_plane);
    out["minMagnification"] = scope.min_magnification;
    out["maxMagnification"] = scope.max_magnification;
    out["magnification"] = magnification;
    if (scope.reticle_id) {
        auto ret = bs::Repository<bs::ReticleRecord>(db_).Get(*scope.reticle_id);
        if (ret && ret.value()) {
            out["hasReticle"] = true;
            out["reticleName"] = Q(ret.value()->name);
            out["reticleUnits"] = Q(ret.value()->units);
            out["reticleDefinition"] = Q(ret.value()->definition);
        }
    }
}

void Backend::Recompute() {
    QVariantMap out;
    if (current_profile_id_ == 0) {
        out["ok"] = false;
        out["error"] = tr("Create a profile to get a solution.");
    } else if (auto p = bs::LoadProfile(db_, current_profile_id_); !p) {
        out["ok"] = false;
        out["error"] = Q(p.error().message);
    } else {
        const auto unit = angle_unit_ == "moa" ? al::AngleUnit::kMoa : al::AngleUnit::kMrad;
        const al::SolutionSummary r = al::Summarize(p.value(), Session(), unit);
        out = {{"ok", r.ok},
               {"error", Tr(r.error)},
               {"rangeM", r.range_m},
               {"elevation", r.elevation},
               {"windage", r.windage},
               {"elevationClicks", r.elevation_clicks},
               {"windageClicks", r.windage_clicks},
               {"hasScope", p.value().scope.has_value()},
               {"dropCm", r.drop_cm},
               {"windageCm", r.windage_cm},
               {"velocity", r.velocity_mps},
               {"energy", r.energy_j},
               {"time", r.time_s},
               {"mach", r.mach},
               {"muzzleVelocity", r.muzzle_velocity_mps},
               {"stability", r.stability},
               {"velocityScale", p.value().profile.velocity_scale},
               {"dragScale", p.value().profile.drag_scale},
               {"spinDriftCm", r.spin_drift_cm},
               {"subsonic", r.subsonic},
               {"transonicRangeM", r.transonic_range_m}};
        AddReticle(p.value(), r, out);
    }
    solution_ = out;
    emit solutionChanged();
}
