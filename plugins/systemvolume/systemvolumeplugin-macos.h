/**
 * Copyright 2019 Weixuan XIAO <veyx.shaw@gmail.com>
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

#ifndef SYSTEMVOLUMEPLUGINMACOS_H
#define SYSTEMVOLUMEPLUGINMACOS_H

#include <QObject>
#include <QMap>

#include <core/kdeconnectplugin.h>

#import <CoreAudio/CoreAudio.h>

#define PACKET_TYPE_SYSTEMVOLUME QStringLiteral("kdeconnect.systemvolume")
#define PACKET_TYPE_SYSTEMVOLUME_REQUEST QStringLiteral("kdeconnect.systemvolume.request")

class MacOSCoreAudioDevice;

class Q_DECL_EXPORT SystemvolumePlugin : public KdeConnectPlugin
{
    Q_OBJECT

public:
    explicit SystemvolumePlugin(QObject *parent, const QVariantList &args);
    bool receivePacket(const NetworkPacket& np) override;
    void connected() override;
    void sendSinkList();

    void updateDeviceVolume(AudioDeviceID deviceId);
    void updateDeviceMuted(AudioDeviceID deviceId);
private:
    QMap<QString, MacOSCoreAudioDevice*> sinksMap;
};

#endif // SYSTEMVOLUMEPLUGINMACOS_H
