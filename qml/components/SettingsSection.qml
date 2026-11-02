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

import Lomiri.Components 1.3
import QtQuick 2.7
import QtQuick.Layouts 1.3

ColumnLayout {
    id: root

    property string title
    default property alias content: body.data

    Layout.fillWidth: true
    spacing: 0

    Item {
        Layout.preferredHeight: units.gu(2)
    }

    ListItem {
        Layout.preferredHeight: sectionLabel.height + units.gu(2)
        Layout.fillWidth: true
        divider.visible: true

        Label {
            id: sectionLabel

            text: root.title
            font.pixelSize: units.gu(2)

            anchors {
                left: parent.left
                leftMargin: units.gu(2)
                verticalCenter: parent.verticalCenter
            }

        }

    }

    ColumnLayout {
        id: body

        Layout.fillWidth: true
        spacing: 0
    }

}
