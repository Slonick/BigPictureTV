#include "steamwindowmanager.h"
#include <windows.h>
#include <QLocale>
#include "logmanager.h"

QString getRegistryValue(const std::wstring &keyPath, const std::wstring &valueName)
{
    HKEY hKey;
    WCHAR value[256];
    DWORD valueLength = sizeof(value);
    QString result;

    if (RegOpenKeyEx(HKEY_CURRENT_USER, keyPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, valueName.c_str(), NULL, NULL, (LPBYTE)value, &valueLength)
            == ERROR_SUCCESS) {
            result = QString::fromWCharArray(value).toLower();
        }
        RegCloseKey(hKey);
    }

    return result;
}

QString cleanString(const QString &str)
{
    const QChar NON_BREAKING_SPACE = QChar(0x00A0);
    QString cleanedStr = str;
    return cleanedStr.replace(NON_BREAKING_SPACE, ' ');
}

QVector<QString> getAllWindowTitles()
{
    QVector<QString> windowTitles;

    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            QVector<QString> *titles = reinterpret_cast<QVector<QString> *>(lParam);

            if (IsWindowVisible(hwnd) && !(GetWindowLong(hwnd, GWL_STYLE) & WS_MINIMIZE)) {
                WCHAR windowTitle[256];
                if (GetWindowText(hwnd, windowTitle, sizeof(windowTitle) / sizeof(WCHAR)) > 0) {
                    titles->append(QString::fromWCharArray(windowTitle));
                }
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&windowTitles));

    return windowTitles;
}

QString SteamWindowManager::getSteamLanguage()
{
    return getRegistryValue(L"Software\\Valve\\Steam\\steamglobal", L"Language");
}

static QString systemLocaleToSteamLanguage()
{
    QLocale locale = QLocale::system();
    QLocale::Language lang = locale.language();
    QLocale::Country country = locale.country();

    switch (lang) {
    case QLocale::Chinese:
        return (country == QLocale::Taiwan || country == QLocale::HongKong || country == QLocale::Macau)
            ? "tchinese" : "schinese";
    case QLocale::Japanese:    return "japanese";
    case QLocale::Korean:      return "koreana";
    case QLocale::Thai:        return "thai";
    case QLocale::Bulgarian:   return "bulgarian";
    case QLocale::Czech:       return "czech";
    case QLocale::Danish:      return "danish";
    case QLocale::German:      return "german";
    case QLocale::English:     return "english";
    case QLocale::Spanish:
        return (country == QLocale::Mexico || country == QLocale::Argentina ||
                country == QLocale::Colombia || country == QLocale::Chile ||
                country == QLocale::Peru || country == QLocale::Venezuela)
            ? "latam" : "spanish";
    case QLocale::Greek:       return "greek";
    case QLocale::French:      return "french";
    case QLocale::Indonesian:  return "indonesian";
    case QLocale::Italian:     return "italian";
    case QLocale::Hungarian:   return "hungarian";
    case QLocale::Dutch:       return "dutch";
    case QLocale::NorwegianBokmal:
    case QLocale::NorwegianNynorsk:
        return "norwegian";
    case QLocale::Polish:      return "polish";
    case QLocale::Portuguese:
        return (country == QLocale::Brazil) ? "brazilian" : "portuguese";
    case QLocale::Romanian:    return "romanian";
    case QLocale::Russian:     return "russian";
    case QLocale::Finnish:     return "finnish";
    case QLocale::Swedish:     return "swedish";
    case QLocale::Turkish:     return "turkish";
    case QLocale::Vietnamese:  return "vietnamese";
    case QLocale::Ukrainian:   return "ukrainian";
    default:                   return QString();
    }
}

QString SteamWindowManager::getBigPictureWindowTitle()
{
    const QMap<QString, QString> BIG_PICTURE_WINDOW_TITLES
        = {{"schinese", "Steam 大屏幕模式"},
           {"tchinese", "Steam Big Picture 模式"},
           {"japanese", "Steam Big Pictureモード"},
           {"koreana", "Steam Big Picture 모드"},
           {"thai", "โหมด Big Picture บน Steam"},
           {"bulgarian", "Steam режим „Голям екран“"},
           {"czech", "Steam režim Big Picture"},
           {"danish", "Steam Big Picture-tilstand"},
           {"german", "Big-Picture-Modus"},
           {"english", "Steam Big Picture mode"},
           {"spanish", "Modo Big Picture de Steam"},
           {"latam", "Modo Big Picture de Steam"},
           {"greek", "Steam Λειτουργία Big Picture"},
           {"french", "Steam mode Big Picture"},
           {"indonesian", "Mode Big Picture Steam"},
           {"italian", "Modalità Big Picture di Steam"},
           {"hungarian", "Steam Nagy Kép mód"},
           {"dutch", "Steam Big Picture-modus"},
           {"norwegian", "Steam Big Picture-modus"},
           {"polish", "Tryb Big Picture Steam"},
           {"portuguese", "Steam Big Picture"},
           {"brazilian", "Steam Modo Big Picture"},
           {"romanian", "Steam modul Big Picture"},
           {"russian", "Режим Big Picture"},
           {"finnish", "Steamin televisiotila"},
           {"swedish", "Steams Big Picture-läge"},
           {"turkish", "Steam Geniş Ekran Modu"},
           {"vietnamese", "Chế độ Big Picture trên Steam"},
           {"ukrainian", "Steam у режимі Big Picture"}};

    QString language = getSteamLanguage().toLower();
    if (language.isEmpty() || !BIG_PICTURE_WINDOW_TITLES.contains(language)) {
        QString systemLanguage = systemLocaleToSteamLanguage();
        if (!systemLanguage.isEmpty() && BIG_PICTURE_WINDOW_TITLES.contains(systemLanguage)) {
            LogManager::debug("Steam language not found in registry, falling back to system locale: " + systemLanguage);
            language = systemLanguage;
        } else {
            LogManager::debug("Steam language not found and no system locale match, falling back to english");
            language = "english";
        }
    }
    return BIG_PICTURE_WINDOW_TITLES.value(language);
}

bool SteamWindowManager::isBigPictureRunning()
{
    QString bigPictureTitle = cleanString(getBigPictureWindowTitle().toLower());
    QStringList bigPictureWords = bigPictureTitle.split(' ', Qt::SkipEmptyParts);

    QVector<QString> currentWindowTitles = getAllWindowTitles();
    for (const auto &windowTitle : currentWindowTitles) {
        QString cleanedTitle = cleanString(windowTitle.toLower());
        QStringList windowWords = cleanedTitle.split(' ', Qt::SkipEmptyParts);

        if (std::all_of(bigPictureWords.begin(),
                        bigPictureWords.end(),
                        [&windowWords](const QString &word) {
                            return windowWords.contains(word);
                        })) {
            LogManager::debug("Big Picture window found");
            return true;
        }
    }
    LogManager::debug("Big Picture window not found");
    return false;
}

bool SteamWindowManager::isCustomWindowRunning(const QString &windowTitle)
{
    QString cleanedWindowTitle = cleanString(windowTitle.toLower());
    QStringList customWindowTitleWords = cleanedWindowTitle.split(' ', Qt::SkipEmptyParts);

    QVector<QString> currentWindowTitles = getAllWindowTitles();
    for (const auto &windowTitle : currentWindowTitles) {
        QString cleanedTitle = cleanString(windowTitle.toLower());
        QStringList windowWords = cleanedTitle.split(' ', Qt::SkipEmptyParts);
        if (std::all_of(customWindowTitleWords.begin(),
                        customWindowTitleWords.end(),
                        [&windowWords](const QString &word) {
                            return windowWords.contains(word);
                        })) {
            return true;
        }
    }
    return false;
}

bool SteamWindowManager::isBigPictureWindowTitle(const QString &windowTitle)
{
    QString bigPictureTitle = cleanString(getBigPictureWindowTitle().toLower());
    QStringList bigPictureWords = bigPictureTitle.split(' ', Qt::SkipEmptyParts);

    QString cleanedTitle = cleanString(windowTitle.toLower());
    QStringList windowWords = cleanedTitle.split(' ', Qt::SkipEmptyParts);

    return std::all_of(bigPictureWords.begin(),
                       bigPictureWords.end(),
                       [&windowWords](const QString &word) {
                           return windowWords.contains(word);
                       });
}

bool SteamWindowManager::isCustomWindowTitle(const QString &windowTitle, const QString &customTitle)
{
    QString cleanedCustomTitle = cleanString(customTitle.toLower());
    QStringList customTitleWords = cleanedCustomTitle.split(' ', Qt::SkipEmptyParts);

    QString cleanedWindowTitle = cleanString(windowTitle.toLower());
    QStringList windowWords = cleanedWindowTitle.split(' ', Qt::SkipEmptyParts);

    return std::all_of(customTitleWords.begin(),
                       customTitleWords.end(),
                       [&windowWords](const QString &word) {
                           return windowWords.contains(word);
                       });
}
