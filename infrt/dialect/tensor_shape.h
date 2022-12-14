#pragma once
#include <mlir/IR/Dialect.h>
#include <mlir/IR/OpDefinition.h>
#include <mlir/Interfaces/SideEffectInterfaces.h>

namespace infrt::ts {

class ShapeType : public mlir::Type::TypeBase<ShapeType, mlir::Type, mlir::TypeStorage> {
 public:
  using Base::Base;
};

class PartialShapeType : public mlir::Type::TypeBase<PartialShapeType, mlir::Type, mlir::TypeStorage> {
 public:
  using Base::Base;
};

using namespace mlir;  // NOLINT
#define GET_OP_CLASSES
#include "infrt/dialect/tensor_shape.hpp.inc"
#include "infrt/dialect/tensor_shape_dialect.hpp.inc"

}  // namespace infrt::ts
