import QtQuick 2.0
import QtQuick.Controls 2.2

import ComputerManager 1.0

Item {
    // Etapas da tela de carregamento: 0 Conectar, 1 Liberar acesso (pareamento
    // automático - pulada quando o host já está pareado), 2 Abrir o jogo (vai
    // daqui até o primeiro quadro, já no StreamSegue).
    function onSearchingComputer() {
        loadingScreen.title = qsTr("Conectando ao servidor")
        loadingScreen.step = 0
        loadingScreen.stepProgress = 0.3
    }

    function onPairing() {
        loadingScreen.title = qsTr("Preparando seu acesso")
        loadingScreen.step = 1
        loadingScreen.stepProgress = 0.3
    }

    function onSearchingApp() {
        loadingScreen.title = qsTr("Abrindo seu jogo")
        loadingScreen.step = 2
        loadingScreen.stepProgress = 0.05
    }

    function onSessionCreated(appName, session) {
        var component = Qt.createComponent("StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": appName,
            "session": session,
            "quitAfter": true,
            "startProgress": Math.max(loadingScreen.shownProgress, 0.12)
        })
        // Sem animação de troca: as duas telas têm o mesmo fundo, então o
        // cliente só vê o título mudar.
        stackView.push(segue, {}, StackView.Immediate)
    }

    function onFailed(message) {
        loadingScreen.visible = false
        errorDialog.text = message
        errorDialog.open()
        console.error(message)
    }

    function onAppQuitRequired(appName) {
        quitAppDialog.appName = appName
        quitAppDialog.open()
    }

    // No PIN to read here (unlike CliPair.qml), so unlike that view there's
    // nothing useful for the user to cancel out of mid-pairing - keep the
    // same escape-quits behavior anyway for consistency.
    Keys.onEscapePressed: {
        Qt.quit()
    }
    Keys.onBackPressed: {
        Qt.quit()
    }
    Keys.onCancelPressed: {
        Qt.quit()
    }

    StackView.onActivated: {
        if (!launcher.isExecuted()) {
            toolBar.visible = false

            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.pairing.connect(onPairing)
            launcher.searchingApp.connect(onSearchingApp)
            launcher.sessionCreated.connect(onSessionCreated)
            launcher.failed.connect(onFailed)
            launcher.appQuitRequired.connect(onAppQuitRequired)
            launcher.execute(ComputerManager)
        }
    }

    LanhouseLoadingScreen {
        id: loadingScreen
        title: qsTr("Conectando ao servidor")
        step: 0
        stepProgress: 0.1
    }

    ErrorMessageDialog {
        id: errorDialog

        onClosed: {
            Qt.quit();
        }
    }

    NavigableMessageDialog {
        id: quitAppDialog
        text: qsTr("Are you sure you want to quit %1? Any unsaved progress will be lost.").arg(appName)
        standardButtons: Dialog.Yes | Dialog.No
        property string appName: ""

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var params = {"appName": appName, "quitRunningAppFn": function() { launcher.quitRunningApp() }}
            stackView.push(component.createObject(stackView, params))
        }

        onAccepted: quitApp()
        onRejected: Qt.quit()
    }
}
