#include <QDir>
#include <QFile>
#include <QObject>
#include <QQuickStyle>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuickTest>

Q_IMPORT_QML_PLUGIN(BalCalcPlugin)

// Points the app's Backend at a fresh scratch database before any QML runs.
class Setup : public QObject {
    Q_OBJECT

public slots:
    void applicationAvailable() {
        const QString db = QDir::temp().filePath("balcalc_ui_tests.db");
        QFile::remove(db);
        qputenv("BALCALC_DB", db.toUtf8());
        QQuickStyle::setStyle("Material");
    }
};

QUICK_TEST_MAIN_WITH_SETUP(ui, Setup)

#include "tst_ui.moc"
