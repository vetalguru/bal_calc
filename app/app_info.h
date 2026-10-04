#ifndef BALCALC_APP_INFO_H
#define BALCALC_APP_INFO_H

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <ballistics/storage/database.h>

// Build and storage facts shown on the About screen. Opens the app
// database on construction so a broken storage path surfaces at startup.
class AppInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString engineVersion READ engineVersion CONSTANT)
    Q_PROPERTY(QString sqliteVersion READ sqliteVersion CONSTANT)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString databaseStatus READ databaseStatus CONSTANT)

public:
    explicit AppInfo(QObject* parent = nullptr);

    QString engineVersion() const;
    QString sqliteVersion() const;
    QString databasePath() const { return db_path_; }
    QString databaseStatus() const { return db_status_; }

private:
    ballistics::storage::Database db_;
    QString db_path_;
    QString db_status_;
};

#endif // BALCALC_APP_INFO_H
