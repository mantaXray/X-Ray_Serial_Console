/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: appsettings.h
 * Version: 1.00.00
 * Description: 应用 INI 配置文件访问接口
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QSettings>
#include <QString>

/**
 * @brief 返回当前应用使用的 INI 配置文件路径
 *
 * @param 无
 *
 * @return 配置文件绝对路径
 */
QString appSettingsFilePath();

/**
 * @brief 创建指向统一 INI 配置文件的设置对象
 *
 * @param 无
 *
 * @return 指向应用配置文件的 QSettings 对象
 */
QSettings createAppSettings();

#endif // APPSETTINGS_H
