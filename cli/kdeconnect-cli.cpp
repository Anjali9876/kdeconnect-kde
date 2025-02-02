/*
 * Copyright 2015 Aleix Pol Gonzalez <aleixpol@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <QCryptographicHash>
#include <QIODevice>
#include <QDBusMessage>
#include <QCoreApplication>
#include <QTextStream>
#include <QFile>

#include <KAboutData>

#include "interfaces/devicesmodel.h"
#include "interfaces/notificationsmodel.h"
#include "interfaces/dbusinterfaces.h"
#include "interfaces/dbushelpers.h"
#include "kdeconnect-version.h"

#include <dbushelper.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    KAboutData about(QStringLiteral("kdeconnect-cli"),
                     QStringLiteral("kdeconnect-cli"),
                     QStringLiteral(KDECONNECT_VERSION_STRING),
                     i18n("KDE Connect CLI tool"),
                     KAboutLicense::GPL,
                     i18n("(C) 2015 Aleix Pol Gonzalez"));
    KAboutData::setApplicationData(about);

    about.addAuthor( i18n("Aleix Pol Gonzalez"), QString(), QStringLiteral("aleixpol@kde.org") );
    about.addAuthor( i18n("Albert Vaca Cintora"), QString(), QStringLiteral("albertvaka@gmail.com") );
    QCommandLineParser parser;
    parser.addOption(QCommandLineOption(QStringList(QStringLiteral("l")) << QStringLiteral("list-devices"), i18n("List all devices")));
    parser.addOption(QCommandLineOption(QStringList(QStringLiteral("a")) << QStringLiteral("list-available"), i18n("List available (paired and reachable) devices")));
    parser.addOption(QCommandLineOption(QStringLiteral("id-only"), i18n("Make --list-devices or --list-available print only the devices id, to ease scripting")));
    parser.addOption(QCommandLineOption(QStringLiteral("name-only"), i18n("Make --list-devices or --list-available print only the devices name, to ease scripting")));
    parser.addOption(QCommandLineOption(QStringLiteral("id-name-only"), i18n("Make --list-devices or --list-available print only the devices id and name, to ease scripting")));
    parser.addOption(QCommandLineOption(QStringLiteral("refresh"), i18n("Search for devices in the network and re-establish connections")));
    parser.addOption(QCommandLineOption(QStringLiteral("pair"), i18n("Request pairing to a said device")));
    parser.addOption(QCommandLineOption(QStringLiteral("ring"), i18n("Find the said device by ringing it.")));
    parser.addOption(QCommandLineOption(QStringLiteral("unpair"), i18n("Stop pairing to a said device")));
    parser.addOption(QCommandLineOption(QStringLiteral("ping"), i18n("Sends a ping to said device")));
    parser.addOption(QCommandLineOption(QStringLiteral("ping-msg"), i18n("Same as ping but you can set the message to display"), i18n("message")));
    parser.addOption(QCommandLineOption(QStringLiteral("share"), i18n("Share a file to a said device"), QStringLiteral("path")));
    parser.addOption(QCommandLineOption(QStringLiteral("share-text"), i18n("Share text to a said device"), QStringLiteral("text")));
    parser.addOption(QCommandLineOption(QStringLiteral("list-notifications"), i18n("Display the notifications on a said device")));
    parser.addOption(QCommandLineOption(QStringLiteral("lock"), i18n("Lock the specified device")));
    parser.addOption(QCommandLineOption(QStringLiteral("send-sms"), i18n("Sends an SMS. Requires destination"), i18n("message")));
    parser.addOption(QCommandLineOption(QStringLiteral("destination"), i18n("Phone number to send the message"), i18n("phone number")));
    parser.addOption(QCommandLineOption(QStringList(QStringLiteral("device")) << QStringLiteral("d"), i18n("Device ID"), QStringLiteral("dev")));
    parser.addOption(QCommandLineOption(QStringList(QStringLiteral("name")) << QStringLiteral("n"), i18n("Device Name"), QStringLiteral("name")));
    parser.addOption(QCommandLineOption(QStringLiteral("encryption-info"), i18n("Get encryption info about said device")));
    parser.addOption(QCommandLineOption(QStringLiteral("list-commands"), i18n("Lists remote commands and their ids")));
    parser.addOption(QCommandLineOption(QStringLiteral("execute-command"), i18n("Executes a remote command by id"), QStringLiteral("id")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("k"), QStringLiteral("send-keys")}, i18n("Sends keys to a said device"), QStringLiteral("key")));
    parser.addOption(QCommandLineOption(QStringLiteral("my-id"), i18n("Display this device's id and exit")));
    parser.addOption(QCommandLineOption(QStringLiteral("photo"), i18n("Open the connected device's camera and transfer the photo")));

    //Hidden because it's an implementation detail
    QCommandLineOption deviceAutocomplete(QStringLiteral("shell-device-autocompletion"));
    deviceAutocomplete.setHidden(true);
    deviceAutocomplete.setDescription(QStringLiteral("Outputs all available devices id's with their name and paired status")); //Not visible, so no translation needed
    deviceAutocomplete.setValueName(QStringLiteral("shell"));
    parser.addOption(deviceAutocomplete);
    about.setupCommandLine(&parser);

    parser.addHelpOption();
    parser.process(app);
    about.processCommandLine(&parser);

    const QString id = QStringLiteral("kdeconnect-cli-") + QString::number(QCoreApplication::applicationPid());
    DaemonDbusInterface iface;

    if (parser.isSet(QStringLiteral("my-id"))) {
        QTextStream(stdout) << iface.selfId() << endl;
    } else if (parser.isSet(QStringLiteral("l")) || parser.isSet(QStringLiteral("a"))) {
        bool reachable = false;
        if (parser.isSet(QStringLiteral("a"))) {
            reachable = true;
        } else {
            blockOnReply(iface.acquireDiscoveryMode(id));
            QThread::sleep(2);
        }
        const QStringList devices = blockOnReply<QStringList>(iface.devices(reachable, false));

        bool displayCount = true;
        for (const QString& id : devices) {
            if (parser.isSet(QStringLiteral("id-only"))) {
                QTextStream(stdout) << id << endl;
                displayCount = false;
            } else if (parser.isSet(QStringLiteral("name-only"))) {
                DeviceDbusInterface deviceIface(id);
                QTextStream(stdout) << deviceIface.name() << endl;
                displayCount = false;
            } else if (parser.isSet(QStringLiteral("id-name-only"))) {
                DeviceDbusInterface deviceIface(id);
                QTextStream(stdout) << id << ' ' << deviceIface.name() << endl;
                displayCount = false;
            } else {
                DeviceDbusInterface deviceIface(id);
                QString statusInfo;
                const bool isReachable = deviceIface.isReachable();
                const bool isTrusted = deviceIface.isTrusted();
                if (isReachable && isTrusted) {
                    statusInfo = i18n("(paired and reachable)");
                } else if (isReachable) {
                    statusInfo = i18n("(reachable)");
                } else if (isTrusted) {
                    statusInfo = i18n("(paired)");
                }
                QTextStream(stdout) << "- " << deviceIface.name()
                        << ": " << deviceIface.id() << ' ' << statusInfo << endl;
            }
        }
        if (displayCount) {
            QTextStream(stderr) << i18np("1 device found", "%1 devices found", devices.size()) << endl;
        } else if (devices.isEmpty()) {
            QTextStream(stderr) << i18n("No devices found") << endl;
        }

        blockOnReply(iface.releaseDiscoveryMode(id));
    } else if (parser.isSet(QStringLiteral("shell-device-autocompletion"))) {
        //Outputs a list of reachable devices in zsh autocomplete format, with the name as description
        const QStringList devices = blockOnReply<QStringList>(iface.devices(true, false));
        for (const QString &id : devices) {
            DeviceDbusInterface deviceIface(id);
            QString statusInfo;
            const bool isTrusted = deviceIface.isTrusted();
            if (isTrusted) {
                statusInfo = i18n("(paired)");
            } else {
                statusInfo = i18n("(unpaired)");
            }

            //Description: "device name (paired/unpaired)"
            QString description = deviceIface.name() + QLatin1Char(' ') + statusInfo;
            //Replace characters
            description.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
            description.replace(QLatin1Char('['),  QStringLiteral("\\["));
            description.replace(QLatin1Char(']'),  QStringLiteral("\\]"));
            description.replace(QLatin1Char('\''), QStringLiteral("\\'"));
            description.replace(QLatin1Char('\"'), QStringLiteral("\\\""));
            description.replace(QLatin1Char('\n'), QLatin1Char(' '));
            description.remove(QLatin1Char('\0'));

            //Output id and description
            QTextStream(stdout) << id << '[' << description << ']' << endl;
        }
    } else if(parser.isSet(QStringLiteral("refresh"))) {
        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect"), QStringLiteral("org.kde.kdeconnect.daemon"), QStringLiteral("forceOnNetworkChange"));
        blockOnReply(DbusHelper::sessionBus().asyncCall(msg));
    } else {

        QString device = parser.value(QStringLiteral("device"));
        if (device.isEmpty() && parser.isSet(QStringLiteral("name"))) {
            device = blockOnReply(iface.deviceIdByName(parser.value(QStringLiteral("name"))));
            if (device.isEmpty()) {
                QTextStream(stderr) << "Couldn't find device: " << parser.value(QStringLiteral("name")) << endl;
                return 1;
            }
        }
        if(device.isEmpty()) {
            QTextStream(stderr) << i18n("No device specified") << endl;
            parser.showHelp(1);
        }

        if (parser.isSet(QStringLiteral("share"))) {
            QStringList urls;
            
            QUrl url = QUrl::fromUserInput(parser.value(QStringLiteral("share")), QDir::currentPath());
            urls.append(url.toString());

            // Check for more arguments
            const auto args = parser.positionalArguments();
            for (const QString& input : args) {
                QUrl url = QUrl::fromUserInput(input, QDir::currentPath());
                urls.append(url.toString());
            }

            QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect/devices/") + device + QStringLiteral("/share"),
                                                              QStringLiteral("org.kde.kdeconnect.device.share"), QStringLiteral("shareUrls"));
            
            msg.setArguments(QVariantList() << QVariant(urls));
            blockOnReply(DbusHelper::sessionBus().asyncCall(msg));
        
            for (const QString& url : qAsConst(urls)) {
                QTextStream(stdout) << i18n("Shared %1", url) << endl;
            }
        } else if (parser.isSet(QStringLiteral("share-text"))) {
            QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect/devices/") + device + QStringLiteral("/share"), QStringLiteral("org.kde.kdeconnect.device.share"), QStringLiteral("shareText"));
            msg.setArguments(QVariantList() << parser.value(QStringLiteral("share-text")));
            blockOnReply(DbusHelper::sessionBus().asyncCall(msg));
            QTextStream(stdout) << i18n("Shared text: %1", parser.value(QStringLiteral("share-text"))) << endl;
        } else if(parser.isSet(QStringLiteral("pair"))) {
            DeviceDbusInterface dev(device);
            if (!dev.isReachable()) {
                //Device doesn't exist, go into discovery mode and wait up to 30 seconds for the device to appear
                QEventLoop wait;
                QTextStream(stderr) << i18n("waiting for device...") << endl;
                blockOnReply(iface.acquireDiscoveryMode(id));

                QObject::connect(&iface, &DaemonDbusInterface::deviceAdded, &iface, [&](const QString& deviceAddedId) {
                    if (device == deviceAddedId) {
                        wait.quit();
                    }
                });
                QTimer::singleShot(30 * 1000, &wait, &QEventLoop::quit);

                wait.exec();
            }

            if (!dev.isReachable()) {
                QTextStream(stderr) << i18n("Device not found") << endl;
            } else if(blockOnReply<bool>(dev.isTrusted())) {
                QTextStream(stderr) << i18n("Already paired") << endl;
            } else {
                QTextStream(stderr) << i18n("Pair requested") << endl;
                blockOnReply(dev.requestPair());
            }
            blockOnReply(iface.releaseDiscoveryMode(id));
        } else if(parser.isSet(QStringLiteral("unpair"))) {
            DeviceDbusInterface dev(device);
            if (!dev.isTrusted()) {
                QTextStream(stderr) << i18n("Already not paired") << endl;
            } else {
                QTextStream(stderr) << i18n("Unpaired") << endl;
                blockOnReply(dev.unpair());
            }
        } else if(parser.isSet(QStringLiteral("ping")) || parser.isSet(QStringLiteral("ping-msg"))) {
            QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect/devices/") + device + QStringLiteral("/ping"), QStringLiteral("org.kde.kdeconnect.device.ping"), QStringLiteral("sendPing"));
            if (parser.isSet(QStringLiteral("ping-msg"))) {
                QString message = parser.value(QStringLiteral("ping-msg"));
                msg.setArguments(QVariantList() << message);
            }
            blockOnReply(DbusHelper::sessionBus().asyncCall(msg));
        } else if(parser.isSet(QStringLiteral("send-sms"))) {
            if (parser.isSet(QStringLiteral("destination"))) {
                QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect/devices/") + device + QStringLiteral("/sms"), QStringLiteral("org.kde.kdeconnect.device.sms"), QStringLiteral("sendSms"));
                msg.setArguments({ parser.value(QStringLiteral("destination")), parser.value(QStringLiteral("send-sms"))});
                blockOnReply(DbusHelper::sessionBus().asyncCall(msg));
            } else {
                QTextStream(stderr) << i18n("error: should specify the SMS's recipient by passing --destination <phone number>");
                return 1;
            }
        } else if(parser.isSet(QStringLiteral("ring"))) {
            QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect/devices/") + device + QStringLiteral("/findmyphone"), QStringLiteral("org.kde.kdeconnect.device.findmyphone"), QStringLiteral("ring"));
            blockOnReply(DbusHelper::sessionBus().asyncCall(msg));
        } else if(parser.isSet(QStringLiteral("photo"))) {
            QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect/devices/") + device + QStringLiteral("/photo"), QStringLiteral("org.kde.kdeconnect.device.photo"), QStringLiteral("requestPhoto"));
            blockOnReply(DbusHelper::sessionBus().asyncCall(msg));
        } else if(parser.isSet(QStringLiteral("send-keys"))) {
            QString seq = parser.value(QStringLiteral("send-keys"));
            QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kdeconnect"), QStringLiteral("/modules/kdeconnect/devices/") + device + QStringLiteral("/remotekeyboard"), QStringLiteral("org.kde.kdeconnect.device.remotekeyboard"), QStringLiteral("sendKeyPress"));
            if (seq.trimmed() == QLatin1String("-")) {
                // from stdin
                QFile in;
                if(in.open(stdin,QIODevice::ReadOnly | QIODevice::Unbuffered)) {
                    while (!in.atEnd()) {
                        QByteArray line = in.readLine();  // sanitize to ASCII-codes > 31?
                        msg.setArguments({QString::fromLatin1(line), -1, false, false, false});
                        blockOnReply(DbusHelper::sessionBus().asyncCall(msg));
                    }
                    in.close();
                }
            } else {
                msg.setArguments({seq, -1, false, false, false});
                blockOnReply(DbusHelper::sessionBus().asyncCall(msg));
            }
        } else if(parser.isSet(QStringLiteral("list-notifications"))) {
            NotificationsModel notifications;
            notifications.setDeviceId(device);
            for(int i=0, rows=notifications.rowCount(); i<rows; ++i) {
                QModelIndex idx = notifications.index(i);
                QTextStream(stdout) << "- " << idx.data(NotificationsModel::AppNameModelRole).toString()
                    << ": " << idx.data(NotificationsModel::NameModelRole).toString() << endl;
            }
        } else if(parser.isSet(QStringLiteral("list-commands"))) {
            RemoteCommandsDbusInterface iface(device);
            const auto cmds = QJsonDocument::fromJson(iface.commands()).object();
            for (auto it = cmds.constBegin(), itEnd = cmds.constEnd(); it!=itEnd; ++it) {
                const QJsonObject cont = it->toObject();
                QTextStream(stdout) << it.key() << ": " << cont.value(QStringLiteral("name")).toString() << ": " << cont.value(QStringLiteral("command")).toString() << endl;
            }
        } else if(parser.isSet(QStringLiteral("execute-command"))) {
            RemoteCommandsDbusInterface iface(device);
            blockOnReply(iface.triggerCommand(parser.value(QStringLiteral("execute-command"))));
        } else if(parser.isSet(QStringLiteral("encryption-info"))) {
            DeviceDbusInterface dev(device);
            QString info = blockOnReply<QString>(dev.encryptionInfo()); // QSsl::Der = 1
            QTextStream(stdout) << info << endl;
        } else {
            QTextStream(stderr) << i18n("Nothing to be done") << endl;
        }
    }
    QMetaObject::invokeMethod(&app, "quit", Qt::QueuedConnection);

    return app.exec();
}
