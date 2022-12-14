// Copyright (c) 2022 CINN Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cinn/hlir/op/contrib/cast.h"

#include <gflags/gflags.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cinn/common/cas.h"
#include "cinn/common/common.h"
#include "cinn/common/context.h"
#include "cinn/common/macros.h"
#include "cinn/hlir/framework/node.h"
#include "cinn/hlir/framework/op.h"
#include "cinn/hlir/framework/op_strategy.h"
#include "cinn/hlir/pe/elementwise.h"
#include "cinn/hlir/pe/ir_schedule_pe.h"
#include "cinn/ir/ir.h"
#include "cinn/ir/ir_base.h"
#include "cinn/ir/ir_schedule.h"
#include "cinn/ir/tensor.h"
#include "cinn/lang/builtin.h"
#include "cinn/lang/compute.h"

DECLARE_bool(cinn_ir_schedule);

namespace cinn {
namespace hlir {
namespace op {

using common::CINNValue;
using common::CINNValuePack;

ir::Tensor Cast(const ir::Tensor &A, const Type &dtype, const std::string &name) {
  auto res = Compute(
      A->shape, [=](const std::vector<Expr> &indices) { return ir::Cast::Make(dtype, A(indices)); }, name);
  return res;
}

std::shared_ptr<framework::OpStrategy> StrategyForCast(const framework::NodeAttr &attrs,
                                                       const std::vector<ir::Tensor> &inputs,
                                                       const std::vector<Type> &out_type,
                                                       const std::vector<std::vector<int>> &output_shapes,
                                                       const Target &target) {
  framework::CINNCompute cast_compute([=](lang::Args args, lang::RetValue *ret) {
    CHECK(!args.empty()) << "The input arguments of Cast compute is empty! Please check.\n";
    CINNValuePack pack_args = args[0];
    CHECK_GE(pack_args.size(), 1U) << "at least 1 input tensors for Cast compute\n";
    Expr A = pack_args[0];
    CHECK(A.as_tensor());
    CHECK(!output_shapes.empty());
    auto tensor_A = A.as_tensor_ref();
    auto stages   = CreateStages({tensor_A});
    VLOG(3) << "A shape: " << utils::Join(tensor_A->shape, ", ")
            << ", output_shapes: " << utils::Join(output_shapes[0], ", ");
    std::string tensor_name = UniqName("Cast_out");
    if (FLAGS_cinn_ir_schedule) {
      CHECK_EQ(pack_args.size(), 2U);
      tensor_name = pack_args[1].operator std::string();
    }
    ir::Tensor out = Cast(tensor_A, out_type[0], tensor_name);
    std::vector<CINNValue> res;
    stages->InsertLazily(out);
    res.push_back(CINNValue(out));
    CHECK(!out_type.empty()) << "Output type of Cast is empty! Please check.\n";
    res.push_back(CINNValue(stages));
    *ret = CINNValuePack{res};
  });

  framework::CINNSchedule cast_schedule([=](lang::Args args, lang::RetValue *ret) {
    if (FLAGS_cinn_ir_schedule) {
      CHECK(!args.empty()) << "The input argument of cast_schedule is empty! Please check.\n";
      common::CINNValuePack arg_pack = args[0];
      std::vector<Expr> vec_ast;
      for (int i = 0; i < arg_pack.size(); i++) {
        if (arg_pack[i].is_expr()) {
          Expr temp = arg_pack[i];
          vec_ast.emplace_back(temp);
        }
      }
      CHECK(!vec_ast.empty());
      ir::ModuleExpr mod_expr(vec_ast);
      ir::IRSchedule ir_sch(mod_expr);
      ir_sch.MergeExprs();
      long prod_size = std::accumulate(output_shapes[0].begin(), output_shapes[0].end(), 1, std::multiplies<int>());
      if (prod_size > 1) {
        if (target.arch == Target::Arch::NVGPU) {
          pe::IRCudaScheduleInjective(ir_sch, output_shapes.front(), target);
        } else if (target.arch == Target::Arch::X86) {
          pe::IRScheduleInjectiveCPU(ir_sch, output_shapes.front(), target, true);
        }
      }
      std::vector<common::CINNValue> res{common::CINNValue(ir_sch.GetModule().GetExprs().at(0))};
      *ret = common::CINNValuePack{res};
    } else {
      CHECK(!args.empty()) << "The input argument of cast schedule is empty! Please check.\n";
      CINNValuePack arg_pack = args[0];
      Expr out               = arg_pack[0];
      CHECK(out.as_tensor());
      *ret = arg_pack;
    }
  });

  auto strategy = std::make_shared<framework::OpStrategy>();
  strategy->AddImpl(cast_compute, cast_schedule, "strategy.cast.x86", 1);

  return strategy;
}

std::vector<std::vector<int>> InferShapeForCast(const std::vector<std::vector<int>> &inputs_shape,
                                                const framework::AttrMapType &attrs) {
  CHECK_EQ(inputs_shape.size(), 1U) << "The input's shape size should be 1! Please check again.";
  std::vector<std::vector<int>> res{inputs_shape[0]};
  return res;
}

std::vector<Type> InferDtypeForCast(const std::vector<Type> &inputs_type, const framework::AttrMapType &attrs) {
  CHECK_EQ(inputs_type.size(), 1U) << "The input's type size should be 1! Please check again.";
  std::vector<Type> res;
  if (attrs.find("dtype") != attrs.end()) {
    auto dtype_str = absl::get<std::string>(attrs.at("dtype"));
    res.push_back(common::Str2Type(dtype_str));
  }
  CHECK(!res.empty()) << "The cast should have an attr named 'dtype'.";
  return res;
}

}  // namespace op
}  // namespace hlir
}  // namespace cinn

CINN_REGISTER_HELPER(cast_ops) {
  CINN_REGISTER_OP(cast)
      .describe("Cast.")
      .set_num_inputs(1)
      .set_num_outputs(1)
      .set_attr<cinn::hlir::framework::StrategyFunction>("CINNStrategy", cinn::hlir::op::StrategyForCast)
      .set_attr("infershape", MakeOpFunction(cinn::hlir::op::InferShapeForCast))
      .set_attr("inferdtype", MakeOpFunction(cinn::hlir::op::InferDtypeForCast))
      .set_support_level(4);

  return true;
}
