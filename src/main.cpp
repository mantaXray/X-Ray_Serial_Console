/**
 * Copyright (c) 2026, mantaXray
 * All rights reserved.
 *
 * Filename: main.cpp
 * Version: 1.00.00
 * Description: X-Ray Serial Console entry point
 *
 * Author: mantaXray
 * Date: 2026年04月28日
 */

#include "appversion.h"
#include "mainwindow.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QStyleFactory>

namespace {

/**
 * @brief 构建应用固定使用的亮色调色板
 *
 * @param 无
 *
 * @return 统一的蓝白科技风调色板
 */
QPalette buildXRayPalette()
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(QStringLiteral("#f3f7fc")));
    palette.setColor(QPalette::WindowText, QColor(QStringLiteral("#16324f")));
    palette.setColor(QPalette::Base, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#edf4ff")));
    palette.setColor(QPalette::Text, QColor(QStringLiteral("#16324f")));
    palette.setColor(QPalette::Button, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#16324f")));
    palette.setColor(QPalette::BrightText, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#2563eb")));
    palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::ToolTipBase, QColor(QStringLiteral("#ffffff")));
    palette.setColor(QPalette::ToolTipText, QColor(QStringLiteral("#16324f")));
    palette.setColor(QPalette::Link, QColor(QStringLiteral("#2563eb")));

    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(QStringLiteral("#8aa0b7")));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(QStringLiteral("#8aa0b7")));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(QStringLiteral("#8aa0b7")));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(QStringLiteral("#f1f6fb")));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(QStringLiteral("#f1f6fb")));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(QStringLiteral("#b9d0f5")));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(QStringLiteral("#f8fbff")));
    return palette;
}

} // namespace

/**
 * @brief 程序主入口
 *
 * @param argc: 命令行参数数量
 * @param argv: 命令行参数数组
 *
 * @return 进程退出码
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("mantaXray"));
    QCoreApplication::setApplicationName(QStringLiteral("X-Ray Serial Console"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(AppVersion::kSemanticVersion));
    a.setApplicationDisplayName(QStringLiteral("X-Ray 串口调试台"));
    a.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    a.setPalette(buildXRayPalette());
    a.setStyleSheet(QStringLiteral(R"(
        QToolTip {
            color: #16324f;
            background: #ffffff;
            border: 1px solid #bfd2e6;
            padding: 4px 8px;
        }
    )"));

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
