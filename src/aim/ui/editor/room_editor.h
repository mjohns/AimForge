#pragma once

#include "aim/proto/scenario.pb.h"

namespace aim {

Room GetDefaultSimpleRoom();
Room GetDefaultCylinderRoom();
Room GetDefaultBarrelRoom();

struct CameraUpdates {
  float delta_pitch = 0;
  float delta_yaw = 0;
};

void DrawRoomEditorInputs(Room& room, CameraUpdates* camera_updates);

}  // namespace aim
