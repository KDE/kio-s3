/*
    SPDX-FileCopyrightText: 2026 Nekto Oleg <27015@riseup.net>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import org.kde.kirigamiaddons.components as Components
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

    actions: QQC2.Action {
	icon.name: "list-add-symbolic"
	text: i18n("Add Profile…")
	onTriggered: editDialog.openForNew()
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

    // ---- Edit Dialog ----
    FormCard.FormCardDialog {
        id: editDialog

        title: isNewProfile ? i18n("Add S3 Profile") : i18n("Edit S3 Profile")
        modal: true
        parent: root.QQC2.Overlay.overlay

        property bool isNewProfile: true
        property int editIndex: -1

        standardButtons: FormCard.FormCardDialog.Ok | FormCard.FormCardDialog.Cancel

        Component.onCompleted: {
            const okButton = standardButton(FormCard.FormCardDialog.Ok);
            okButton.text = Qt.binding(() => editDialog.isNewProfile ? i18n("Add") : i18n("Save"))
            okButton.icon.name = Qt.binding(() =>  editDialog.isNewProfile ? "list-add-symbolic" : "document-save-symbolic")
            okButton.enabled = Qt.binding(() =>  nameField.text.length > 0)
        }

        onAccepted: {
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

        onRejected: editDialog.close();

        FormCard.FormTextFieldDelegate {
            id: nameField
            label: i18n("Name:")
            placeholderText: i18n("e.g., My S3 Storage")
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextFieldDelegate {
            id: endpointField
            label: i18n("Endpoint URL:")
            placeholderText: i18n("e.g., https://s3.example.com")
            inputMethodHints: Qt.ImhUrlCharactersOnly
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextFieldDelegate {
            id: regionField
            label: i18n("Region:")
            placeholderText: i18n("e.g., us-east-1, auto")
        }

        FormCard.FormDelegateSeparator {}

        Kirigami.Heading {
            text: i18nc("@title:group", "Authentication")
            level: 2
            type: Kirigami.Heading.Type.Primary
            Layout.leftMargin: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
            Layout.topMargin: Kirigami.Units.largeSpacing
        }

        FormCard.FormTextFieldDelegate {
            id: awsProfileField
            label: i18n("AWS Profile:")
            placeholderText: i18n("Profile from ~/.aws/credentials")
        }

        FormCard.FormDelegateSeparator {}

        Kirigami.Heading {
            text: i18nc("@title:group", "Advanced")
            level: 2
            type: Kirigami.Heading.Type.Primary
            Layout.leftMargin: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
            Layout.topMargin: Kirigami.Units.largeSpacing
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Advanced")
        }

        FormCard.FormCheckDelegate {
            id: pathStyleCheck
            //label: i18n("Addressing:")
            text: i18n("Use path-style S3 URLs")
        }

        function openForNew(): void {
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

        function openForEdit(index: int): void {
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
    Components.MessageDialog {
        id: deleteDialog

        property int deleteIndex: -1
        property string name: ''

        title: i18n("Delete Profile")
        subtitle: i18n("Are you sure you want to delete the profile \"%1\"?", name)
        standardButtons: Components.MessageDialog.Ok | Components.MessageDialog.Cancel
        dialogType: Components.MessageDialog.Warning

        Component.onCompleted: {
            const okButton = standardButton(Kirigami.Dialog.Ok);
            okButton.text = i18n("Delete")
            okButton.icon.name = "edit-delete-symbolic"
        }

        onAccepted: {
            kcm.profileModel.removeProfile(deleteDialog.deleteIndex);
            deleteDialog.close();
        }

        onRejected: deleteDialog.close()

        function openForProfile(index: int, profileName: string): void {
            deleteIndex = index;
            name = profileName;
            open();
        }
    }
}
