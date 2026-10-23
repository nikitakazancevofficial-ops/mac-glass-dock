pragma ComponentBehavior: Bound
import QtQuick
import MacGlassDock
VolumetricGlassSurface {
    required property DockPlatform platform
    blurAvailable: platform.blurAvailable
}
