#pragma once

#include "aim/proto/scenario.pb.h"

namespace aim {

Room GetDefaultSimpleRoom();
Room GetDefaultCylinderRoom();
Room GetDefaultBarrelRoom();

void DrawRoomEditorInputs(Room& room);

}  // namespace aim
