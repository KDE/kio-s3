/*
    SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.ScrollViewKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 30
    implicitHeight: Kirigami.Units.gridUnit * 20

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        width: parent.width - Kirigami.Units.gridUnit * 4
        visible: profileList.count === 0
        icon.name: "folder-cloud"
        text: i18n("No S3 profiles configured")
        explanation: i18n("Add a profile to connect to an S3-compatible storage backend")

        helpfulAction: Kirigami.Action {
            icon.name: "list-add-symbolic"
            text: i18n("Add Profile…")
            onTriggered: editDialog.openForNew()
        }
    }

    view: ListView {
        id: profileList
        model: kcm.profileModel

        delegate: Kirigami.SwipeListItem {
            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    source: "folder-cloud"
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: Kirigami.Units.iconSizes.medium
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: model.name
                        elide: Text.ElideRight
                    }

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: {
                            var parts = [];
                            if (model.endpointUrl) {
                                parts.push(model.endpointUrl);
                            } else {
                                parts.push(i18n("Default AWS endpoint"));
                            }
                            if (model.region) {
                                parts.push(model.region);
                            }
                            return parts.join(" \u00B7 ");
                        }
                        elide: Text.ElideRight
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                }
            }

            actions: [
                Kirigami.Action {
                    icon.name: "document-edit-symbolic"
                    text: i18n("Edit")
                    onTriggered: editDialog.openForEdit(model.index)
                },
                Kirigami.Action {
                    icon.name: "edit-delete-symbolic"
                    text: i18n("Delete")
                    onTriggered: deleteDialog.openForProfile(model.index, model.name)
                }
            ]
        }
    }

    footer: QQC2.ToolBar {
        contentItem: RowLayout {
            QQC2.ToolButton {
                icon.name: "list-add-symbolic"
                text: i18n("Add Profile…")
                onClicked: editDialog.openForNew()
            }
            Item { Layout.fillWidth: true }
        }
    }

    // ---- Edit Dialog ----
    Kirigami.Dialog {
        id: editDialog

        title: isNewProfile ? i18n("Add S3 Profile") : i18n("Edit S3 Profile")
        modal: true
        preferredWidth: Kirigami.Units.gridUnit * 25

        property bool isNewProfile: true
        property int editIndex: -1

        contentItem: Kirigami.FormLayout {
            QQC2.TextField {
                id: nameField
                Kirigami.FormData.label: i18n("Name:")
                placeholderText: i18n("e.g., My S3 Storage")
            }

            QQC2.TextField {
                id: endpointField
                Kirigami.FormData.label: i18n("Endpoint URL:")
                placeholderText: i18n("e.g., https://s3.example.com")
                inputMethodHints: Qt.ImhUrlCharactersOnly
            }

            QQC2.TextField {
                id: regionField
                Kirigami.FormData.label: i18n("Region:")
                placeholderText: i18n("e.g., us-east-1, auto")
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18n("Authentication")
            }

            QQC2.TextField {
                id: awsProfileField
                Kirigami.FormData.label: i18n("AWS Profile:")
                placeholderText: i18n("Profile from ~/.aws/credentials")
            }

            Kirigami.Separator {
                Kirigami.FormData.isSection: true
                Kirigami.FormData.label: i18n("Advanced")
            }

            QQC2.CheckBox {
                id: pathStyleCheck
                Kirigami.FormData.label: i18n("Addressing:")
                text: i18n("Use path-style S3 URLs")
            }
        }

        customFooterActions: [
            Kirigami.Action {
                text: editDialog.isNewProfile ? i18n("Add") : i18n("Save")
                icon.name: editDialog.isNewProfile ? "list-add-symbolic" : "document-save-symbolic"
                enabled: nameField.text.length > 0
                onTriggered: {
                    if (editDialog.isNewProfile) {
                        kcm.profileModel.addProfile(
                            nameField.text,
                            endpointField.text,
                            regionField.text,
                            awsProfileField.text,
                            pathStyleCheck.checked
                        );
                    } else {
                        kcm.profileModel.editProfile(
                            editDialog.editIndex,
                            nameField.text,
                            endpointField.text,
                            regionField.text,
                            awsProfileField.text,
                            pathStyleCheck.checked
                        );
                    }
                    editDialog.close();
                }
            }
        ]

        function openForNew() {
            isNewProfile = true;
            editIndex = -1;
            nameField.text = "";
            endpointField.text = "";
            regionField.text = "";
            awsProfileField.text = "";
            pathStyleCheck.checked = false;
            open();
            nameField.forceActiveFocus();
        }

        function openForEdit(index) {
            isNewProfile = false;
            editIndex = index;
            var profile = kcm.profileModel.profileAt(index);
            nameField.text = profile.name;
            endpointField.text = profile.endpointUrl;
            regionField.text = profile.region;
            awsProfileField.text = profile.awsProfile;
            pathStyleCheck.checked = profile.pathStyle;
            open();
            nameField.forceActiveFocus();
        }
    }

    // ---- Delete Confirmation ----
    Kirigami.PromptDialog {
        id: deleteDialog

        property int deleteIndex: -1

        title: i18n("Delete Profile")
        subtitle: ""
        standardButtons: Kirigami.Dialog.NoButton

        customFooterActions: [
            Kirigami.Action {
                text: i18n("Delete")
                icon.name: "edit-delete-symbolic"
                onTriggered: {
                    kcm.profileModel.removeProfile(deleteDialog.deleteIndex);
                    deleteDialog.close();
                }
            }
        ]

        function openForProfile(index, name) {
            deleteIndex = index;
            subtitle = i18n("Are you sure you want to delete the profile \"%1\"?", name);
            open();
        }
    }
}
