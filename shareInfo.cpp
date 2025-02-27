#include "shareInfo.h"
//==================//

State_Overlay shareInfo;

State_Overlay::State_Overlay() : isOverlayVisible(false), isRunning(true), isDragging(false) {
    selected = { -1, -1, -1, -1 };
}
