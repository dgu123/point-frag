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

#include "sys/script.hpp"
#include "sys/command.hpp"
#include "sys/logging.hpp"
#include "sys/tasking.hpp"
#include "utest/utest.hpp"

using namespace pf;

VARI(coucou, 0, 2, 3, "coucou");
VARS(player0, "ben", "player name");

#define _RUN_SCRIPT(STR, RUN_MODE) do {\
  ScriptStatus status;\
  scriptSystem->RUN_MODE(STR, status);\
  if (!status.success) PF_ERROR(status.msg);\
} while (0)
#define RUN(STR) _RUN_SCRIPT(STR,run)
#define RUN_NON_PROTECTED(STR) _RUN_SCRIPT(STR,runNonProtected)

void utest_lua(void)
{
  ScriptSystem *scriptSystem = LuaScriptSystemCreate();
  ScriptStatus status;
  scriptSystem->run("local x = 0", status);
  CommandSystemStart(*scriptSystem);

  // Run some code. This may modify console variables
  RUN("cv.coucou = 1");
  RUN_NON_PROTECTED("print(pf.cv.coucou)");
  RUN("cv.player0 = \"hop\"");
  RUN_NON_PROTECTED("print(pf.cv.player0)");
  if (coucou() == 1) PF_MSG("coucou is equal to 1");

  CommandSystemEnd();
  PF_DELETE(scriptSystem);
}

UTEST_REGISTER(utest_lua)

