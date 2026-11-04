/*
 * Copyright (C) 2026 Nima Fanniasl <nimafanniasl@proton.me>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import Lomiri.Components 1.3
import Lomiri.Components.Popups 1.3
import MediatekRadio 1.0
import "../components"

Page {
    Flickable {
        id: flick

        anchors.fill: parent
        clip: true
        contentHeight: column.implicitHeight

        ColumnLayout {
            id: column

            width: flick.width
            spacing: 0

            SettingsSection {
                title: i18n.tr("Audio Output")

                SettingsRow {
                    iconName: "audio-speakers-symbolic"
                    title: i18n.tr("Mono audio for weak signals")
                    subtitle: i18n.tr("Reduces high-frequency multiplex static")

                    Switch {
                        SlotsLayout.position: SlotsLayout.Trailing
                        checked: MediatekRadio.mono
                        onCheckedChanged: MediatekRadio.mono = checked
                    }

                }

            }

            SettingsSection {
                title: i18n.tr("Tuning & Band")

                SettingsRow {
                    iconName: "broadcast"
                    title: i18n.tr("Decode RDS radiotext")
                    subtitle: i18n.tr("Live station metadata & program info")

                    Switch {
                        SlotsLayout.position: SlotsLayout.Trailing
                        checked: MediatekRadio.rdsEnabled
                        onCheckedChanged: MediatekRadio.rdsEnabled = checked
                    }

                }

                SettingsRow {
                    iconName: "webbrowser-app-symbolic"
                    title: i18n.tr("Region")
                    subtitle: MediatekRadio.region == MediatekRadio.RegionWorld ? "World • 50µs" : "Americas • 75µs"
                    onClicked: {
                        PopupUtils.open(selectRegionPopup);
                    }

                    ProgressionSlot {
                        SlotsLayout.overrideVerticalPositioning: true
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Component {
                        id: selectRegionPopup

                        Dialog {
                            id: selectRegionDialog

                            title: i18n.tr("Select your region:")

                            OptionSelector {
                                id: regionSelector

                                selectedIndex: MediatekRadio.region
                                model: [{
                                    "text": "Europe / Worldwide • 50µs"
                                }, {
                                    "text": "Americas / South Korea • 75µs"
                                }]
                                expanded: true

                                delegate: OptionSelectorDelegate {
                                    text: modelData.text
                                }

                            }

                            RowLayout {
                                width: parent.width
                                spacing: units.gu(1)

                                Button {
                                    text: i18n.tr("Save")
                                    color: LomiriColors.green
                                    Layout.fillWidth: true
                                    onClicked: {
                                        MediatekRadio.region = regionSelector.selectedIndex;
                                        PopupUtils.close(selectRegionDialog);
                                    }
                                }

                                Button {
                                    text: i18n.tr("Cancel")
                                    Layout.fillWidth: true
                                    onClicked: {
                                        PopupUtils.close(selectRegionDialog);
                                    }
                                }

                            }

                        }

                    }

                }

            }

            SettingsSection {
                title: i18n.tr("Recordings")

                SettingsRow {
                    iconName: "audio-input-microphone-symbolic"
                    title: i18n.tr("Capture Quality")
                    subtitle: i18n.tr("WAV Lossless (48 kHz, 16-bit)")
                }

                SettingsRow {
                    iconName: "folder-symbolic"
                    title: i18n.tr("Storage Location")
                    subtitle: "~/.local/share/fmradio.volla/recordings"
                    onClicked: MediatekRadio.openRecordingsFolder()

                    ProgressionSlot {
                        SlotsLayout.overrideVerticalPositioning: true
                        anchors.verticalCenter: parent.verticalCenter
                    }

                }

            }

        }

    }

    header: PageHeader {
        id: header

        title: i18n.tr("Radio Settings")
        flickable: flick
    }

}
