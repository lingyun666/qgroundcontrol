/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick

import QGroundControl.FlightDisplay

// Active按钮实现
FlyViewBottomToolStripButton {
    buttonId:       "activeButton"
    iconSource:     "/res/active.svg"
    buttonText:     qsTr("Takeoff")
} 