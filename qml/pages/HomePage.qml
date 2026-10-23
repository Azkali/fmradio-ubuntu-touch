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

import Qt.labs.settings 1.0
import QtQuick 2.7
import Lomiri.Components 1.3
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import MediatekRadio 1.0
import "../components"

Page {
    anchors.fill: parent

    ColumnLayout {
        anchors.horizontalCenter: parent.horizontalCenter

        anchors {
            top: header.bottom
        }

        RowLayout {
            Layout.topMargin: units.gu(8)
            Layout.alignment: Qt.AlignHCenter
            spacing: units.gu(1)

            Label {
                id: frequency

                Layout.alignment: Qt.AlignHCenter
                font.pixelSize: units.gu(10)
                text: MediatekRadio.frequency / 100
            }

            Label {
                font.pixelSize: units.gu(5)
                font.weight: Font.Light
                text: "MHz"
                color: LomiriColors.slate
                Layout.alignment: Qt.AlignBottom
                Layout.bottomMargin: units.gu(1)
            }

        }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: units.gu(4)
            spacing: units.gu(1)
            visible: MediatekRadio.rdsEnabled

            Label {
                Layout.alignment: Qt.AlignHCenter
                font.pixelSize: units.gu(3)
                visible: text !== ""
                text: MediatekRadio.stationName
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                color: LomiriColors.slate
                visible: text !== ""
                text: MediatekRadio.radioText
            }

        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: units.gu(2)
            spacing: units.gu(4)

            HapticButton {
                id: previousButton

                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: units.gu(6)
                Layout.preferredHeight: units.gu(6)
                onClicked: {
                    MediatekRadio.seekDown();
                }

                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        name: "media-skip-backward"
                        width: units.gu(3)
                        height: units.gu(3)
                        color: theme.palette.normal.foregroundText
                    }

                }

                background: RoundButtonBackground {
                }

            }

            HapticButton {
                id: startStopButton

                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: units.gu(10)
                Layout.preferredHeight: units.gu(10)
                onClicked: {
                    if (!MediatekRadio.running)
                        MediatekRadio.startRadio(MediatekRadio.frequency);
                    else
                        MediatekRadio.stopRadio();
                }

                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        name: MediatekRadio.running ? "media-playback-pause" : "media-playback-start"
                        width: units.gu(5)
                        height: units.gu(5)
                        color: theme.palette.normal.foregroundText
                    }

                }

                background: RoundButtonBackground {
                }

            }

            HapticButton {
                id: forwardButton

                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: units.gu(6)
                Layout.preferredHeight: units.gu(6)
                onClicked: {
                    MediatekRadio.seekUp();
                }

                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        name: "media-skip-forward"
                        width: units.gu(3)
                        height: units.gu(3)
                        color: theme.palette.normal.foregroundText
                    }

                }

                background: RoundButtonBackground {
                }

            }

        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: units.gu(2)
            spacing: units.gu(2)

            HapticButton {
                Layout.alignment: Qt.AlignHCenter
                onClicked: {
                    MediatekRadio.stepDown();
                }

                contentItem: Row {
                    spacing: units.gu(0.5)

                    Icon {
                        name: "list-remove"
                        width: units.gu(2)
                        height: units.gu(2)
                        color: theme.palette.normal.foregroundText
                    }

                    Label {
                        text: "0.1"
                    }

                }

                background: RoundButtonBackground {
                }

            }

            HapticButton {
                id: recordButton

                function formatTime(recordingDuration) {
                    const total = Math.max(0, Math.floor(recordingDuration));
                    const h = Math.floor(total / 3600);
                    const m = Math.floor((total % 3600) / 60);
                    const s = total % 60;
                    const pad = (n) => {
                        return n.toString().padStart(2, "0");
                    };
                    return pad(h) + ":" + pad(m) + ":" + pad(s);
                }

                Layout.alignment: Qt.AlignHCenter
                onClicked: {
                    if (!MediatekRadio.recording) {
                        MediatekRadio.startRecording();
                    } else {
                        MediatekRadio.stopRecording();
                        recordingIcon.opacity = 1;
                    }
                }

                contentItem: Row {
                    spacing: units.gu(0.5)

                    Icon {
                        id: recordingIcon

                        name: "media-record"
                        width: units.gu(2)
                        height: units.gu(2)

                        SequentialAnimation on opacity {
                            running: MediatekRadio.recording
                            loops: Animation.Infinite

                            NumberAnimation {
                                to: 0.2
                                duration: 1000
                            }

                            NumberAnimation {
                                to: 1
                                duration: 1000
                            }

                        }

                    }

                    Label {
                        id: recordLabel

                        text: !MediatekRadio.recording ? "Record" : recordButton.formatTime(MediatekRadio.recordingDuration)
                    }

                }

                background: RoundButtonBackground {
                }

            }

            HapticButton {
                Layout.alignment: Qt.AlignHCenter
                onClicked: {
                    MediatekRadio.stepUp();
                }

                contentItem: Row {
                    spacing: units.gu(0.5)

                    Label {
                        text: "0.1"
                    }

                    Icon {
                        name: "list-add"
                        width: units.gu(2)
                        height: units.gu(2)
                        color: theme.palette.normal.foregroundText
                    }

                }

                background: RoundButtonBackground {
                }

            }

        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: units.gu(2)
            spacing: units.gu(2)

            Repeater {
                model: MediatekRadio.presets

                PresetButton {
                    text: modelData / 100
                    isEmpty: modelData == 0
                    active: MediatekRadio.frequency == modelData
                    onClicked: {
                        if (isEmpty)
                            MediatekRadio.savePreset(index, MediatekRadio.frequency);
                        else
                            MediatekRadio.tune(text * 100);
                    }
                    onPressAndHold: {
                        if (!isEmpty)
                            MediatekRadio.clearPreset(index);

                    }
                }

            }

        }

    }

    BottomEdge {
        id: bottomEdge

        width: units.gu(45)
        height: units.gu(45)

        contentComponent: TextInput {
            id: frequencyInput

            font.pixelSize: units.gu(5)
            inputMask: "990.9;_"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            text: MediatekRadio.frequency / 100
            onEditingFinished: {
                MediatekRadio.tune(frequencyInput.text * 100);
                bottomEdge.collapse();
            }

            Connections {
                target: bottomEdge
                onCommitCompleted: {
                    focus = true;
                }
            }

            Connections {
                target: bottomEdge
                onCollapseCompleted: {
                    focus = false;
                }
            }

        }

    }

    header: PageHeader {
        id: header

        title: i18n.tr('FM Radio')

        trailingActionBar {
            actions: [
                Action {
                    iconName: "settings"
                    onTriggered: {
                        mainStack.push(Qt.resolvedUrl("SettingsPage.qml"));
                    }
                },
                Action {
                    iconName: "document-open"
                    onTriggered: MediatekRadio.openRecordingsFolder()
                },
                Action {
                    iconName: MediatekRadio.speakerOutput ? "audio-headphones-symbolic" : "audio-speakers-symbolic"
                    onTriggered: {
                        MediatekRadio.speakerOutput = MediatekRadio.speakerOutput ? false : true;
                    }
                }
            ]
        }

    }

}
