#pragma once
#include <cstdio>
#include <cstring>

#include "Common.h"
#include "StoreVisitor.h"
#include "LinAlg.h"
#include "cork.h"

class ExportOFF : public StoreVisitor
{
public:
  bool groupBoundingBoxes = false;

  ~ExportOFF();

  bool open(const char* path_obj);

  void init(class Store& store) override;

  void beginFile(Group* group) override;

  void endFile() override;

  void beginModel(Group* group) override;

  void endModel() override;

  void beginGroup(struct Group* group) override;

  void EndGroup() override;

  void geometry(struct Geometry* geometry) override;

private:
  FILE* out = nullptr;
  FILE* oneout = nullptr;
  std::vector<CorkTriMesh> c_mesh;
  std::vector<float> verts;
  std::vector<uint32_t> faces;
  Map definedColors;
  Store* store = nullptr;
  Buffer<const char*> stack;
  unsigned stack_p = 0;
  unsigned off_v = 1;
  unsigned off_n = 1;
  unsigned off_t = 1;
  struct Connectivity* conn = nullptr;
  float curr_translation[3] = { 0,0,0 };

  bool anchors = false;
  bool primitiveBoundingBoxes = false;
  bool compositeBoundingBoxes = false;
};