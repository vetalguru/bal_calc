#include "app_info.h"

#include <QDir>
#include <QStandardPaths>

#include <ballistics/version.h>

AppInfo::AppInfo(QObject* parent) : QObject(parent) {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    db_path_ = QDir(dir).filePath("balcalc.db");

    if (auto s = db_.Open(db_path_.toStdString()); !s) {
        db_status_ = tr("Error: %1").arg(QString::fromStdString(s.error().message));
        return;
    }
    auto version = db_.SchemaVersion();
    db_status_ = version ? tr("OK, schema v%1").arg(version.value())
                         : tr("Error: %1").arg(QString::fromStdString(version.error().message));
}

QString AppInfo::engineVersion() const {
    return QString::fromLatin1(ballistics::version());
}

QString AppInfo::sqliteVersion() const {
    return QString::fromLatin1(ballistics::storage::SqliteVersion());
}
