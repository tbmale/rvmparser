#include <cassert>
#include <cstdio>
#include <string>
#include <algorithm>
#include "ExportOFF.h"
#include "Store.h"
#include "LinAlgOps.h"

namespace {

  bool open_w(FILE** f, const char* path)
  {
    auto err = fopen_s(f, path, "w");
    if (err == 0) return true;

    char buf[1024];
    if (strerror_s(buf, sizeof(buf), err) != 0) {
      buf[0] = '\0';
    }
    fprintf(stderr, "Failed to open %s for writing: %s", path, buf);
    return false;
  }

  void wireBoundingBox(std::vector<float> &verts, unsigned& off_v, const BBox3f& bbox)
  {
    for (unsigned i = 0; i < 8; i++) {
      float px = (i & 1) ? bbox.min[0] : bbox.min[3];
      float py = (i & 2) ? bbox.min[1] : bbox.min[4];
      float pz = (i & 4) ? bbox.min[2] : bbox.min[5];
      verts.push_back(px);
    }
    off_v += 8;
  }

  void getMidpoint(Vec3f& p, Geometry* geo)
  {
    switch (geo->kind) {
    case Geometry::Kind::CircularTorus: {
      auto & ct = geo->circularTorus;
      auto c = std::cos(0.5f * ct.angle);
      auto s = std::sin(0.5f * ct.angle);
      p.x = ct.offset * c;
      p.y = ct.offset * s;
      p.z = 0.f;
      break;
    }
    default:
      p = Vec3f(0.f);
      break;
    }
    p = mul(geo->M_3x4, p);
  }
  
  bool same_mesh(CorkTriMesh m1,CorkTriMesh m2){
    if(m1.n_vertices!=m2.n_vertices || m1.n_triangles!=m2.n_triangles)
      return false;
    for(int i=0;i<m1.n_vertices;i++)
      if(m1.vertices[i]!=m2.vertices[i])
        return false;
    for(int i=0;i<m1.n_triangles;i++)
      if(m1.triangles[i]!=m2.triangles[i])
        return false;
    return true;
  }
  
  void fprintCTM(FILE* f,CorkTriMesh m){
    fprintf(f,"OFF\n%d %d 0\n",m.n_vertices,m.n_triangles);
    for(int i=0;i<m.n_vertices;i++)
      fprintf(f,"%f %f %f\n",m.vertices[3*i],m.vertices[3*i+1],m.vertices[3*i+2]);
    for(int i=0;i<m.n_triangles;i++)
      fprintf(f,"3 %d %d %d\n",m.triangles[3*i],m.triangles[3*i+1],m.triangles[3*i+2]);
  }

}



ExportOFF::~ExportOFF()
{
  if (out) {
    fclose(out);
  }
}

bool ExportOFF::open(const char* path_obj)
{
  if (!open_w(&out, path_obj)) return false;
  return true;
}


void ExportOFF::init(class Store& store)
{
  assert(out);
  this->store = &store;
  conn = store.conn;

  stack.accommodate(store.groupCountAllocated());

  char colorName[6];
  for (auto * line = store.getFirstDebugLine(); line != nullptr; line = line->next) {

    for (unsigned k = 0; k < 6; k++) {
      auto v = (line->color >> (4 * k)) & 0xf;
      if (v < 10) colorName[k] = '0' + v;
      else colorName[k] = 'a' + v - 10;
    }
    auto * name = store.strings.intern(&colorName[0], &colorName[6]);
    if (!definedColors.get(uint64_t(name))) {
      definedColors.insert(uint64_t(name), 1);
      auto r = (1.f / 255.f)*((line->color >> 16) & 0xFF);
      auto g = (1.f / 255.f)*((line->color >> 8) & 0xFF);
      auto b = (1.f / 255.f)*((line->color) & 0xFF);
    }

    verts.push_back(line->a[0]);
    verts.push_back(line->a[1]);
    verts.push_back(line->a[2]);
    verts.push_back(line->b[0]);
    verts.push_back(line->b[1]);
    verts.push_back(line->b[2]);
    off_v += 2;
  }
}

void ExportOFF::beginFile(Group* group)
{
  //fprintf(out, "OFF\n");
}

void ExportOFF::endFile() {
      FILE* m_out;
  CorkTriMesh r_mesh;
  printf("geometries:%d\n",c_mesh.size());
  r_mesh=c_mesh[0];
  if(c_mesh.size()==2){
    if(same_mesh(c_mesh[0],c_mesh[1]))
      r_mesh=c_mesh[0];
    else{
      CorkTriMesh o_mesh;
      computeUnion(r_mesh,c_mesh[1],&o_mesh);
      r_mesh=o_mesh;
    }
  }
  if(c_mesh.size()>2){
    for(int i=1;i<c_mesh.size();i++){
      CorkTriMesh o_mesh;
  char buffer[1024];
  printf("result verts:%d, tris:%d\n",r_mesh.n_vertices,r_mesh.n_triangles);
  printf("primitive[%d] verts:%d, tris:%d\n",i,c_mesh[i].n_vertices,c_mesh[i].n_triangles);
  // fprintCTM(stdout,r_mesh);
  // fprintCTM(stdout,c_mesh[i]);
      computeUnion(r_mesh,c_mesh[i],&o_mesh);
      
      free(r_mesh.vertices);
      free(r_mesh.triangles);
      
      r_mesh.n_vertices=o_mesh.n_vertices;
      r_mesh.vertices=(float*)malloc(sizeof(float)*o_mesh.n_vertices*3);
      // for(int i=0;i<r_mesh.n_vertices;i++)
        // r_mesh.vertices[i]=o_mesh.vertices[i];
      std::memcpy(r_mesh.vertices,o_mesh.vertices,sizeof(float)*o_mesh.n_vertices*3);
      
      r_mesh.n_triangles=o_mesh.n_triangles;
      r_mesh.triangles=(uint32_t*)malloc(sizeof(uint32_t)*o_mesh.n_triangles*3);
      // for(int i=0;i<r_mesh.n_triangles;i++)
        // r_mesh.triangles[i]=o_mesh.triangles[i];
      std::memcpy(r_mesh.triangles,o_mesh.triangles,sizeof(uint32_t)*o_mesh.n_triangles*3);

      o_mesh.n_vertices=0;
      o_mesh.n_triangles=0;
      free(o_mesh.vertices);
      free(o_mesh.triangles);
      
      sprintf(buffer,"+%d.off",i);
      if((m_out=fopen(buffer,"w"))!=nullptr) {
        fprintCTM(m_out,r_mesh);
        fclose(m_out);
      }
      
    }
  }
  
  fprintf(out,"OFF\n%d %d 0\n",verts.size() / 3,faces.size() / 3);
  for(unsigned i=0;i<verts.size();i+=3)
    fprintf(out,"%f %f %f\n",verts[i],verts[i+1],verts[i+2]);
  for(unsigned i=0;i<faces.size();i+=3)
    fprintf(out,"3 %d %d %d\n",faces[i],faces[i+1],faces[i+2]);
}

void ExportOFF::beginModel(Group* group)
{
  //printf( "# Model project=%s, name=%s\n", group->model.project, group->model.name);
}

void ExportOFF::endModel() { }

void ExportOFF::beginGroup(Group* group)
{
  //printf("%s\n",group->group.name);
  for (unsigned i = 0; i < 3; i++) curr_translation[i] = group->group.translation[i];

  stack[stack_p++] = group->group.name;

  if (groupBoundingBoxes && !isEmpty(group->group.bboxWorld)) {
    wireBoundingBox(verts, off_v, group->group.bboxWorld);
  }
}

void ExportOFF::EndGroup() {
  assert(stack_p);
  stack_p--;
}

void ExportOFF::geometry(struct Geometry* geometry)
{
  const auto & M = geometry->M_3x4;
  // printf("%d\n",geometry->kind);
  CorkTriMesh mesh;
  std::vector<float> c_verts;
  std::vector<uint32_t> c_tris;
  if (geometry->colorName == nullptr) {
    geometry->colorName = store->strings.intern("default");
  }

  if (!definedColors.get(uint64_t(geometry->colorName))) {
    definedColors.insert(uint64_t(geometry->colorName), 1);

    auto r = (1.f / 255.f)*((geometry->color >> 16) & 0xFF);
    auto g = (1.f / 255.f)*((geometry->color >> 8) & 0xFF);
    auto b = (1.f / 255.f)*((geometry->color) & 0xFF);
  }

  auto scale = 1.f;
  
  if (geometry->kind == Geometry::Kind::Line) {
    auto a = scale * mul(geometry->M_3x4, Vec3f(geometry->line.a, 0, 0));
    auto b = scale * mul(geometry->M_3x4, Vec3f(geometry->line.b, 0, 0));
    verts.push_back(a.x);
    verts.push_back(a.y);
    verts.push_back(a.z);
    verts.push_back(b.x);
    verts.push_back(b.y);
    verts.push_back(b.z);
    off_v += 2;
  }
  else {
    assert(geometry->triangulation);
    auto * tri = geometry->triangulation;

    if (tri->indices != 0) {
      //sprintf(buffer, "g\n");
      if (geometry->triangulation->error != 0.f) {
        fprintf(out, "# error=%f\n", geometry->triangulation->error);
      }
      for (size_t i = 0; i < 3 * tri->vertices_n; i += 3) {

        auto p = scale * mul(geometry->M_3x4, Vec3f(tri->vertices + i));
        auto n = normalize(mul(Mat3f(geometry->M_3x4.data), Vec3f(tri->normals + i)));

        verts.push_back(p.x);
        verts.push_back(p.y);
        verts.push_back(p.z);
        c_verts.push_back(p.x);
        c_verts.push_back(p.y);
        c_verts.push_back(p.z);
      }
      if (tri->texCoords) {
        for (size_t i = 0; i < tri->vertices_n; i++) {
          const Vec2f vt(tri->texCoords + 2 * i);
        }
      }
      else {
        for (size_t i = 0; i < tri->vertices_n; i++) {
          auto p = scale * mul(geometry->M_3x4, Vec3f(tri->vertices + 3*i));
        }

        for (size_t i = 0; i < 3 * tri->triangles_n; i += 3) {
          auto a = tri->indices[i + 0];
          auto b = tri->indices[i + 1];
          auto c = tri->indices[i + 2];
          faces.push_back(a + off_v - 1);
          faces.push_back(b + off_v - 1);
          faces.push_back(c + off_v - 1);
          c_tris.push_back(a);
          c_tris.push_back(b);
          c_tris.push_back(c);
        }
      }

      off_v += tri->vertices_n;
      off_n += tri->vertices_n;
      off_t += tri->vertices_n;
      mesh.n_vertices=c_verts.size()/3;
      mesh.n_triangles=c_tris.size()/3;
      mesh.vertices=(float*)malloc(sizeof(float)*mesh.n_vertices*3);
      for(int i=0;i<c_verts.size();i++){
        mesh.vertices[i]=c_verts[i];
      }
      mesh.triangles=(uint32_t*)malloc(sizeof(uint32_t)*mesh.n_triangles*3);
      for(int i=0;i<c_tris.size();i++)
        mesh.triangles[i]=c_tris[i];
      c_mesh.push_back(mesh);
      char buffer[1024];
      sprintf(buffer,"%d.off",c_mesh.size());
      if((oneout=fopen(buffer,"w"))!=nullptr) {
        fprintCTM(oneout,mesh);
        fclose(oneout);
      }
    }
  }

  //if (primitiveBoundingBoxes) {
  //  sprintf(buffer, "usemtl magenta\n");

  //  for (unsigned i = 0; i < 8; i++) {
  //    float px = (i & 1) ? geometry->bbox[0] : geometry->bbox[3];
  //    float py = (i & 2) ? geometry->bbox[1] : geometry->bbox[4];
  //    float pz = (i & 4) ? geometry->bbox[2] : geometry->bbox[5];

  //    float Px = M[0] * px + M[3] * py + M[6] * pz + M[9];
  //    float Py = M[1] * px + M[4] * py + M[7] * pz + M[10];
  //    float Pz = M[2] * px + M[5] * py + M[8] * pz + M[11];

  //    sprintf(buffer, "v %f %f %f\n", Px, Py, Pz);
  //  }
  //  sprintf(buffer, "l %d %d %d %d %d\n",
  //          off_v + 0, off_v + 1, off_v + 3, off_v + 2, off_v + 0);
  //  sprintf(buffer, "l %d %d %d %d %d\n",
  //          off_v + 4, off_v + 5, off_v + 7, off_v + 6, off_v + 4);
  //  sprintf(buffer, "l %d %d\n", off_v + 0, off_v + 4);
  //  sprintf(buffer, "l %d %d\n", off_v + 1, off_v + 5);
  //  sprintf(buffer, "l %d %d\n", off_v + 2, off_v + 6);
  //  sprintf(buffer, "l %d %d\n", off_v + 3, off_v + 7);
  //  off_v += 8;
  //}


  //for (unsigned k = 0; k < 6; k++) {
  //  auto other = geometry->conn_geo[k];
  //  if (geometry < other) {
  //    sprintf(buffer, "usemtl blue_line\n");
  //    float p[3];
  //    getMidpoint(p, geometry);
  //    sprintf(buffer, "v %f %f %f\n", p[0], p[1], p[2]);
  //    getMidpoint(p, other);
  //    sprintf(buffer, "v %f %f %f\n", p[0], p[1], p[2]);
  //    sprintf(buffer, "l %d %d\n", off_v, off_v + 1);

  //    off_v += 2;
  //  }
  //}

}
