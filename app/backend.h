#ifndef BALCALC_BACKEND_H
#define BALCALC_BACKEND_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <ballistics/applogic/session.h>
#include <ballistics/applogic/truing.h>
#include <ballistics/storage/database.h>

// The app's single QML-facing object: the database, the profile list, the
// current conditions and the firing solution for them. All logic lives in
// ballistics::applogic; this class adapts it to QML properties.
class Backend : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString engineVersion READ engineVersion CONSTANT)
    Q_PROPERTY(QString sqliteVersion READ sqliteVersion CONSTANT)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString databaseError READ databaseError CONSTANT)

    // Rifles [{id, name, caliber}] and the user's cartridges [{id, name,
    // caliber, bulletName, muzzleVelocity, matches}] (those matching the
    // current rifle's calibre first, `matches` true).
    Q_PROPERTY(QVariantList rifles READ rifles NOTIFY armoryChanged)
    Q_PROPERTY(QVariantList cartridges READ cartridges NOTIFY armoryChanged)
    // The solution is for this rifle + cartridge (both persisted).
    Q_PROPERTY(int currentRifleId READ currentRifleId WRITE setCurrentRifleId NOTIFY selectionChanged)
    Q_PROPERTY(int currentCartridgeId READ currentCartridgeId WRITE setCurrentCartridgeId NOTIFY
                   selectionChanged)
    // Their pair (shot log, truing, point-of-impact shift); 0 until both
    // are chosen.
    Q_PROPERTY(int currentProfileId READ currentProfileId NOTIFY currentProfileIdChanged)
    // {rifleName, cartridgeName, zeroRangeM, offsetUpCm, offsetRightCm} of
    // the current pair (empty without one).
    Q_PROPERTY(QVariantMap currentPair READ currentPair NOTIFY currentProfileIdChanged)
    Q_PROPERTY(QString angleUnit READ angleUnit WRITE setAngleUnit NOTIFY angleUnitChanged)
    // "" = follow the system, otherwise "uk", "ru" or "en".
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

    // Current conditions (UI units). Any change recomputes the solution.
    Q_PROPERTY(double temperatureC MEMBER temperature_c_ NOTIFY conditionsChanged)
    Q_PROPERTY(double pressureHpa MEMBER pressure_hpa_ NOTIFY conditionsChanged)
    Q_PROPERTY(double altitudeM MEMBER altitude_m_ NOTIFY conditionsChanged)
    Q_PROPERTY(double humidityPct MEMBER humidity_pct_ NOTIFY conditionsChanged)
    Q_PROPERTY(bool powderFollowsAir MEMBER powder_follows_air_ NOTIFY conditionsChanged)
    Q_PROPERTY(double powderC MEMBER powder_c_ NOTIFY conditionsChanged)
    Q_PROPERTY(double windSpeed MEMBER wind_speed_ NOTIFY conditionsChanged)
    Q_PROPERTY(double windFromDeg MEMBER wind_from_deg_ NOTIFY conditionsChanged)
    Q_PROPERTY(double lookAngleDeg MEMBER look_angle_deg_ NOTIFY conditionsChanged)
    Q_PROPERTY(double cantDeg MEMBER cant_deg_ NOTIFY conditionsChanged)
    Q_PROPERTY(bool coriolis MEMBER coriolis_ NOTIFY conditionsChanged)
    Q_PROPERTY(double latitudeDeg MEMBER latitude_deg_ NOTIFY conditionsChanged)
    Q_PROPERTY(bool useAzimuth MEMBER use_azimuth_ NOTIFY conditionsChanged)
    Q_PROPERTY(double azimuthDeg MEMBER azimuth_deg_ NOTIFY conditionsChanged)
    Q_PROPERTY(double targetRangeM MEMBER target_range_m_ NOTIFY conditionsChanged)
    Q_PROPERTY(double magnification MEMBER magnification_ NOTIFY conditionsChanged)
    // "dial_elevation" | "hold" | "dial" (persisted).
    Q_PROPERTY(QString holdMode READ holdMode WRITE setHoldMode NOTIFY holdModeChanged)

    // Range card span (persisted).
    Q_PROPERTY(double tableFromM MEMBER table_from_m_ NOTIFY tableSpecChanged)
    Q_PROPERTY(double tableToM MEMBER table_to_m_ NOTIFY tableSpecChanged)
    Q_PROPERTY(double tableStepM MEMBER table_step_m_ NOTIFY tableSpecChanged)

    // Result for the current profile and conditions; keys as in
    // applogic::SolutionSummary (camelCase) plus "ok" and "error".
    Q_PROPERTY(QVariantMap solution READ solution NOTIFY solutionChanged)

public:
    explicit Backend(QObject* parent = nullptr);

    QString engineVersion() const;
    QString sqliteVersion() const;
    QString databasePath() const { return db_path_; }
    QString databaseError() const { return db_error_; }

    QVariantList rifles() const { return rifles_; }
    QVariantList cartridges() const { return cartridges_; }
    int currentRifleId() const { return current_rifle_id_; }
    void setCurrentRifleId(int id);
    int currentCartridgeId() const { return current_cartridge_id_; }
    void setCurrentCartridgeId(int id);
    int currentProfileId() const { return current_profile_id_; }
    QVariantMap currentPair();
    QString angleUnit() const { return angle_unit_; }
    void setAngleUnit(const QString& unit);
    QString holdMode() const { return hold_mode_; }
    void setHoldMode(const QString& mode);
    QString language() const { return language_; }
    void setLanguage(const QString& language);
    QVariantMap solution() const { return solution_; }

    // Forms as maps (keys: camelCase RifleForm / CartridgeForm fields); id 0
    // gives the defaults for a new one. Save returns an error message, or ""
    // on success (the saved record becomes current); delete removes the
    // record's pairs and shot logs too.
    Q_INVOKABLE QVariantMap rifleForm(int id);
    Q_INVOKABLE QString saveRifle(const QVariantMap& form);
    Q_INVOKABLE QString deleteRifle(int id);
    Q_INVOKABLE QVariantMap cartridgeForm(int id);
    Q_INVOKABLE QString saveCartridge(const QVariantMap& form);
    Q_INVOKABLE QString deleteCartridge(int id);
    // The cartridge form with a library bullet chosen for it.
    Q_INVOKABLE QVariantMap cartridgeFormWithBullet(const QVariantMap& form, int bullet_id);
    // Factory loads (starter library, imported .ammo) matching `filter`,
    // as in `cartridges`; a new cartridge form copied from one of them.
    Q_INVOKABLE QVariantList libraryCartridges(const QString& filter);
    Q_INVOKABLE QVariantMap cartridgeFormFromLibrary(int id);
    // Point-of-impact shift of the current cartridge from the rifle's zero.
    Q_INVOKABLE QString setZeroOffset(double up_cm, double right_cm);
    // Adds a ready-to-try sample rifle and cartridge and makes them current.
    Q_INVOKABLE QString addSampleProfile();

    // Range card for the current profile and conditions over the table
    // span: {ok, error, hasScope, rows: [{rangeM, elevation, windage,
    // elevationClicks, windageClicks, dropCm, windageCm, velocity, mach,
    // energy, time}]}.
    Q_INVOKABLE QVariantMap rangeTable();
    // Trajectory samples for the chart: {ok, error, rows: [...]} as above,
    // `points` samples from the muzzle to `max_range_m`.
    Q_INVOKABLE QVariantMap trajectoryCurve(double max_range_m, int points);

    // Shot log of the current profile: [{id, rangeM, observed, predicted,
    // observedWindage, hasWindage, shotAt, used, notes, temperatureC}],
    // angles in the current angle unit.
    Q_INVOKABLE QVariantList shots();
    // Logs a hit at the current conditions; angles in the current unit.
    Q_INVOKABLE QString logShot(double range_m, double elevation, bool has_windage, double windage,
                                const QString& notes);
    Q_INVOKABLE QString deleteShot(int id);
    Q_INVOKABLE QString setShotUsed(int id, bool used);
    // Fits the current profile: {ok, error, velocityScale, dragScale,
    // dragFitted, rmsBefore, rmsAfter, velocityBefore, velocityAfter,
    // points: [{rangeM, observed, before, after}]} (angles in the unit).
    Q_INVOKABLE QVariantMap computeTruing();
    // Applies the last computeTruing() result / resets the scales to 1.
    Q_INVOKABLE QString applyTruing();
    Q_INVOKABLE QString resetTruing();

    // Reticles in the library: [{id, name, units}].
    Q_INVOKABLE QVariantList reticles();

    // Bullet library: summaries matching `filter`, one bullet as a form
    // (keys: camelCase BulletForm fields, bands as [{velocity, bc}]), save
    // and delete returning an error message or "".
    Q_INVOKABLE QVariantList libraryBullets(const QString& filter);
    Q_INVOKABLE QVariantMap bulletForm(int id);
    Q_INVOKABLE QString saveBullet(const QVariantMap& form);
    Q_INVOKABLE QString deleteBullet(int id);

    // Rifle and cartridge files (JSON, `kind` "rifle" or "cartridge"):
    // error message or "" on success. Import (also of old profile files)
    // makes what it brought in current.
    Q_INVOKABLE QString exportItem(const QString& kind, int id, const QUrl& file);
    Q_INVOKABLE QString copyItemToClipboard(const QString& kind, int id);
    Q_INVOKABLE QString importShared(const QUrl& file);
    Q_INVOKABLE QString importSharedFromClipboard();

    // Imports data files (.ammo, .drg, .reticle, profile or bullet-list
    // .json); returns a summary such as "3 imported" plus any problems.
    Q_INVOKABLE QString importFiles(const QList<QUrl>& files);

    // Starter library import result, for the About screen.
    Q_PROPERTY(QString seedReport READ seedReport CONSTANT)
    QString seedReport() const { return seed_report_; }
    // Suggested file name for an export.
    Q_INVOKABLE QString exportFileName(const QString& kind, int id) const;

    // Station pressure from sea-level pressure (QNH) at an altitude, hPa.
    Q_INVOKABLE double stationPressure(double qnh_hpa, double altitude_m) const;

signals:
    void armoryChanged();
    void selectionChanged();
    void currentProfileIdChanged();
    void angleUnitChanged();
    void languageChanged();
    void holdModeChanged();
    void conditionsChanged();
    void solutionChanged();
    void tableSpecChanged();
    void libraryChanged();
    void shotsChanged();

private:
    void ReloadArmory();
    // Keeps the selection valid and the pair in step with it.
    void UpdatePair();
    void Select(int rifle_id, int cartridge_id);
    QVariantMap Table(double from_m, double to_m, double step_m);
    QString ImportJson(const std::string& json);
    void SeedStarterLibrary();
    void InstallTranslator();
    void Recompute();
    void AddReticle(const ballistics::storage::LoadedProfile& p,
                    const ballistics::applogic::SolutionSummary& r, QVariantMap& out);
    ballistics::applogic::SessionConditions Session() const;
    void ApplySession(const ballistics::applogic::SessionConditions& s);

    ballistics::storage::Database db_;
    QString db_path_;
    QString db_error_;
    QVariantList rifles_;
    QVariantList cartridges_;
    int current_rifle_id_ = 0;
    int current_cartridge_id_ = 0;
    int current_profile_id_ = 0;
    QString angle_unit_ = QStringLiteral("mrad");
    QString language_;
    QTranslator translator_;
    QVariantMap solution_;
    QString seed_report_;
    ballistics::applogic::TruingResult last_truing_;
    QTimer recompute_timer_;
    QTimer save_timer_;

    double temperature_c_ = 15.0;
    double pressure_hpa_ = 1013.25;
    double altitude_m_ = 0.0;
    double humidity_pct_ = 50.0;
    bool powder_follows_air_ = true;
    double powder_c_ = 15.0;
    double wind_speed_ = 0.0;
    double wind_from_deg_ = 90.0;
    double look_angle_deg_ = 0.0;
    double cant_deg_ = 0.0;
    bool coriolis_ = false;
    double latitude_deg_ = 50.0;
    bool use_azimuth_ = false;
    double azimuth_deg_ = 0.0;
    double target_range_m_ = 300.0;
    double magnification_ = 0.0;
    QString hold_mode_ = QStringLiteral("dial_elevation");
    double table_from_m_ = 100.0;
    double table_to_m_ = 1000.0;
    double table_step_m_ = 50.0;
};

#endif // BALCALC_BACKEND_H
