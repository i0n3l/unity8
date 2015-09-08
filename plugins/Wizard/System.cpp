/*
 * Copyright (C) 2014-2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranties of
 * MERCHANTABILITY, SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "System.h"

#include <QDBusPendingCall>
#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QMap>
#include <QProcess>
#include <QDebug>

System::System()
    : QObject()
{
    // Register the argument needed for UpdateActivationEnvironment below
    qDBusRegisterMetaType<QMap<QString,QString>>();

    m_fsWatcher.addPath(wizardEnabledPath());
    connect(&m_fsWatcher, &QFileSystemWatcher::fileChanged, this, &System::wizardEnabledChanged);
}

QString System::wizardEnabledPath()
{
    // Uses ubuntu-system-settings namespace for historic compatibility reasons
    return QDir::home().filePath(".config/ubuntu-system-settings/wizard-has-run");
}

bool System::wizardEnabled() const
{
    return !QFile::exists(wizardEnabledPath());
}

void System::setWizardEnabled(bool enabled)
{
    if (wizardEnabled() == enabled)
        return;

    if (enabled) {
        QFile::remove(wizardEnabledPath());
    } else {
        QDir(wizardEnabledPath()).mkpath("..");
        QFile(wizardEnabledPath()).open(QIODevice::WriteOnly);
        m_fsWatcher.addPath(wizardEnabledPath());
        wizardEnabledChanged();
    }
}

void System::setSessionVariable(const QString &variable, const QString &value)
{
    // We need to update both upstart's and DBus's environment
    QProcess::startDetached(QStringLiteral("initctl set-env --global %1=%2").arg(variable, value));

    QMap<QString,QString> valueMap;
    valueMap.insert(variable, value);

    QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.DBus",
                                                      "/org/freedesktop/DBus",
                                                      "org.freedesktop.DBus",
                                                      "UpdateActivationEnvironment");

    msg << QVariant::fromValue(valueMap);
    QDBusConnection::sessionBus().asyncCall(msg);
}

void System::updateSessionLocale(const QString &locale)
{
    const QString language = locale.split(".")[0];

    setSessionVariable("LANGUAGE", language);
    setSessionVariable("LANG", locale);
    setSessionVariable("LC_ALL", locale);

    // QLocale caches the default locale on startup, and Qt uses that cached
    // copy when formatting dates.  So manually update it here.
    QLocale::setDefault(QLocale(locale));

    // Restart bits of the session to pick up new language.
//    QProcess::startDetached("sh -c \"initctl emit indicator-services-end; \
//                                     initctl stop scope-registry; \
//                                     initctl stop smart-scopes-proxy; \
//                                     initctl emit --no-wait indicator-services-start; \
//                                     initctl restart --no-wait maliit-server; \
//                                     initctl restart --no-wait unity8-dash\"");
}
