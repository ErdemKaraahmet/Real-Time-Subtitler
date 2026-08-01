#include "controlPanel_internal.h"

void renderSystemPage(void) {
    igAlignTextToFramePadding();
    igText("Version: %s", RTS_VERSION);
    igAlignTextToFramePadding();
    igText("Open Control Panel on Startup");
    igSameLine(270.0f, 0.0f);
    igCheckbox("##open_control_panel_on_startup", &uiConfig.open_control_panel_on_startup);
}
