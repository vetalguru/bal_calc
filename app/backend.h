#ifndef BALCALC_BACKEND_H
#define BALCALC_BACKEND_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QTranslator>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <ballistics/applogic/session.h>
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

    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(int currentProfileId READ currentProfileId WRITE setCurrentProfileId NOTIFY
                   currentProfileIdChanged)
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

    QVariantList profiles() const { return profiles_; }
    int currentProfileId() const { return current_profile_id_; }
    void setCurrentProfileId(int id);
    QString angleUnit() const { return angle_unit_; }
    void setAngleUnit(const QString& unit);
    QString language() const { return language_; }
    void setLanguage(const QString& language);
    QVariantMap solution() const { return solution_; }

    // Profile form as a map (keys: camelCase ProfileForm fields); id 0 gives
    // the defaults for a new profile.
    Q_INVOKABLE QVariantMap profileForm(int id);
    // Saves the form; returns an error message, or "" on success (the saved
    // profile becomes current).
    Q_INVOKABLE QString saveProfile(const QVariantMap& form);
    Q_INVOKABLE QString deleteProfile(int id);

    // Range card for the current profile and conditions over the table
    // span: {ok, error, hasScope, rows: [{rangeM, elevation, windage,
    // elevationClicks, windageClicks, dropCm, windageCm, velocity, mach,
    // energy, time}]}.
    Q_INVOKABLE QVariantMap rangeTable();
    // Trajectory samples for the chart: {ok, error, rows: [...]} as above,
    // `points` samples from the muzzle to `max_range_m`.
    Q_INVOKABLE QVariantMap trajectoryCurve(double max_range_m, int points);

    // Station pressure from sea-level pressure (QNH) at an altitude, hPa.
    Q_INVOKABLE double stationPressure(double qnh_hpa, double altitude_m) const;

signals:
    void profilesChanged();
    void currentProfileIdChanged();
    void angleUnitChanged();
    void languageChanged();
    void conditionsChanged();
    void solutionChanged();
    void tableSpecChanged();

private:
    void ReloadProfiles();
    QVariantMap Table(double from_m, double to_m, double step_m);
    void InstallTranslator();
    void Recompute();
    ballistics::applogic::SessionConditions Session() const;
    void ApplySession(const ballistics::applogic::SessionConditions& s);

    ballistics::storage::Database db_;
    QString db_path_;
    QString db_error_;
    QVariantList profiles_;
    int current_profile_id_ = 0;
    QString angle_unit_ = QStringLiteral("mrad");
    QString language_;
    QTranslator translator_;
    QVariantMap solution_;
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
    double table_from_m_ = 100.0;
    double table_to_m_ = 1000.0;
    double table_step_m_ = 50.0;
};

#endif // BALCALC_BACKEND_H
