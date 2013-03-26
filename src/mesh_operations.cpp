/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2008, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Ioan Sucan */

#include "geometric_shapes/mesh_operations.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <set>
#include <float.h>

#include <console_bridge/console.h>
#include <resource_retriever/retriever.h>

#if defined(IS_ASSIMP3)
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#else
#include <assimp/aiScene.h>
#include <assimp/assimp.hpp>
#include <assimp/aiPostProcess.h>
#endif

#include <Eigen/Geometry>

namespace shapes
{

namespace detail
{

namespace
{

/// Local representation of a vertex that knows its position in an array (used for sorting)
struct LocalVertexType
{
  LocalVertexType() : x(0.0), y(0.0), z(0.0)
  {
  }
  
  LocalVertexType(const Eigen::Vector3d &v) : x(v.x()), y(v.y()), z(v.z())
  {
  }
  
  double x,y,z;
  unsigned int index;
};

/// Sorting operator by point value
struct ltLocalVertexValue
{
  bool operator()(const LocalVertexType &p1, const LocalVertexType &p2) const
  {
    if (p1.x < p2.x)
      return true;
    if (p1.x > p2.x)
      return false;
    if (p1.y < p2.y)
      return true;
    if (p1.y > p2.y)
      return false;
    if (p1.z < p2.z)
      return true;
    return false;
  }
};

/// Sorting operator by point index
struct ltLocalVertexIndex
{
  bool operator()(const LocalVertexType &p1, const LocalVertexType &p2) const
  {
    return p1.index < p2.index;
  }
};

}
}

Mesh* createMeshFromVertices(const EigenSTL::vector_Vector3d &vertices, const std::vector<unsigned int> &triangles)
{
  unsigned int nt = triangles.size() / 3;
  Mesh *mesh = new Mesh(vertices.size(), nt);
  for (unsigned int i = 0 ; i < vertices.size() ; ++i)
  {
    mesh->vertices[3 * i    ] = vertices[i].x();
    mesh->vertices[3 * i + 1] = vertices[i].y();
    mesh->vertices[3 * i + 2] = vertices[i].z();
  }

  std::copy(triangles.begin(), triangles.end(), mesh->triangles);
  mesh->computeTriangleNormals();
  mesh->computeVertexNormals();
  
  return mesh;
}

Mesh* createMeshFromVertices(const EigenSTL::vector_Vector3d &source)
{
  if (source.size() < 3)
    return NULL;
  
  if (source.size() % 3 != 0)
    logError("The number of vertices to construct a mesh from is not divisible by 3. Probably constructed triangles will not make sense.");
  
  std::set<detail::LocalVertexType, detail::ltLocalVertexValue> vertices;
  std::vector<unsigned int> triangles;
  
  unsigned int n = source.size() / 3;
  for (unsigned int i = 0 ; i < n ; ++i)
  {
    // check if we have new vertices
    unsigned int i3 = i * 3;
    detail::LocalVertexType vt1(source[i3]);
    std::set<detail::LocalVertexType, detail::ltLocalVertexValue>::iterator p1 = vertices.find(vt1);
    if (p1 == vertices.end())
    {
      vt1.index = vertices.size();
      vertices.insert(vt1);
    }
    else
      vt1.index = p1->index;
    triangles.push_back(vt1.index);
    
    detail::LocalVertexType vt2(source[++i3]);
    std::set<detail::LocalVertexType, detail::ltLocalVertexValue>::iterator p2 = vertices.find(vt2);
    if (p2 == vertices.end())
    {
      vt2.index = vertices.size();
      vertices.insert(vt2);
    }
    else
      vt2.index = p2->index;
    triangles.push_back(vt2.index);
    
    detail::LocalVertexType vt3(source[++i3]);
    std::set<detail::LocalVertexType, detail::ltLocalVertexValue>::iterator p3 = vertices.find(vt3);
    if (p3 == vertices.end())
    {
      vt3.index = vertices.size();
      vertices.insert(vt3);
    }
    else
      vt3.index = p3->index;
    
    triangles.push_back(vt3.index);
  }

  // sort our vertices
  std::vector<detail::LocalVertexType> vt;
  vt.insert(vt.end(), vertices.begin(), vertices.end());
  std::sort(vt.begin(), vt.end(), detail::ltLocalVertexIndex());

  // copy the data to a mesh structure
  unsigned int nt = triangles.size() / 3;

  Mesh *mesh = new Mesh(vt.size(), nt);
  for (unsigned int i = 0 ; i < vt.size() ; ++i)
  {    
    unsigned int i3 = i * 3;
    mesh->vertices[i3    ] = vt[i].x;
    mesh->vertices[i3 + 1] = vt[i].y;
    mesh->vertices[i3 + 2] = vt[i].z;
  }

  std::copy(triangles.begin(), triangles.end(), mesh->triangles);
  mesh->computeTriangleNormals();
  mesh->computeVertexNormals();
  
  return mesh;
}

Mesh* createMeshFromResource(const std::string& resource)
{
  static const Eigen::Vector3d one(1.0, 1.0, 1.0);
  return createMeshFromResource(resource, one);
}

Mesh* createMeshFromBinary(const char* buffer, std::size_t size,
                           const std::string &assimp_hint)
{
  static const Eigen::Vector3d one(1.0, 1.0, 1.0);
  return createMeshFromBinary(buffer, size, one, assimp_hint);
}

Mesh* createMeshFromBinary(const char *buffer, std::size_t size, const Eigen::Vector3d &scale,
                           const std::string &assimp_hint)
{
  if (!buffer || size < 1)
  {
    logWarn("Cannot construct mesh from empty binary buffer");
    return NULL;
  }
  
  // try to get a file extension
  std::string hint;
  std::size_t pos = assimp_hint.find_last_of(".");
  if (pos != std::string::npos)
  {
    hint = assimp_hint.substr(pos + 1);
    std::transform(hint.begin(), hint.end(), hint.begin(), ::tolower);
    if (hint.find("stl") != std::string::npos)
      hint = "stl";
  }
  
  // Create an instance of the Importer class
  Assimp::Importer importer;
  

  // And have it read the given file with some postprocessing
  const aiScene* scene = importer.ReadFileFromMemory(reinterpret_cast<const void*>(buffer), size,
                                                     aiProcess_Triangulate            |
                                                     aiProcess_JoinIdenticalVertices  |
                                                     aiProcess_SortByPType            |
                                                     aiProcess_OptimizeGraph          |
                                                     aiProcess_OptimizeMeshes, assimp_hint.c_str());
  if (scene)
    return createMeshFromAsset(scene, scale, assimp_hint);
  else
    return NULL;
}

Mesh* createMeshFromResource(const std::string& resource, const Eigen::Vector3d &scale)
{
  resource_retriever::Retriever retriever;
  resource_retriever::MemoryResource res;
  try
  {
    res = retriever.get(resource);
  } 
  catch (resource_retriever::Exception& e)
  {
    logError("%s", e.what());
    return NULL;
  }

  if (res.size == 0)
  {
    logWarn("Retrieved empty mesh for resource '%s'", resource.c_str());
    return NULL;
  }
  
  Mesh *m = createMeshFromBinary(reinterpret_cast<const char*>(res.data.get()), res.size, scale, resource);
  if (!m)
    logWarn("Assimp reports no scene in %s", resource.c_str());
  return m;
}

namespace
{
void extractMeshData(const aiScene *scene, const aiNode *node, const aiMatrix4x4 &parent_transform, const Eigen::Vector3d &scale,
                     EigenSTL::vector_Vector3d &vertices, std::vector<unsigned int> &triangles)
{
  aiMatrix4x4 transform = parent_transform;
  transform *= node->mTransformation;
  for (unsigned int j = 0 ; j < node->mNumMeshes; ++j)
  {
    const aiMesh* a = scene->mMeshes[node->mMeshes[j]];   
    unsigned int offset = vertices.size();    
    for (unsigned int i = 0 ; i < a->mNumVertices ; ++i)
    {
      aiVector3D v = transform * a->mVertices[i];
      vertices.push_back(Eigen::Vector3d(v.x * scale.x(), v.y * scale.y(), v.z * scale.z()));
    }
    for (unsigned int i = 0 ; i < a->mNumFaces ; ++i)
      if (a->mFaces[i].mNumIndices == 3)
      {
        triangles.push_back(offset + a->mFaces[i].mIndices[0]);
        triangles.push_back(offset + a->mFaces[i].mIndices[1]);
        triangles.push_back(offset + a->mFaces[i].mIndices[2]);
      }
  }
  
  for (unsigned int n = 0; n < node->mNumChildren; ++n)
    extractMeshData(scene, node->mChildren[n], transform, scale, vertices, triangles);
}
}

Mesh* createMeshFromAsset(const aiScene* scene, const std::string &resource_name)
{
  static const Eigen::Vector3d one(1.0, 1.0, 1.0);
  return createMeshFromAsset(scene, one, resource_name);
}

Mesh* createMeshFromAsset(const aiScene* scene, const Eigen::Vector3d &scale, const std::string &resource_name)
{
  if (!scene->HasMeshes())
  {
    logWarn("Assimp reports scene in %s has no meshes", resource_name.c_str());
    return NULL;
  }
  EigenSTL::vector_Vector3d vertices;
  std::vector<unsigned int> triangles;
  extractMeshData(scene, scene->mRootNode, aiMatrix4x4(), scale, vertices, triangles);
  if (vertices.empty())
  {
    logWarn("There are no vertices in the scene %s", resource_name.c_str());
    return NULL;
  }
  if (triangles.empty())
  {
    logWarn("There are no triangles in the scene %s", resource_name.c_str());
    return NULL;
  }
  
  return createMeshFromVertices(vertices, triangles);
}

Mesh* createMeshFromShape(const Box &box)
{
  double x = box.size[0] / 2.0;
  double y = box.size[1] / 2.0;
  double z = box.size[2] / 2.0;
  
  // define vertices of box mesh
  Mesh *result = new Mesh(8, 12);
  result->vertices[0] = -x;
  result->vertices[1] = -y;
  result->vertices[2] = -z;
  
  result->vertices[3] = x;
  result->vertices[4] = -y;
  result->vertices[5] = -z;
  
  result->vertices[6] = x;
  result->vertices[7] = -y;
  result->vertices[8] = z;
  
  result->vertices[9] = -x;
  result->vertices[10] = -y;
  result->vertices[11] = z;
  
  result->vertices[12] = -x;
  result->vertices[13] = y;
  result->vertices[14] = z;
  
  result->vertices[15] = -x;
  result->vertices[16] = y;
  result->vertices[17] = -z;
  
  result->vertices[18] = x;
  result->vertices[19] = y;
  result->vertices[20] = z;
  
  result->vertices[21] = x;
  result->vertices[22] = y;
  result->vertices[23] = -z;
  
  static const unsigned int tri[] = {0, 1, 2,
                                     2, 3, 0,
                                     4, 3, 2,
                                     2, 6, 4,
                                     7, 6, 2,
                                     2, 1, 7,
                                     3, 4, 5,
                                     5, 0, 3,
                                     0, 5, 7,
                                     7, 1, 0,
                                     7, 5, 4,
                                     4, 6, 7};  
  memcpy(result->triangles, tri, sizeof(unsigned int) * 36);
  result->computeTriangleNormals();
  result->computeVertexNormals();
  return result;
}

}
