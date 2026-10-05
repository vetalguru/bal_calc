#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("vetalguru");
    QGuiApplication::setApplicationName("BalCalc");
    QQuickStyle::setStyle("Material");

    // Testing aids: render a page and save it as an image, then quit.
    // Pair with QT_QPA_PLATFORM=offscreen for headless runs, and
    // BALCALC_DB=<file> to use a scratch database.
    QCommandLineParser args;
    args.addHelpOption();
    const QCommandLineOption screenshot("screenshot", "Save the window to FILE and quit.", "FILE");
    const QCommandLineOption page("page", "Page to show: 0 solution, 1 conditions, 2 profiles, 3 settings.",
                                  "N", "0");
    const QCommandLineOption size("size", "Window size WxH.", "WxH");
    args.addOptions({screenshot, page, size});
    args.process(app);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("BalCalc", "Main");

    if (args.isSet(screenshot) && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        if (window == nullptr) {
            return 1;
        }
        window->setProperty("page", args.value(page).toInt());
        const QStringList wh = args.value(size).split('x');
        if (wh.size() == 2) {
            window->resize(wh[0].toInt(), wh[1].toInt());
        }
        const QString file = args.value(screenshot);
        QTimer::singleShot(2000, window, [window, file] {
            const bool ok = window->grabWindow().save(file);
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }
    return QGuiApplication::exec();
}
