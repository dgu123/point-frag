// ======================================================================== //
// Copyright (C) 2011 Benjamin Segovia                                      //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#ifndef __PF_GAME_FRAME_HPP__
#define __PF_GAME_FRAME_HPP__

#include "sys/tasking.hpp"

namespace pf
{
  class FPSCamera;
  class InputControl;

  /*! Stores the data required for the frame */
  class GameFrame : public RefCount, public NonCopyable
  {
  public:
    /*! Used to create the next frame */
    GameFrame(GameFrame &previous);
    GameFrame(int w, int h);
    Ref<FPSCamera> cam;     //!< Camera for this frame
    Ref<InputControl> event;//!< Input as captured for this frame
    PF_CLASS(GameFrame);
  };

  /*! Responsible to handle the complete frame */
  class TaskGameFrame : public Task
  {
  public:
    /*! GameFrames are chained one after other */
    TaskGameFrame(GameFrame &previous);
    /*! Will basically spawn all the other tasks */
    virtual Task *run(void);
    Ref<GameFrame> previous;
    PF_CLASS(TaskGameFrame);
  };
}

#endif /* __PF_GAME_FRAME_HPP__ */

