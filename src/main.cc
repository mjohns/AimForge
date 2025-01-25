#include "application.h"
#include "model.h"
#include "flatbuffers/flatbuffers.h"
#include "replay_generated.h"

int main(int, char**) {
  auto app = aim::Application::Create();

  flatbuffers::FlatBufferBuilder builder;
  auto replay = CreateReplayDirect(builder, "Abominable Snowman", 100);
  builder.Finish(replay);

  aim::Scenario s;
  s.Run(app.get());


  return 0;
}
