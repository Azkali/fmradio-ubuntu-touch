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

HapticButton {
    id: root

    property bool isEmpty
    property bool active

    Component {
        id: pinContent

        Row {
            spacing: units.gu(0.5)

            Icon {
                name: "unpinned"
                width: units.gu(2)
                height: units.gu(2)
                color: theme.palette.normal.foregroundText
            }

            Label {
                text: "Pin"
            }

        }

    }

    Component {
        id: textContent

        Label {
            text: root.text
        }

    }

    contentItem: Loader {
        sourceComponent: root.isEmpty ? pinContent : textContent
    }

    background: Rectangle {
        radius: width / 2
        color: root.active ? theme.palette.normal.foreground : theme.palette.normal.background
        border.width: units.gu(0.1)
        border.color: theme.palette.normal.base
    }

}
