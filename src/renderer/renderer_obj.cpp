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

#include "renderer/renderer_obj.hpp"
#include "renderer/renderer.hpp"
#include "models/obj.hpp"
#include "sys/logging.hpp"

#include <cstring>

namespace pf
{
#define OGL_NAME (this->renderer.driver)

  /*! Append the newly loaded texture in the obj (XXX hack make something
   *  better later rather than using lock on the renderer obj. Better should
   *  be to make renderer obj immutable and replace it when all textures are
   *  loaded)
   */
  class TaskUpdateObjTexture : public Task
  {

  };

  /*! Load all textures for the given renderer obj */
  class TaskLoadObjTexture : public Task
  {
  public:
    TaskLoadObjTexture(TextureStreamer &streamer, const Obj *obj) :
      Task("TaskLoadObjTexture"), streamer(streamer)
    {
      if (obj->matNum) {
        this->texNum = obj->matNum;
        this->texName = PF_NEW_ARRAY(std::string, this->texNum);
        for (size_t i = 0; i < this->texNum; ++i) {
          const char *name = obj->mat[i].map_Kd;
          if (name == NULL || strlen(name) == 0)
            continue;
          this->texName[i] = name;
        }
      } else {
        this->texNum = 0;
        this->texName = NULL;
      }
    }
    ~TaskLoadObjTexture(void) {
      if (this->texName) PF_DELETE_ARRAY(this->texName);
    }
    virtual Task* run(void) {
      for (size_t i = 0; i < texNum; ++i) {
        if (texName[i].size() == 0) continue;
        Ref<Task> loading = streamer.createLoadTask(texName[i]);
        if (loading) {
          loading->ends(this);
          loading->scheduled();
        }
      }
      return NULL;
    }
    TextureStreamer &streamer;
    std::string *texName;
    size_t texNum;
  };

  RendererObj::RendererObj(Renderer &renderer_, const FileName &fileName) :
    renderer(renderer_),
    vertexArray(0), arrayBuffer(0), elementBuffer(0), grpNum(0)
  {
    PF_MSG_V("RendererObj: loading .obj file \"" << fileName.base() << "\"");
    Obj *obj = PF_NEW(Obj);
    bool isLoaded = false;
    for (size_t i = 0; i < defaultPathNum; ++i) {
      isLoaded = obj->load(FileName(defaultPath[i]) + fileName);
      if (isLoaded) break;
    }

    if (isLoaded) {
      PF_MSG_V("RendererObj: asynchronously loading textures");
      Ref<Task> texLoadingTask = PF_NEW(TaskLoadObjTexture, *renderer.streamer, obj);
      texLoadingTask->scheduled();

      // Map each material group to the texture name
      this->tex = PF_NEW_ARRAY(Ref<Texture2D>, obj->grpNum);
      this->texName = PF_NEW_ARRAY(std::string, obj->grpNum);
      for (size_t i = 0; i < obj->grpNum; ++i) {
        const int mat = obj->grp[i].m;
        const char *name = obj->mat[mat].map_Kd;
        if (name == NULL || strlen(name) == 0)
          this->tex[i] = renderer.defaultTex;
        else {
          TextureState state = renderer.streamer->getTextureState(name);
          if (state.value == TextureState::COMPLETE)
            this->tex[i] = state.tex;
          else
            this->tex[i] = renderer.defaultTex;
        }
      }

      // Compute each object bounding box and group of triangles
      PF_MSG_V("RendererObj: computing bounding boxes");
      this->grpNum = obj->grpNum;
      this->bbox = PF_NEW_ARRAY(BBox3f, obj->grpNum);
      this->grp = PF_NEW_ARRAY(RendererObj::Group, this->grpNum);
      for (size_t i = 0; i < obj->grpNum; ++i) {
        const size_t first = obj->grp[i].first, last = obj->grp[i].last;
        this->grp[i].first = first;
        this->grp[i].last = last;
        this->bbox[i] = BBox3f(empty);
        for (size_t j = first; j < last; ++j) {
          this->bbox[i].grow(obj->vert[obj->tri[j].v[0]].p);
          this->bbox[i].grow(obj->vert[obj->tri[j].v[1]].p);
          this->bbox[i].grow(obj->vert[obj->tri[j].v[2]].p);
        }
      }

      // Create the vertex buffer
      PF_MSG_V("RendererObj: creating OGL objects");
      const size_t vertexSize = obj->vertNum * sizeof(ObjVertex);
      R_CALL (GenBuffers, 1, &this->arrayBuffer);
      R_CALL (BindBuffer, GL_ARRAY_BUFFER, this->arrayBuffer);
      R_CALL (BufferData, GL_ARRAY_BUFFER, vertexSize, obj->vert, GL_STATIC_DRAW);
      R_CALL (BindBuffer, GL_ARRAY_BUFFER, 0);

      // Build the indices
      GLuint *indices = PF_NEW_ARRAY(GLuint, obj->triNum * 3);
      const size_t indexSize = sizeof(GLuint[3]) * obj->triNum;
      for (size_t from = 0, to = 0; from < obj->triNum; ++from, to += 3) {
        indices[to+0] = obj->tri[from].v[0];
        indices[to+1] = obj->tri[from].v[1];
        indices[to+2] = obj->tri[from].v[2];
      }
      R_CALL (GenBuffers, 1, &this->elementBuffer);
      R_CALL (BindBuffer, GL_ELEMENT_ARRAY_BUFFER, this->elementBuffer);
      R_CALL (BufferData, GL_ELEMENT_ARRAY_BUFFER, indexSize, indices, GL_STATIC_DRAW);
      R_CALL (BindBuffer, GL_ELEMENT_ARRAY_BUFFER, 0);
      PF_DELETE_ARRAY(indices);

      // Contain the vertex array
      R_CALL (GenVertexArrays, 1, &this->vertexArray);
      R_CALL (BindVertexArray, this->vertexArray);
      R_CALL (BindBuffer, GL_ARRAY_BUFFER, this->arrayBuffer);
      R_CALL (VertexAttribPointer, RendererDriver::ATTR_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(ObjVertex), NULL);
      R_CALL (VertexAttribPointer, RendererDriver::ATTR_TEXCOORD, 2, GL_FLOAT, GL_FALSE, sizeof(ObjVertex), (void*)offsetof(ObjVertex, t));
      R_CALL (BindBuffer, GL_ARRAY_BUFFER, 0);
      R_CALL (EnableVertexAttribArray, RendererDriver::ATTR_POSITION);
      R_CALL (EnableVertexAttribArray, RendererDriver::ATTR_TEXCOORD);
      R_CALL (BindVertexArray, 0);
    }

    // We do not need anymore the OBJ structure
    PF_DELETE(obj);
  }

  RendererObj::~RendererObj(void) {
    if (this->isValid()) {
      R_CALL (DeleteVertexArrays, 1, &this->vertexArray);
      R_CALL (DeleteBuffers, 1, &this->arrayBuffer);
      R_CALL (DeleteBuffers, 1, &this->elementBuffer);
      PF_DELETE_ARRAY(this->tex);
      PF_DELETE_ARRAY(this->texName);
      PF_DELETE_ARRAY(this->bbox);
      PF_DELETE_ARRAY(this->grp);
    }
  }

  void RendererObj::display(void) {
    Lock<MutexSys> lock(mutex);
    R_CALL (BindVertexArray, this->vertexArray);
    R_CALL (BindBuffer, GL_ELEMENT_ARRAY_BUFFER, this->elementBuffer);
    R_CALL (ActiveTexture, GL_TEXTURE0);
    for (size_t grp = 0; grp < this->grpNum; ++grp) {
      const uintptr_t offset = this->grp[grp].first * sizeof(int[3]);
      const GLuint n = this->grp[grp].last - this->grp[grp].first + 1;
      R_CALL (BindTexture, GL_TEXTURE_2D, this->tex[grp]->handle);
      R_CALL (DrawElements, GL_TRIANGLES, 3*n, GL_UNSIGNED_INT, (void*) offset);
    }
    R_CALL (BindVertexArray, 0);
    R_CALL (BindBuffer, GL_ELEMENT_ARRAY_BUFFER, 0);
  }

#undef OGL_NAME
}
