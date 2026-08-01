#include "controlPanel_internal.h"

void renderSystemPage(void) {
    igAlignTextToFramePadding();
    igText("Version: %s", RTS_VERSION);
}
