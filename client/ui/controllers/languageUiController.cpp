#include "languageUiController.h"

LanguageUiController::LanguageUiController(SettingsController* settingsController,
                                           LanguageModel* languageModel,
                                           QObject *parent)
    : QObject(parent),
      m_settingsController(settingsController),
      m_languageModel(languageModel)
{
}

void LanguageUiController::onAppLanguageChanged(const QLocale &locale)
{
    emit updateTranslations(locale);
}

void LanguageUiController::changeLanguage(const LanguageSettings::AvailableLanguageEnum language)
{
    QLocale locale = languageEnumToLocale(language);
    m_settingsController->setAppLanguage(locale);
}

int LanguageUiController::getCurrentLanguageIndex() const
{
    auto locale = m_settingsController->getAppLanguage();
    switch (locale.language()) {
    case QLocale::English: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::English); break;
    case QLocale::Russian: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::Russian); break;
    case QLocale::French:  return static_cast<int>(LanguageSettings::AvailableLanguageEnum::French); break;
    case QLocale::Chinese: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::China_cn); break;
    default: return static_cast<int>(LanguageSettings::AvailableLanguageEnum::English); break;
    }
}

int LanguageUiController::getLineHeightAppend() const
{
    return 0;
}

QString LanguageUiController::getCurrentLanguageName() const
{
    int index = getCurrentLanguageIndex();
    return getLocalLanguageName(static_cast<LanguageSettings::AvailableLanguageEnum>(index));
}

LanguageSettings::AvailableLanguageEnum LanguageUiController::getSystemLanguageEnum() const
{
    QLocale locale = QLocale::system();
    switch (locale.language()) {
    case QLocale::Russian: return LanguageSettings::AvailableLanguageEnum::Russian;
    case QLocale::French:  return LanguageSettings::AvailableLanguageEnum::French;
    case QLocale::Chinese: return LanguageSettings::AvailableLanguageEnum::China_cn;
    case QLocale::English: return LanguageSettings::AvailableLanguageEnum::English;
    default: return LanguageSettings::AvailableLanguageEnum::English;
    }
}

QString LanguageUiController::getCurrentSiteUrl(const QString &path) const
{
    Q_UNUSED(path);
    return QString();
}

QString LanguageUiController::getCurrentDocsUrl(const QString &path) const
{
    Q_UNUSED(path);
    return QString();
}

QString LanguageUiController::getLocalLanguageName(const LanguageSettings::AvailableLanguageEnum language) const
{
    QString strLanguage("");
    switch (language) {
    case LanguageSettings::AvailableLanguageEnum::English: strLanguage = "English"; break;
    case LanguageSettings::AvailableLanguageEnum::Russian: strLanguage = "Русский"; break;
    case LanguageSettings::AvailableLanguageEnum::French:  strLanguage = "Français"; break;
    case LanguageSettings::AvailableLanguageEnum::China_cn: strLanguage = "\347\256\200\344\275\223\344\270\255\346\226\207"; break;
    default: break;
    }

    return strLanguage;
}

QLocale LanguageUiController::languageEnumToLocale(const LanguageSettings::AvailableLanguageEnum language) const
{
    switch (language) {
    case LanguageSettings::AvailableLanguageEnum::English: return QLocale::English;
    case LanguageSettings::AvailableLanguageEnum::Russian: return QLocale::Russian;
    case LanguageSettings::AvailableLanguageEnum::French:  return QLocale::French;
    case LanguageSettings::AvailableLanguageEnum::China_cn: return QLocale::Chinese;
    default: return QLocale::English;
    }
}

