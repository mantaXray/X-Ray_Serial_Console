/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: appsettings.cpp
 * Version: 1.00.00
 * Description: 应用 INI 配置文件访问实现
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#include "appsettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr auto kSettingsOrgName = "mantaXray";
constexpr auto kSettingsAppName = "X-Ray Serial Console";
constexpr auto kSettingsFileName = "xray_serial_console.ini";

/**
 * @brief 返回程序目录下的旧配置文件路径
 *
 * @param 无
 *
 * @return 程序目录下的 INI 路径
 */
QString previousAppDirectorySettingsFilePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QString::fromLatin1(kSettingsFileName));
}

/**
 * @brief 判断路径是否位于 CMake 构建目录内
 *
 * @param directoryPath: 待检查目录
 *
 * @return `true` 表示该目录或其父目录包含 CMake 构建标记
 */
bool isCMakeBuildDirectory(const QString &directoryPath)
{
    QDir directory(directoryPath);
    while (directory.exists()) {
        if (QFileInfo::exists(directory.filePath(QStringLiteral("CMakeCache.txt")))
            || QFileInfo::exists(directory.filePath(QStringLiteral("CMakeFiles")))) {
            return true;
        }

        if (!directory.cdUp()) {
            break;
        }
    }

    return false;
}

/**
 * @brief 返回稳定的用户配置目录
 *
 * @param 无
 *
 * @return 可写配置目录路径
 */
QString stableSettingsDirectory()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.trimmed().isEmpty()) {
        configDir = QDir::currentPath();
    }
    QDir().mkpath(configDir);
    return configDir;
}

/**
 * @brief 返回首选配置目录
 *
 * @param 无
 *
 * @return 可写配置目录路径
 */
QString preferredSettingsDirectory()
{
    const QString appDirPath = QCoreApplication::applicationDirPath();
    if (!appDirPath.trimmed().isEmpty()) {
        const QFileInfo appDirInfo(appDirPath);
        if (appDirInfo.exists() && appDirInfo.isWritable() && !isCMakeBuildDirectory(appDirPath)) {
            return appDirPath;
        }
    }

    return stableSettingsDirectory();
}

/**
 * @brief 把一个 QSettings 的全部键复制到目标配置
 *
 * @param target: 目标配置
 * @param source: 来源配置
 *
 * @return 无
 */
void copySettings(QSettings &target, QSettings &source)
{
    for (const QString &key : source.allKeys()) {
        target.setValue(key, source.value(key));
    }
}

/**
 * @brief 首次使用 INI 文件时迁移旧的原生配置
 *
 * @param 无
 *
 * @return 无
 */
void ensureSettingsFileInitialized()
{
    static bool initialized = false;
    if (initialized) {
        return;
    }
    initialized = true;

    const QString filePath = appSettingsFilePath();
    if (QFileInfo::exists(filePath)) {
        return;
    }

    QSettings target(filePath, QSettings::IniFormat);
    const QString previousAppDirFilePath = previousAppDirectorySettingsFilePath();
    const QFileInfo targetInfo(filePath);
    const QFileInfo previousInfo(previousAppDirFilePath);
    if (previousInfo.exists() && previousInfo.absoluteFilePath() != targetInfo.absoluteFilePath()) {
        QSettings previous(previousAppDirFilePath, QSettings::IniFormat);
        copySettings(target, previous);
    } else {
        QSettings legacy(QString::fromLatin1(kSettingsOrgName), QString::fromLatin1(kSettingsAppName));
        copySettings(target, legacy);
    }

    target.setValue(QStringLiteral("config/file_version"), 1);
    target.sync();
}

} // namespace

QString appSettingsFilePath()
{
    return QDir(preferredSettingsDirectory()).filePath(QString::fromLatin1(kSettingsFileName));
}

QSettings createAppSettings()
{
    ensureSettingsFileInitialized();
    return QSettings(appSettingsFilePath(), QSettings::IniFormat);
}
