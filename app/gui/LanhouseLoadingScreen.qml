import QtQuick 2.15

// Tela de carregamento LanHouse (canvas "Tela de Carregamento LanHouse",
// alternativa C "Etapas nomeadas" aprovada em 2026-10-17; identidade Ultra
// Violet + Amarelo ácido + Inter). Usada pelas duas telas que o cliente vê
// antes do stream: LanhouseConnectSegue (conexão/pareamento) e StreamSegue
// (início do stream).
//
// O progresso é real: quem usa a tela informa `step` (0 Conectar, 1 Liberar
// acesso, 2 Abrir o jogo) e `stepProgress` (0..1 dentro da etapa atual).
// Etapas anteriores aparecem concluídas (barra cheia + check).
Rectangle {
    id: root

    property string title: ""
    property string hint: ""
    // false enquanto a animação não deve rodar (ver spinnerTimer no StreamSegue)
    property bool busy: true

    property int step: 0
    property real stepProgress: 0

    readonly property var stepNames: [qsTr("Conectar"), qsTr("Liberar acesso"), qsTr("Preparando o PC e abrindo o jogo")]
    // Canvas "Texto 1" (aprovado 2026-10-17): a última etapa é mais larga -
    // cabe o nome numa linha e mostra que é a etapa mais demorada.
    readonly property var stepWidths: [140, 140, 260]

    readonly property color violet: "#4A12D6"
    readonly property color acid: "#EFFF00"
    readonly property color ink: "#F3EFFF"
    readonly property color inkSoft: "#EDE8FF"
    readonly property color muted: "#ADA3D6"
    readonly property color trackColor: "#24F3EFFF"

    // O que a barra da etapa atual mostra: nunca recua dentro da etapa e,
    // entre um avanço real e outro, anda devagar (até stepProgress + 0.08)
    // pra não parecer travada, sem passar da próxima marca real.
    property real shownProgress: 0
    property bool animateShown: true
    Behavior on shownProgress {
        enabled: root.animateShown
        NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
    }
    // Valores passados na criação (ex.: StreamSegue continuando de onde o
    // LanhouseConnectSegue parou) não disparam os handlers abaixo.
    Component.onCompleted: {
        animateShown = false
        shownProgress = stepProgress
        animateShown = true
    }
    onStepChanged: {
        animateShown = false
        shownProgress = stepProgress
        animateShown = true
    }
    onStepProgressChanged: {
        if (stepProgress > shownProgress) {
            shownProgress = stepProgress
        }
    }
    Timer {
        interval: 250
        repeat: true
        running: root.busy && root.visible
        onTriggered: {
            var cap = Math.min(root.stepProgress + 0.08, 0.95)
            if (root.shownProgress < cap) {
                root.shownProgress = Math.min(root.shownProgress + 0.004, cap)
            }
        }
    }

    anchors.fill: parent
    color: "#0D0818"

    FontLoader { id: interSemiBold; source: "qrc:/data/Inter-SemiBold.ttf" }
    FontLoader { id: interBold; source: "qrc:/data/Inter-Bold.ttf" }

    // Brilho violeta no topo (sem depender de Qt5Compat.GraphicalEffects)
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height * 0.65
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1C1148" }
            GradientStop { position: 1.0; color: "#000D0818" }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 44

        // Logo: controle amarelo (mesmo ícone do topo do site) + "LanHouse"
        // - canvas "Logo 2", aprovado 2026-10-17.
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            Image {
                source: "qrc:/res/lanhouse-gamepad.svg"
                sourceSize.width: 22
                sourceSize.height: 22
                width: 22
                height: 22
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: "LanHouse"
                color: root.ink
                font.family: interBold.name
                font.pixelSize: 22
                font.letterSpacing: 0.4
            }
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 28

            Row {
                anchors.horizontalCenter: parent.horizontalCenter

                Text {
                    id: titleText
                    text: root.title
                    color: root.ink
                    font.family: interSemiBold.name
                    font.pixelSize: 28
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, root.width - 160)
                }

                Text {
                    text: "…"
                    color: root.ink
                    font: titleText.font
                    visible: root.title.length > 0

                    SequentialAnimation on opacity {
                        running: root.busy && root.visible
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.5; to: 1.0; duration: 700; easing.type: Easing.InOutQuad }
                        NumberAnimation { from: 1.0; to: 0.5; duration: 700; easing.type: Easing.InOutQuad }
                    }
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8

                Repeater {
                    model: root.stepNames.length

                    Column {
                        id: stepColumn
                        readonly property bool done: index < root.step
                        readonly property bool current: index === root.step
                        // A animação de "respirar" para no meio quando a etapa
                        // termina; sem isso o nome ficaria meio apagado.
                        onCurrentChanged: if (!current) labelRow.opacity = 1
                        width: root.stepWidths[index]
                        spacing: 10

                        // Movimento "B — bem forte" (canvas, aprovado 2026-10-17):
                        // na etapa atual a barra inteira brilha em violeta, a
                        // ponta amarela pulsa, o círculo solta uma onda e o
                        // nome "respira" - pra tela nunca parecer travada.
                        // Sem box-shadow em QML: o brilho é feito com camadas
                        // translúcidas que pulsam juntas.
                        Item {
                            width: parent.width
                            height: 6

                            Item {
                                id: trackGlow
                                anchors.fill: parent
                                visible: stepColumn.current

                                Repeater {
                                    model: [{ grow: 6, alpha: 0.30 }, { grow: 12, alpha: 0.16 }, { grow: 20, alpha: 0.07 }]
                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: trackGlow.width + modelData.grow * 2
                                        height: trackGlow.height + modelData.grow
                                        radius: height / 2
                                        color: "#7C40FF"
                                        opacity: modelData.alpha
                                    }
                                }

                                SequentialAnimation on opacity {
                                    running: stepColumn.current && root.busy && root.visible
                                    loops: Animation.Infinite
                                    NumberAnimation { from: 0; to: 1; duration: 600; easing.type: Easing.InOutSine }
                                    NumberAnimation { from: 1; to: 0; duration: 600; easing.type: Easing.InOutSine }
                                }
                            }

                            Rectangle {
                                anchors.fill: parent
                                radius: 3
                                color: stepColumn.done ? root.violet : root.trackColor
                                clip: true

                                Rectangle {
                                    visible: stepColumn.current
                                    height: parent.height
                                    radius: 3
                                    width: parent.width * root.shownProgress
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0.0; color: root.violet }
                                        GradientStop { position: 0.8; color: root.violet }
                                        GradientStop { position: 1.0; color: root.acid }
                                    }
                                }
                            }

                            // Ponta amarela pulsando no fim do preenchimento
                            Item {
                                id: tip
                                visible: stepColumn.current
                                width: 6
                                height: 6
                                x: parent.width * root.shownProgress - width / 2
                                y: 0

                                Item {
                                    id: tipGlow
                                    anchors.centerIn: parent
                                    width: 1
                                    height: 1
                                    opacity: 0.35
                                    scale: 0.6

                                    Repeater {
                                        model: [{ size: 12, alpha: 0.55 }, { size: 22, alpha: 0.28 }, { size: 34, alpha: 0.12 }]
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: modelData.size
                                            height: modelData.size
                                            radius: modelData.size / 2
                                            color: root.acid
                                            opacity: modelData.alpha
                                        }
                                    }

                                    ParallelAnimation {
                                        running: stepColumn.current && root.busy && root.visible
                                        loops: Animation.Infinite
                                        SequentialAnimation {
                                            NumberAnimation { target: tipGlow; property: "opacity"; from: 0.35; to: 1; duration: 450; easing.type: Easing.InOutSine }
                                            NumberAnimation { target: tipGlow; property: "opacity"; from: 1; to: 0.35; duration: 450; easing.type: Easing.InOutSine }
                                        }
                                        SequentialAnimation {
                                            NumberAnimation { target: tipGlow; property: "scale"; from: 0.6; to: 1.1; duration: 450; easing.type: Easing.InOutSine }
                                            NumberAnimation { target: tipGlow; property: "scale"; from: 1.1; to: 0.6; duration: 450; easing.type: Easing.InOutSine }
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 3
                                    color: root.acid
                                }
                            }
                        }

                        Row {
                            id: labelRow
                            spacing: 6

                            SequentialAnimation on opacity {
                                running: stepColumn.current && root.busy && root.visible
                                loops: Animation.Infinite
                                NumberAnimation { from: 1; to: 0.3; duration: 800; easing.type: Easing.InOutSine }
                                NumberAnimation { from: 0.3; to: 1; duration: 800; easing.type: Easing.InOutSine }
                            }

                            // Check (concluída) ou círculo (atual); desenhado em
                            // Canvas porque a Inter embutida não garante o glifo ✓.
                            Canvas {
                                width: 14
                                height: 14
                                anchors.verticalCenter: parent.verticalCenter
                                visible: stepColumn.done || stepColumn.current
                                property bool isDone: stepColumn.done
                                onIsDoneChanged: requestPaint()
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = root.acid
                                    ctx.lineWidth = 1.8
                                    ctx.lineCap = "round"
                                    ctx.lineJoin = "round"
                                    ctx.beginPath()
                                    if (isDone) {
                                        ctx.moveTo(3, 7.5)
                                        ctx.lineTo(5.5, 10)
                                        ctx.lineTo(11, 4.5)
                                    } else {
                                        ctx.arc(7, 7, 4, 0, 2 * Math.PI)
                                    }
                                    ctx.stroke()
                                }

                                // Onda que sai do círculo da etapa atual
                                Rectangle {
                                    id: ring
                                    visible: stepColumn.current
                                    x: 3
                                    y: 3
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: "transparent"
                                    border.color: root.acid
                                    border.width: 2.6
                                    opacity: 0

                                    ParallelAnimation {
                                        running: stepColumn.current && root.busy && root.visible
                                        loops: Animation.Infinite
                                        NumberAnimation { target: ring; property: "scale"; from: 1; to: 4.2; duration: 1000; easing.type: Easing.OutQuad }
                                        NumberAnimation { target: ring; property: "opacity"; from: 0.9; to: 0; duration: 1000; easing.type: Easing.OutQuad }
                                    }
                                }
                            }

                            Text {
                                text: root.stepNames[index]
                                color: stepColumn.current ? root.acid : (stepColumn.done ? root.inkSoft : root.muted)
                                font.family: interSemiBold.name
                                font.pixelSize: 13
                            }
                        }
                    }
                }
            }
        }
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 36
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(implicitWidth, parent.width - 64)
        text: root.hint
        textFormat: Text.StyledText
        color: root.muted
        font.family: interSemiBold.name
        font.weight: Font.Normal
        font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        visible: root.hint.length > 0
    }
}
