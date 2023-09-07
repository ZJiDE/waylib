// Copyright (C) 2023 JiDe Zhang <zccrs@live.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import Waylib.Server
import Tinywl

Item {
    id: root

    required property SurfaceItem surface
    required property ToplevelSurface waylandSurface
    required property ListModel dockModel
    required property DynamicCreatorComponent creator

    property OutputPositioner output
    property CoordMapper outputCoordMapper
    property bool mapped: waylandSurface.surface && waylandSurface.surface.mapped && waylandSurface.WaylandSocket.rootSocket.enabled
    property bool pendingDestroy: false

    Binding {
        target: surface
        property: "states"
        restoreMode: Binding.RestoreNone
        value: State {
            name: "maximize"
            when: waylandSurface && waylandSurface.isMaximized && outputCoordMapper
            PropertyChanges {
                restoreEntryValues: true
                target: root.surface

                x: outputCoordMapper.x
                y: outputCoordMapper.y + output.topMargin
                width: outputCoordMapper.width
                height: outputCoordMapper.height - output.topMargin
            }
        }
    }

    Binding {
        target: surface
        property: "transitions"
        restoreMode: Binding.RestoreNone
        value: Transition {
            id: stateTransition

            NumberAnimation {
                properties: "x,y,width,height"
                duration: 100
            }
        }
    }

    Binding {
        target: surface
        property: "resizeMode"
        value: {
            if (!surface.effectiveVisible)
                return SurfaceItem.ManualResize
            if (Helper.resizingItem === surface
                    || stateTransition.running
                    || waylandSurface.isMaximized)
                return SurfaceItem.SizeToSurface
            return SurfaceItem.SizeFromSurface
        }
        restoreMode: Binding.RestoreNone
    }

    OpacityAnimator {
        id: hideAnimation
        duration: 300
        target: surface
        from: 1
        to: 0

        onStopped: {
            surface.visible = false
            if (pendingDestroy)
                creator.destroyObject(surface)
        }
    }

    Connections {
        target: surface

        function onEffectiveVisibleChanged() {
            if (surface.effectiveVisible) {
                console.assert(surface.resizeMode !== SurfaceItem.ManualResize,
                               "The surface's resizeMode Shouldn't is ManualResize")
                // Apply the WSurfaceItem's size to wl_surface
                surface.resize(SurfaceItem.SizeToSurface)

                if (waylandSurface && waylandSurface.isActivated)
                    surface.forceActiveFocus()
            } else {
                Helper.cancelMoveResize(surface)
            }
        }
    }

    onMappedChanged: {
        if (pendingDestroy)
            return

        // When Socket is enabled and mapped becomes false, set visible
        // after hideAnimation complete， Otherwise set visible directly.
        if (mapped) {
            if (waylandSurface.isMinimized) {
                surface.visible = false;
                dockModel.append({ source: surface });
            } else {
                surface.visible = true;

                if (surface.effectiveVisible)
                    Helper.activatedSurface = waylandSurface
            }
        } else { // if not mapped
            if (waylandSurface.isMinimized) {
                // mapped becomes false but not pendingDestroy
                dockModel.removeSurface(surface)
            }

            if (!waylandSurface.WaylandSocket.rootSocket.enabled) {
                surface.visible = false;
            } else {
                // do animation for window close
                hideAnimation.start()
            }
        }
    }

    function doDestroy() {
        pendingDestroy = true

        if (!surface.visible || !hideAnimation.running) {
            if (waylandSurface.isMinimized) {
                // mapped becomes false and pendingDestroy
                dockModel.removeSurface(surface)
            }

            creator.destroyObject(surface)
            return
        }

        // unbind some properties
        mapped = surface.visible
        surface.states = null
        surface.transitions = null
    }

    function getPrimaryOutputPositioner() {
        let output = waylandSurface.surface.primaryOutput
        if (!output)
            return null
        return output.OutputPositioner.positioner
    }

    function updateOutputCoordMapper() {
        let output = getPrimaryOutputPositioner()
        if (!output)
            return

        root.output = output
        root.outputCoordMapper = surface.CoordMapper.helper.get(output)
    }

    function cancelMinimize () {
        if (waylandSurface.isResizeing)
            return

        if (!waylandSurface.isMinimized)
            return

        Helper.activatedSurface = waylandSurface

        surface.visible = true;

        dockModel.removeSurface(surface)
        waylandSurface.setMinimize(false)
    }

    Connections {
        target: waylandSurface
        ignoreUnknownSignals: true

        function onActivateChanged() {
            if (waylandSurface.isActivated) {
                WaylibHelper.itemStackToTop(surface)
                if (surface.effectiveVisible)
                    surface.forceActiveFocus()
            } else {
                surface.focus = false
            }
        }

        function onRequestMove(seat, serial) {
            if (waylandSurface.isMaximized)
                return

            if (!surface.effectiveVisible)
                return

            Helper.startMove(waylandSurface, surface, seat, serial)
        }

        function onRequestResize(seat, edges, serial) {
            if (waylandSurface.isMaximized)
                return

            if (!surface.effectiveVisible)
                return

            Helper.startResize(waylandSurface, surface, seat, edges, serial)
        }

        function rectMarginsRemoved(rect, left, top, right, bottom) {
            rect.x += left
            rect.y += top
            rect.width -= (left + right)
            rect.height -= (top + bottom)
            return rect
        }

        function onRequestMaximize() {
            if (waylandSurface.isResizeing)
                return

            if (waylandSurface.isMaximized)
                return

            if (!surface.effectiveVisible)
                return

            updateOutputCoordMapper()
            waylandSurface.setMaximize(true)
        }

        function onRequestCancelMaximize() {
            if (waylandSurface.isResizeing)
                return

            if (!waylandSurface.isMaximized)
                return

            if (!surface.effectiveVisible)
                return

            waylandSurface.setMaximize(false)
        }

        function onRequestMinimize() {
            if (waylandSurface.isResizeing)
                return

            if (waylandSurface.isMinimized)
                return

            if (!surface.effectiveVisible)
                return

            surface.focus = false;
            if (Helper.activeSurface === surface)
                Helper.activeSurface = null;

            surface.visible = false;
            dockModel.append({ source: surface });
            waylandSurface.setMinimize(true)
        }

        function onRequestCancelMinimize() {
            if (!surface.effectiveVisible)
                return

            cancelMinimize();
        }

        // for xwayland surface
        function onRequestConfigure(geometry, flags) {
            // Disable manager the window's position by client for a normal window
            if (!(waylandSurface.windowTypes & XWaylandSurface.NET_WM_WINDOW_TYPE_NORMAL)) {
                const pos = surface.parent.mapFromGlobal(geometry.x, geometry.y)
                if (flags & XWaylandSurface.XCB_CONFIG_WINDOW_X)
                    surface.x = pos.x
                if (flags & XWaylandSurface.XCB_CONFIG_WINDOW_Y)
                    surface.y = pos.y
            }

            if (flags & XWaylandSurface.XCB_CONFIG_WINDOW_WIDTH)
                surface.width = geometry.width
            if (flags & XWaylandSurface.XCB_CONFIG_WINDOW_HEIGHT)
                surface.height = geometry.height
        }

        function onGeometryChanged() {
            if (!waylandSurface.bypassManager)
                return
            let geo = waylandSurface.geometry
            let pos = surface.parent.mapFromGlobal(geo.x, geo.y)

            surface.x = pos.x
            surface.y = pos.y
            surface.width = pos.width
            surface.height = pos.height
        }
    }

    Component.onCompleted: {
        if (waylandSurface.isMaximized) {
            updateOutputCoordMapper()
        }
    }
}
