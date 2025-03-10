// SPDX-License-Identifier: BSD-3-Clause
// SPDX-FileCopyrightText: 2020-2022 The Monero Project

#include <QMessageBox>
#include <QtNetwork>
#include <QClipboard>
#include <QDesktopServices>
#include <QPushButton>

#include "constants.h"
#include "networktype.h"
#include "Utils.h"
#include "utils/ColorScheme.h"
#include "utils/config.h"
#include "utils/os/tails.h"
#include "utils/os/whonix.h"

namespace Utils {
bool fileExists(const QString &path) {
    QFileInfo check_file(path);
    return check_file.exists() && check_file.isFile();
}

QByteArray fileOpen(const QString &path) {
    QFile file(path);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return QByteArray();
    }

    QByteArray data = file.readAll();
    file.close();
    return data;
}

QByteArray fileOpenQRC(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "error: " << file.errorString();
        return QByteArray();
    }

    QByteArray data = file.readAll();
    file.close();
    return data;
}

bool fileWrite(const QString &path, const QString &data) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream out(&file); out << data << Qt::endl;
        file.close();
        return true;
    }
    return false;
}

bool pixmapWrite(const QString &path, const QPixmap &pixmap) {
    qDebug() << "Writing xdg icon: " << path;
    QFile file(path);
    QFileInfo iconInfo(file);
    QDir().mkpath(iconInfo.path());
    if(file.open(QIODevice::WriteOnly)){
        pixmap.save(&file, "PNG");
        file.close();
        return true;
    }
    return false;
}

QStringList fileFind(const QRegularExpression &pattern, const QString &baseDir, int level, int depth, const int maxPerDir) {
    // like `find /foo -name -maxdepth 2 "*.jpg"`
    QStringList rtn;
    QDir dir(baseDir);
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::NoDot | QDir::NoDotDot);

    int fileCount = 0;
    for (const auto &fileInfo: dir.entryInfoList({"*"})) {
        fileCount += 1;
        if(fileCount > maxPerDir) return rtn;
        if(!fileInfo.isReadable())
            continue;

        const auto fn = fileInfo.fileName();
        const auto path = fileInfo.filePath();

        QRegularExpression re(QRegularExpression::anchoredPattern(pattern.pattern()));
        QRegularExpressionMatch match = re.match(fn);

        if (fileInfo.isDir()) {
            if (level + 1 <= depth)
                rtn << fileFind(pattern, path, level + 1, depth, maxPerDir);
        }
        else if (match.hasMatch()) {
            rtn << path;
        }
    }
    return rtn;
}

bool dirExists(const QString &path) {
    QDir pathDir(path);
    return pathDir.exists();
}

QString defaultWalletDir() {
    QString portablePath = QCoreApplication::applicationDirPath().append("/%1");
    if (QFile::exists(portablePath.arg(".portable")) || QFile::exists(portablePath.arg(".portable.txt"))) {
        return portablePath.arg("feather_data/wallets");
    }

    if (TailsOS::detect()) {
        QString path = []{
            // Starting in 1.1.0 the wallet and config directory were moved from ./.feather to ./feather_data
            // A user might accidentally delete the folder containing the file hidden folder after moving the AppImage
            // We return the old path if it still exists

            QString appImagePath = qgetenv("APPIMAGE");
            if (appImagePath.isEmpty()) {
                qDebug() << "Not an appimage, using currentPath()";
                if (QDir(QDir::currentPath() + "/.feather").exists()) {
                    return QDir::currentPath() + "/.feather/Monero/wallets";
                }
                return QDir::currentPath() + "/feather_data/wallets";
            }

            QFileInfo appImageDir(appImagePath);
            QString absolutePath = appImageDir.absoluteDir().path();
            if (QDir(absolutePath + "/.feather").exists()) {
                return absolutePath + "/.feather/Monero/wallets";
            }
            return absolutePath + "/feather_data/wallets";
        }();

        return path;
    }

    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wallets";
}

QString applicationPath() {
    QString applicationPath = qgetenv("APPIMAGE");
    if (!applicationPath.isEmpty()) {
        applicationPath = QFileInfo(applicationPath).absoluteDir().path();
    } else {
        applicationPath = QCoreApplication::applicationDirPath();
    }
    return applicationPath;
}

bool validateJSON(const QByteArray &blob) {
    QJsonDocument doc = QJsonDocument::fromJson(blob);
    QString jsonString = doc.toJson(QJsonDocument::Indented);
    return !jsonString.isEmpty();
}

bool readJsonFile(QIODevice &device, QSettings::SettingsMap &map) {
    QJsonDocument json = QJsonDocument::fromJson(device.readAll());
    map = json.object().toVariantMap();
    return true;
}

bool writeJsonFile(QIODevice &device, const QSettings::SettingsMap &map) {
    device.write(QJsonDocument(QJsonObject::fromVariantMap(map)).toJson(QJsonDocument::Indented));
    return true;
}

void copyToClipboard(const QString &string){
    QClipboard * clipboard = QApplication::clipboard();
    if (!clipboard) {
        qWarning() << "Unable to access clipboard";
        return;
    }
    clipboard->setText(string, QClipboard::Clipboard);
    if (clipboard->supportsSelection())
        clipboard->setText(string, QClipboard::Selection);

#if defined(Q_OS_LINUX)
    QThread::msleep(1);
#endif
}

QString copyFromClipboard() {
    QClipboard * clipboard = QApplication::clipboard();
    if (!clipboard) {
        qWarning() << "Unable to access clipboard";
        return "";
    }
    return clipboard->text();
}

QString xdgDesktopEntry(){
    return QString(
            "[Desktop Entry]\n"
            "Name=Feather\n"
            "GenericName=Feather\n"
            "X-GNOME-FullName=Feather\n"
            "Comment=a free Monero desktop wallet\n"
            "Keywords=Monero;\n"
            "Exec=\"%1\" %u\n"
            "Terminal=false\n"
            "Type=Application\n"
            "Icon=monero\n"
            "Categories=Network;GNOME;Qt;\n"
            "StartupNotify=true\n"
    ).arg(QApplication::applicationFilePath());
}

bool xdgDesktopEntryWrite(const QString &path){
    QString mime = xdgDesktopEntry();
    QFileInfo file(path);
    QDir().mkpath(file.path());
    qDebug() << "Writing xdg desktop entry: " << path;
    return fileWrite(path, mime);
}

void xdgRefreshApplications(){
    QStringList args = {QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation)};
    QProcess::startDetached("update-desktop-database", args);
}

bool xdgDesktopEntryRegister() {
#if defined(Q_OS_MACOS)
    return false;
#endif
#if defined(Q_OS_WIN)
    // @TODO: implement
    return false;
#endif

    QPixmap appIcon(":assets/images/feather.png");
    QString pathIcon = QString("%1/.local/share/icons/feather.png").arg(QDir::homePath());
    if (!fileExists(pathIcon)) {
        pixmapWrite(pathIcon, appIcon);
    }
    xdgDesktopEntryWrite(QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation));

    xdgRefreshApplications();
    return true;
}

bool portOpen(const QString &hostname, quint16 port) { // TODO: this call should be async
    if (config()->get(Config::offlineMode).toBool()) {
        return false;
    }

    QTcpSocket socket;
    socket.connectToHost(hostname, port);
    return socket.waitForConnected(600);
}

quint16 getDefaultRpcPort(NetworkType::Type type) {
    switch (type) {
        case NetworkType::Type::MAINNET:
            return 18081;
        case NetworkType::Type::TESTNET:
            return 28081;
        case NetworkType::Type::STAGENET:
            return 38081;
    }
    return 18081;
}

bool isTorsocks() {
#if defined(Q_OS_MAC)
    return qgetenv("DYLD_INSERT_LIBRARIES").indexOf("libtorsocks") >= 0;
#elif defined(Q_OS_LINUX)
    return qgetenv("LD_PRELOAD").indexOf("libtorsocks") >= 0;
#else
    return false;
#endif
}

double roundSignificant(double N, double n)
{
    int h;
    double b, d, e, i, j, m, f;
    b = N;

    for (i = 0; b >= 1; ++i)
        b = b / 10;

    d = n - i;
    b = N;
    b = b * pow(10, d);
    e = b + 0.5;
    if ((float)e == (float)ceil(b)) {
        f = (ceil(b));
        h = f - 2;
        if (h % 2 != 0) {
            e = e - 1;
        }
    }
    j = floor(e);
    m = pow(10, d);
    j = j / m;
    return j;
}

int maxLength(const QVector<QString> &array) {
    int maxLength = 0;
    for (const auto &str: array) {
        auto length = str.length();
        if (length > maxLength) {
            maxLength = length;
        }
    }
    return maxLength;
}

QString formatBytes(quint64 bytes)
{
    QVector<QString> sizes = { "B", "KB", "MB", "GB", "TB" };

    int i;
    double _data;
    for (i = 0; i < sizes.count() && bytes >= 10000; i++, bytes /= 1000)
        _data = bytes / 1000.0;

    if (_data < 0)
        _data = 0;

    // unrealistic
    if (_data > 10000)
        _data = 0;

    return QString("%1 %2").arg(QString::number(_data, 'f', 1), sizes[i]);
}

QMap<QString, QLocale> localeCache = {};
QLocale getCurrencyLocale(const QString &currencyCode) {
    QLocale locale;
    if (localeCache.contains(currencyCode)) {
        locale = localeCache[currencyCode];
    } else {
        QList<QLocale> allLocales = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyCountry);
        for (const auto& locale_: allLocales) {
            if (locale_.currencySymbol(QLocale::CurrencyIsoCode) == currencyCode) {
                locale = locale_;
            }
        }
        localeCache[currencyCode] = locale;
    }
    return locale;
}

QString amountToCurrencyString(double amount, const QString &currencyCode) {
    QLocale locale = getCurrencyLocale(currencyCode);

    // \xC2\xA0 = UTF-8 non-breaking space, it looks off.
    if (currencyCode == "USD")
        return locale.toCurrencyString(amount, "$").remove("\xC2\xA0");

    return locale.toCurrencyString(amount).remove("\xC2\xA0");
}

QStandardItem *qStandardItem(const QString& text) {
    auto font = QApplication::font();
    return Utils::qStandardItem(text, font);
}

QStandardItem *qStandardItem(const QString& text, QFont &font) {
    // stupid Qt doesnt set font sizes correctly on OSX
    // @TODO: memleak
    auto item = new QStandardItem(text);
    item->setFont(font);
    return item;
}

QString blockExplorerLink(const QString &blockExplorer, NetworkType::Type nettype, const QString &txid) {
    if (blockExplorer == "exploremonero.com") {
        if (nettype == NetworkType::MAINNET) {
            return QString("https://exploremonero.com/transaction/%1").arg(txid);
        }
    }
    else if (blockExplorer == "moneroblocks.info") {
        if (nettype == NetworkType::MAINNET) {
            return QString("https://moneroblocks.info/tx/%1").arg(txid);
        }
    }
    else if (blockExplorer == "blockchair.com") {
        if (nettype == NetworkType::MAINNET) {
            return QString("https://blockchair.com/monero/transaction/%1").arg(txid);
        }
    }
    else if (blockExplorer == "melo.tools") {
        switch (nettype) {
            case NetworkType::MAINNET:
                return QString("https://melo.tools/explorer/mainnet/tx/%1").arg(txid);
            case NetworkType::STAGENET:
                return QString("https://melo.tools/explorer/stagenet/tx/%1").arg(txid);
            case NetworkType::TESTNET:
                return QString("https://melo.tools/explorer/testnet/tx/%1").arg(txid);
        }
    }
    else if (blockExplorer == "blkchairbknpn73cfjhevhla7rkp4ed5gg2knctvv7it4lioy22defid.onion") {
        if (nettype == NetworkType::MAINNET) {
            return QString("http://blkchairbknpn73cfjhevhla7rkp4ed5gg2knctvv7it4lioy22defid.onion/monero/transaction/%1").arg(txid);
        }
    }

    switch (nettype) {
        case NetworkType::MAINNET:
            return QString("https://xmrchain.net/tx/%1").arg(txid);
        case NetworkType::STAGENET:
            return QString("https://stagenet.xmrchain.net/tx/%1").arg(txid);
        case NetworkType::TESTNET:
            return QString("https://testnet.xmrchain.net/tx/%1").arg(txid);
    }

    return QString("");
}

void externalLinkWarning(QWidget *parent, const QString &url){
    if (!config()->get(Config::warnOnExternalLink).toBool()) {
        QDesktopServices::openUrl(QUrl(url));
        return;
    }

    QString body = QString("You are about to open the following link:\n\n%1").arg(url);

    if (!(TailsOS::detect() || WhonixOS::detect()))
        body += "\n\nYou will NOT be using Tor.";

    QMessageBox linkWarning(parent);
    linkWarning.setWindowTitle("External link warning");
    linkWarning.setText(body);
    QPushButton *copy = linkWarning.addButton("Copy link", QMessageBox::HelpRole);
    linkWarning.addButton(QMessageBox::Cancel);
    linkWarning.addButton(QMessageBox::Ok);
    linkWarning.setDefaultButton(QMessageBox::Ok);

    linkWarning.exec();

    if (linkWarning.clickedButton() == copy) {
        Utils::copyToClipboard(url);
    } else if (linkWarning.result() == QMessageBox::Ok) {
        QDesktopServices::openUrl(QUrl(url));
    }
}

void desktopNotify(const QString &title, const QString &message, int duration) {
    QStringList notify_send = QStringList() << title << message << "-t" << QString::number(duration);
    QStringList kdialog = QStringList() << title << message;
    QStringList macos = QStringList() << "-e" << QString(R"(display notification "%1" with title "%2")").arg(message).arg(title);
#if defined(Q_OS_LINUX)
    QProcess process;
    if (fileExists("/usr/bin/kdialog"))
        process.start("/usr/bin/kdialog", kdialog);
    else if (fileExists("/usr/bin/notify-send"))
        process.start("/usr/bin/notify-send", notify_send);
    process.waitForFinished(-1);
    QString stdout = process.readAllStandardOutput();
    QString stderr = process.readAllStandardError();
#elif defined(Q_OS_MACOS)
    QProcess process;
    // @TODO: need to escape special chars with "\"
    process.start("osascript", macos);
    process.waitForFinished(-1);
    QString stdout = process.readAllStandardOutput();
    QString stderr = process.readAllStandardError();
#endif
}

QTextCharFormat addressTextFormat(const SubaddressIndex &index, quint64 amount) {
    QTextCharFormat rec;
    if (index.isPrimary()) {
        rec.setBackground(QBrush(ColorScheme::YELLOW.asColor(true)));
        rec.setToolTip("Wallet change/primary address");
    }
    else if (index.isValid()) {
        rec.setBackground(QBrush(ColorScheme::GREEN.asColor(true)));
        rec.setToolTip("Wallet receive address");
    }
    else if (amount == 0) {
        rec.setBackground(QBrush(ColorScheme::GRAY.asColor(true)));
        rec.setToolTip("Dummy output (Min. 2 outs consensus rule)");
    }
    return rec;
}

void applicationLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    const QString fn = context.function ? QString::fromUtf8(context.function) : "";
    const QString date = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString line;

    switch (type) {
        case QtDebugMsg:
            line = QString("[%1 D] %2(:%3) %4\n").arg(date).arg(fn).arg(context.line).arg(msg);
            fprintf(stderr, "%s", line.toLatin1().data());
            break;
        case QtInfoMsg:
            line = QString("[%1 I] %2\n").arg(date).arg(msg);
            fprintf(stdout, "%s", line.toLatin1().data());
            break;
        case QtWarningMsg:
            line = QString("[%1 W] %2(:%3) %4\n").arg(date).arg(fn).arg(context.line).arg(msg);
            fprintf(stdout, "%s", line.toLatin1().data());
            break;
        case QtCriticalMsg:
            line = QString("[%1 C] %2(:%3) %4\n").arg(date).arg(fn).arg(context.line).arg(msg);
            fprintf(stderr, "%s", line.toLatin1().data());
            break;
        case QtFatalMsg:
            line = QString("[%1 F] %2(:%3) %4\n").arg(date).arg(fn).arg(context.line).arg(msg);
            fprintf(stderr, "%s", line.toLatin1().data());
            break;
    }
}

QString barrayToString(const QByteArray &data) {
    return QString(QTextCodec::codecForMib(106)->toUnicode(data));
}

QString getAccountName() {
    QString accountName = qgetenv("USER"); // mac/linux
    if (accountName.isEmpty())
        accountName = qgetenv("USERNAME"); // Windows
    if (accountName.isEmpty())
        throw std::runtime_error("Could derive system account name from env vars: USER or USERNAME");
    return accountName;
}

QFont relativeFont(int delta) {
    auto font = QApplication::font();
    font.setPointSize(font.pointSize() + delta);
    return font;
}
}