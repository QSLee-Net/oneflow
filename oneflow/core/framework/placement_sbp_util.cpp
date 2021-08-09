/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include <tuple>
#include "oneflow/core/framework/placement_sbp_util.h"
#include "oneflow/core/framework/tensor_meta.h"
#include "oneflow/core/common/shape.h"
#include "oneflow/core/common/util.h"
#include "oneflow/core/job/parallel_desc.h"
#include "oneflow/core/job/sbp_parallel.cfg.h"
#include "oneflow/core/common/decorator.h"
#include "oneflow/core/common/optional.h"
#include "oneflow/core/common/util.h"
#include "oneflow/core/common/container_util.h"
#include "oneflow/core/rpc/include/global_process_ctx.h"

namespace oneflow {

namespace private_details {

namespace {

using IndexVector = DimVector;
using StrideVector = DimVector;

void GetStrideVector(const Shape& shape, StrideVector* strides) {
  strides->resize(shape.NumAxes());
  for (int i = 0; i < shape.NumAxes(); ++i) { strides->at(i) = shape.Count(i + 1); }
}

Maybe<void> GetIndexesFromOffset(const StrideVector& strides, int64_t offset,
                                 IndexVector* indexes) {
  indexes->resize(strides.size());
  for (int i = 0; i < strides.size(); ++i) {
    indexes->at(i) = offset / strides.at(i);
    offset = offset % strides.at(i);
  }
  CHECK_EQ_OR_RETURN(offset, 0);
  return Maybe<void>::Ok();
}

Maybe<void> GetOffsetFromIndexes(const StrideVector& strides, const IndexVector& indexes,
                                 int64_t* offset) {
  CHECK_EQ_OR_RETURN(strides.size(), indexes.size());
  *offset = 0;
  for (int i = 0; i < strides.size(); ++i) { *offset += indexes.at(i) * strides.at(i); }
  return Maybe<void>::Ok();
}

Maybe<void> GetSelectedIndex2OriginIndex(
    const IndexVector& indexes, const std::vector<int>& axis2is_selected,
    std::function<void(const DimVector&, DimVector*)>* SelectedIndex2OriginIndex) {
  CHECK_EQ_OR_RETURN(axis2is_selected.size(), indexes.size());
  *SelectedIndex2OriginIndex = [=](const DimVector& broadcast, DimVector* origin) {
    origin->resize(indexes.size());
    for (int i = 0; i < indexes.size(); ++i) {
      origin->at(i) = axis2is_selected.at(i) ? broadcast.at(i) : indexes.at(i);
    }
  };
  return Maybe<void>::Ok();
}

Maybe<const Shape> GetSelectedShape(const Shape& hierarchy_shape,
                                    const std::vector<int>& axis2is_selected) {
  CHECK_EQ_OR_RETURN(hierarchy_shape.NumAxes(), axis2is_selected.size());
  DimVector dim_vec = hierarchy_shape.dim_vec();
  for (int i = 0; i < axis2is_selected.size(); ++i) {
    if (!axis2is_selected.at(i)) { dim_vec.at(i) = 1; }
  }
  return std::make_shared<const Shape>(dim_vec);
}

Maybe<Symbol<std::vector<int>>> CalcAxis2IsBroadcast(
    Symbol<cfg::ParallelDistribution> parallel_distribution) {
  std::vector<int> axis2is_selected(parallel_distribution->sbp_parallel_size());
  for (int i = 0; i < axis2is_selected.size(); ++i) {
    axis2is_selected.at(i) = parallel_distribution->sbp_parallel(i).has_broadcast_parallel();
  }
  return SymbolOf(axis2is_selected);
}

static auto* GetAxis2IsBroadcast = DECORATE(&CalcAxis2IsBroadcast, ThreadLocal);

Maybe<Symbol<ParallelDesc>> CalcSelectedSubParallelDesc(Symbol<ParallelDesc> parallel_desc,
                                                        Symbol<std::vector<int>> axis2is_selected,
                                                        int64_t parallel_id) {
  const auto& hierarchy_shape = *parallel_desc->hierarchy();
  const auto& broadcast_parallel_ids =
      JUST(GetSelectedParallelIds(hierarchy_shape, *axis2is_selected, parallel_id));
  ParallelConf parallel_conf;
  parallel_conf.set_device_tag(parallel_desc->device_tag());
  bool found_parallel_id = false;
  for (int64_t i : *broadcast_parallel_ids) {
    found_parallel_id = found_parallel_id || (i == parallel_id);
    int64_t machine_id = JUST(parallel_desc->MachineId4ParallelId(i));
    int64_t device_id = JUST(parallel_desc->DeviceId4ParallelId(i));
    parallel_conf.add_device_name(std::string("@") + std::to_string(machine_id) + ":"
                                  + std::to_string(device_id));
  }
  CHECK_OR_RETURN(found_parallel_id);
  return SymbolOf(ParallelDesc(parallel_conf));
}

static auto* GetSelectedSubParallelDesc = DECORATE(&CalcSelectedSubParallelDesc, ThreadLocal);

}  // namespace

Maybe<std::vector<int64_t>> GetSelectedParallelIds(const Shape& hierarchy_shape,
                                                   const std::vector<int>& axis2is_selected,
                                                   int64_t parallel_id) {
  CHECK_EQ_OR_RETURN(hierarchy_shape.NumAxes(), axis2is_selected.size());
  StrideVector hierarchy_strides{};
  GetStrideVector(hierarchy_shape, &hierarchy_strides);
  IndexVector indexes{};
  JUST(GetIndexesFromOffset(hierarchy_strides, parallel_id, &indexes));
  std::function<void(const DimVector&, DimVector*)> SelectedIndex2OriginIndex;
  JUST(GetSelectedIndex2OriginIndex(indexes, axis2is_selected, &SelectedIndex2OriginIndex));
  const auto& broadcast_shape = JUST(GetSelectedShape(hierarchy_shape, axis2is_selected));
  StrideVector broadcast_strides{};
  GetStrideVector(*broadcast_shape, &broadcast_strides);
  const auto& origin_offsets = std::make_shared<std::vector<int64_t>>(broadcast_shape->elem_cnt());
  for (int64_t i = 0; i < broadcast_shape->elem_cnt(); ++i) {
    IndexVector broadcast_indexes{};
    JUST(GetIndexesFromOffset(broadcast_strides, i, &broadcast_indexes));
    IndexVector origin_indexes{};
    SelectedIndex2OriginIndex(broadcast_indexes, &origin_indexes);
    int64_t origin_offset = -1;
    JUST(GetOffsetFromIndexes(hierarchy_strides, origin_indexes, &origin_offset));
    origin_offsets->at(i) = origin_offset;
  }
  return origin_offsets;
}

Maybe<Symbol<ParallelDesc>> GetBroadcastSubParallelDesc(
    Symbol<ParallelDesc> parallel_desc, Symbol<cfg::ParallelDistribution> parallel_distribution) {
  Optional<int64_t> opt_parallel_id;
  JUST(GetDevice4CurrentProcessCtx(parallel_desc, &opt_parallel_id));
  int64_t parallel_id = JUST(opt_parallel_id.value());
  const auto& axis2is_selected = JUST(GetAxis2IsBroadcast(parallel_distribution));
  return GetSelectedSubParallelDesc(parallel_desc, axis2is_selected, parallel_id);
}

namespace {

Maybe<Symbol<cfg::ParallelDistribution>> MakeNdSbp(const cfg::SbpParallel& sbp) {
  cfg::ParallelDistribution nd_sbp;
  nd_sbp.mutable_sbp_parallel()->Add()->CopyFrom(sbp);
  return SymbolOf(nd_sbp);
}

Maybe<void> InitShapeAxis2NdSbpIndexes(
    Symbol<cfg::ParallelDistribution> nd_sbp,
    std::vector<std::vector<int64_t>>* shape_axis2nd_sbp_indexes) {
  for (int i = 0; i < nd_sbp->sbp_parallel_size(); ++i) {
    const auto& sbp = nd_sbp->sbp_parallel(i);
    if (sbp.has_split_parallel()) {
      int64_t axis = sbp.split_parallel().axis();
      CHECK_GE_OR_RETURN(axis, 0);
      CHECK_LT_OR_RETURN(axis, shape_axis2nd_sbp_indexes->size());
      shape_axis2nd_sbp_indexes->at(axis).push_back(i);
    }
  }
  return Maybe<void>::Ok();
}

int64_t Gcd(int64_t m, int64_t n) {
  if (n == 0) { return m; }
  CHECK_GT(m, 0);
  CHECK_GT(n, 0);
  return Gcd(n, m % n);
}

int64_t Lcm(int64_t m, int64_t n) { return m * n / Gcd(m, n); }

Maybe<void> InitShapAxis2ExpandedDim(
    std::vector<DimVector>* shape_axis2expanded_dims, const Shape& shape, const Shape& hierarchy,
    const std::vector<std::vector<int64_t>>& shape_axis2src_nd_sbp_indexes,
    const std::vector<std::vector<int64_t>>& shape_axis2dst_nd_sbp_indexes) {
  std::vector<DimVector> shape_axis2required_dim(shape.NumAxes());
  for (int i = 0; i < shape.NumAxes(); ++i) {
    const auto& src_nd_sbp_indexes = shape_axis2src_nd_sbp_indexes.at(i);
    const auto& dst_nd_sbp_indexes = shape_axis2dst_nd_sbp_indexes.at(i);
    int64_t max_used_cnt = std::max<size_t>(src_nd_sbp_indexes.size(), dst_nd_sbp_indexes.size());
    for (int j = 0; j < max_used_cnt; ++j) {
      if (j < src_nd_sbp_indexes.size() && j < dst_nd_sbp_indexes.size()) {
        int64_t m = hierarchy.At(src_nd_sbp_indexes.at(j));
        int64_t n = hierarchy.At(dst_nd_sbp_indexes.at(j));
        shape_axis2required_dim.at(i).push_back(Lcm(m, n));
      } else if (j < src_nd_sbp_indexes.size()) {
        shape_axis2required_dim.at(i).push_back(hierarchy.At(src_nd_sbp_indexes.at(j)));
      } else if (j < dst_nd_sbp_indexes.size()) {
        shape_axis2required_dim.at(i).push_back(hierarchy.At(dst_nd_sbp_indexes.at(j)));
      } else {
        UNIMPLEMENTED_THEN_RETURN();
      }
    }
  }
  for (int i = 0; i < shape.NumAxes(); ++i) {
    int64_t total_dim = shape.At(i);
    shape_axis2expanded_dims->at(i).clear();
    if (shape_axis2required_dim.at(i).empty()) {
      shape_axis2expanded_dims->at(i).push_back(total_dim);
    } else {
      Shape inner_shape(shape_axis2required_dim.at(i));
      CHECK_EQ_OR_RETURN(total_dim % inner_shape.elem_cnt(), 0)
          << "dim " << total_dim << "(axis " << i << " in shape " << shape.ToString() << ")"
          << " cannot be reshape into exapanded shape " << inner_shape.ToString();
      auto* dim_vec = &shape_axis2expanded_dims->at(i);
      *dim_vec = shape_axis2required_dim.at(i);
      dim_vec->at(dim_vec->size() - 1) *= total_dim / inner_shape.elem_cnt();
    }
  }
  return Maybe<void>::Ok();
}

Maybe<const Shape> Flatten(const std::vector<DimVector>& shape_axis2expanded_dims) {
  DimVector dim_vec;
  for (const auto& expanded_dims : shape_axis2expanded_dims) {
    CHECK_OR_RETURN(!expanded_dims.empty());
    dim_vec.insert(dim_vec.end(), expanded_dims.begin(), expanded_dims.end());
  }
  return std::make_shared<const Shape>(dim_vec);
}

Maybe<void> InitOldAxis2NewAxisOffset(std::vector<int64_t>* old_axis2new_axis_offset,
                                      const std::vector<DimVector>& shape_axis2expanded_dims) {
  for (int i = 0, offset = 0; i < shape_axis2expanded_dims.size(); ++i) {
    old_axis2new_axis_offset->at(i) = offset;
    offset += shape_axis2expanded_dims.at(i).size();
  }
  return Maybe<void>::Ok();
}

Maybe<Symbol<cfg::ParallelDistribution>> ShiftSplitAxis(
    Symbol<cfg::ParallelDistribution> nd_sbp,
    const std::vector<std::vector<int64_t>>& shape_axis2nd_sbp_indexes,
    const std::vector<int64_t>& old_axis2new_axis_offset) {
  CHECK_EQ_OR_RETURN(shape_axis2nd_sbp_indexes.size(), old_axis2new_axis_offset.size());
  cfg::ParallelDistribution new_nd_sbp(*nd_sbp);
  for (int axis = 0; axis < shape_axis2nd_sbp_indexes.size(); ++axis) {
    int64_t offset = old_axis2new_axis_offset.at(axis);
    for (int64_t j = 0; j < shape_axis2nd_sbp_indexes.at(axis).size(); ++j) {
      int64_t nd_sbp_index = shape_axis2nd_sbp_indexes.at(axis).at(j);
      CHECK_GE_OR_RETURN(nd_sbp_index, 0);
      CHECK_LT_OR_RETURN(nd_sbp_index, new_nd_sbp.sbp_parallel_size());
      auto* sbp_parallel = new_nd_sbp.mutable_sbp_parallel(nd_sbp_index);
      CHECK_OR_RETURN(sbp_parallel->has_split_parallel());
      CHECK_EQ_OR_RETURN(sbp_parallel->split_parallel().axis(), axis);
      sbp_parallel->mutable_split_parallel()->set_axis(offset + j);
    }
  }
  return SymbolOf(new_nd_sbp);
}

Maybe<std::tuple<std::shared_ptr<const Shape>, Symbol<cfg::ParallelDistribution>,
                 Symbol<cfg::ParallelDistribution>>>
CalcDecomposableEquivalentShapeAndNdSbpPair(const Shape& shape, const Shape& hierarchy,
                                            Symbol<cfg::ParallelDistribution> src_nd_sbp,
                                            Symbol<cfg::ParallelDistribution> dst_nd_sbp) {
  CHECK_EQ_OR_RETURN(src_nd_sbp->sbp_parallel_size(), dst_nd_sbp->sbp_parallel_size());
  std::vector<std::vector<int64_t>> shape_axis2src_nd_sbp_indexes(shape.NumAxes());
  JUST(InitShapeAxis2NdSbpIndexes(src_nd_sbp, &shape_axis2src_nd_sbp_indexes));
  std::vector<std::vector<int64_t>> shape_axis2dst_nd_sbp_indexes(shape.NumAxes());
  JUST(InitShapeAxis2NdSbpIndexes(dst_nd_sbp, &shape_axis2dst_nd_sbp_indexes));
  std::vector<DimVector> shape_axis2expanded_dims(shape.NumAxes());
  CHECK_EQ_OR_RETURN(hierarchy.NumAxes(), src_nd_sbp->sbp_parallel_size());
  JUST(InitShapAxis2ExpandedDim(&shape_axis2expanded_dims, shape, hierarchy,
                                shape_axis2src_nd_sbp_indexes, shape_axis2dst_nd_sbp_indexes));
  std::shared_ptr<const Shape> new_shape = JUST(Flatten(shape_axis2expanded_dims));
  CHECK_EQ_OR_RETURN(new_shape->elem_cnt(), shape.elem_cnt());
  std::vector<int64_t> old_axis2new_axis_offset(shape.NumAxes());
  JUST(InitOldAxis2NewAxisOffset(&old_axis2new_axis_offset, shape_axis2expanded_dims));
  Symbol<cfg::ParallelDistribution> new_src_nd_sbp =
      JUST(ShiftSplitAxis(src_nd_sbp, shape_axis2src_nd_sbp_indexes, old_axis2new_axis_offset));
  Symbol<cfg::ParallelDistribution> new_dst_nd_sbp =
      JUST(ShiftSplitAxis(dst_nd_sbp, shape_axis2dst_nd_sbp_indexes, old_axis2new_axis_offset));
  return std::make_tuple(new_shape, new_src_nd_sbp, new_dst_nd_sbp);
}

// nd_sbp is called decomposable if no particular axis is used to split tensor more than once.
// e.g.
// 1) (S0, S1) is decomposable.
// 2) (S0, S0) is not decomposable.
// 3) (S1, S1) is not decomposable.
// although `nd_sbp (S0, S0) on shape (4, 4)` is not decomposable, they could be transformed into a
// decomposable form: `n_sbp (S0, S1) on shape (2, 2, 4)`.
Maybe<std::pair<Symbol<one::ConsistentTensorMeta>, Symbol<cfg::ParallelDistribution>>>
CalcDecomposableEquivalent(Symbol<one::ConsistentTensorMeta> tensor_meta,
                           Symbol<cfg::ParallelDistribution> dst_nd_sbp) {
  std::shared_ptr<const Shape> shape = tensor_meta->shape_ptr();
  Symbol<cfg::ParallelDistribution> src_nd_sbp = tensor_meta->parallel_distribution();
  const auto& hierarchy = tensor_meta->parallel_desc()->hierarchy();
  std::tie(shape, src_nd_sbp, dst_nd_sbp) = *JUST(
      CalcDecomposableEquivalentShapeAndNdSbpPair(*shape, *hierarchy, src_nd_sbp, dst_nd_sbp));

  one::ConsistentTensorMeta decomposible_tensor_meta(shape, tensor_meta->dtype(), src_nd_sbp,
                                                     tensor_meta->parallel_desc());
  return std::make_pair(SymbolOf(decomposible_tensor_meta), dst_nd_sbp);
}

static constexpr auto* GetDecomposableEquivalent =
    DECORATE(&CalcDecomposableEquivalent, ThreadLocal);

}  // namespace

Maybe<std::vector<NaiveBoxingTransformation>> DecomposeByParallelId(
    Symbol<one::ConsistentTensorMeta> tensor_meta, Symbol<cfg::ParallelDistribution> dst_nd_sbp,
    int64_t parallel_id) {
  std::tie(tensor_meta, dst_nd_sbp) = *JUST(GetDecomposableEquivalent(tensor_meta, dst_nd_sbp));
  const auto& parallel_desc = tensor_meta->parallel_desc();
  const auto& src_nd_sbp = tensor_meta->parallel_distribution();
  CHECK_EQ_OR_RETURN(src_nd_sbp->sbp_parallel_size(), dst_nd_sbp->sbp_parallel_size());
  const auto& transformations = std::make_shared<std::vector<NaiveBoxingTransformation>>();
  for (int i = 0; i < src_nd_sbp->sbp_parallel_size(); ++i) {
    const auto& src_sbp = src_nd_sbp->sbp_parallel(i);
    const auto& dst_sbp = dst_nd_sbp->sbp_parallel(i);
    if (src_sbp == dst_sbp) { continue; }
    std::vector<int> axis2selected(src_nd_sbp->sbp_parallel_size());
    axis2selected[i] = 1;
    const auto& sub_parallel_desc =
        JUST(GetSelectedSubParallelDesc(parallel_desc, SymbolOf(axis2selected), parallel_id));
    transformations->push_back(NaiveBoxingTransformation{
        .parallel_desc = sub_parallel_desc,
        .src_nd_sbp = JUST(MakeNdSbp(src_sbp)),
        .dst_nd_sbp = JUST(MakeNdSbp(dst_sbp)),
    });
  }
  return transformations;
}

}  // namespace private_details

static auto* DecomposeByParallelId = DECORATE(&private_details::DecomposeByParallelId, ThreadLocal);

Maybe<std::vector<NaiveBoxingTransformation>> DecomposeIntoNaiveTransformations(
    Symbol<one::ConsistentTensorMeta> tensor_meta, Symbol<cfg::ParallelDistribution> dst_nd_sbp) {
  Optional<int64_t> opt_parallel_id;
  JUST(GetDevice4CurrentProcessCtx(tensor_meta->parallel_desc(), &opt_parallel_id));
  int64_t parallel_id = JUST(opt_parallel_id.value());
  return DecomposeByParallelId(tensor_meta, dst_nd_sbp, parallel_id);
}

namespace {

Maybe<std::unordered_map<int64_t, Symbol<ParallelDesc>>> CalcBroadcastGroup(
    Symbol<ParallelDesc> src_parallel_desc, Symbol<ParallelDesc> dst_parallel_desc,
    bool allow_across_node) {
  CHECK_EQ_OR_RETURN(src_parallel_desc->parallel_num(),
                     src_parallel_desc->sorted_machine_ids().size());
  CHECK_EQ_OR_RETURN(dst_parallel_desc->parallel_num(),
                     dst_parallel_desc->sorted_machine_ids().size());
  CHECK_EQ_OR_RETURN(src_parallel_desc->device_type(), dst_parallel_desc->device_type());
  CHECK_LE_OR_RETURN(src_parallel_desc->parallel_num(), dst_parallel_desc->parallel_num());
  const auto& src_process_ids = src_parallel_desc->sorted_machine_ids();
  HashMap<int64_t, std::vector<int64_t>> process_id2group{};
  HashMap<int64_t, std::vector<int64_t>> node_id2src_process_id{};
  for (int64_t process_id : src_process_ids) {
    std::vector<int64_t> vec{process_id};
    CHECK_OR_RETURN(process_id2group.emplace(process_id, vec).second);
    CHECK_OR_RETURN(dst_parallel_desc->ContainingMachineId(process_id));
    node_id2src_process_id[GlobalProcessCtx::NodeId(process_id)].push_back(process_id);
  }
  std::vector<int64_t> remainder_process_ids{};
  remainder_process_ids.reserve(dst_parallel_desc->sorted_machine_ids().size());
  HashMap<int64_t, int64_t> node_id2counter{};
  for (int64_t process_id : dst_parallel_desc->sorted_machine_ids()) {
    if (!src_parallel_desc->ContainingMachineId(process_id)) {
      const auto& node_iter = node_id2src_process_id.find(GlobalProcessCtx::NodeId(process_id));
      if (node_iter == node_id2src_process_id.end()) {
        CHECK_OR_RETURN(allow_across_node)
            << Error::Unimplemented() << "\n----[src_placement]----\n"
            << src_parallel_desc->parallel_conf().DebugString() << "\n----[dst_placement]----\n"
            << dst_parallel_desc->parallel_conf().DebugString();
        // handle `process_id` later.
        remainder_process_ids.push_back(process_id);
      } else {
        // balancedly put `process_id` into the groups within the same node..
        int64_t node_id = node_iter->first;
        const auto& src_process_ids = node_iter->second;
        int64_t src_process_index = (node_id2counter[node_id]++) % src_process_ids.size();
        int64_t src_process_id = src_process_ids.at(src_process_index);
        JUST(MutMapAt(&process_id2group, src_process_id))->push_back(process_id);
      }
    }
  }
  // put remainder process ids into src groups.
  for (int i = 0; i < remainder_process_ids.size(); ++i) {
    int64_t src_process_id = src_process_ids.at(i % src_process_ids.size());
    JUST(MutMapAt(&process_id2group, src_process_id))->push_back(remainder_process_ids.at(i));
  }
  const auto& map = std::make_shared<std::unordered_map<int64_t, Symbol<ParallelDesc>>>();
  for (const auto& pair : process_id2group) {
    const auto& group = pair.second;
    ParallelConf parallel_conf;
    parallel_conf.set_device_tag(dst_parallel_desc->parallel_conf().device_tag());
    for (int64_t process_id : group) {
      const auto& device_ids = dst_parallel_desc->sorted_dev_phy_ids(process_id);
      CHECK_EQ_OR_RETURN(device_ids.size(), 1);
      parallel_conf.add_device_name(std::string("@") + std::to_string(process_id) + ":"
                                    + std::to_string(device_ids.at(0)));
    }
    const auto& parallel_desc = SymbolOf(ParallelDesc(parallel_conf));
    for (int64_t process_id : group) {
      CHECK_OR_RETURN(map->emplace(process_id, parallel_desc).second);
    }
  }
  return map;
}
auto* CachedBroadcastGroup = DECORATE(&CalcBroadcastGroup, ThreadLocal);

}  // namespace

Maybe<std::unordered_map<int64_t, Symbol<ParallelDesc>>> GetBroadcastGroup(
    Symbol<ParallelDesc> src_parallel_desc, Symbol<ParallelDesc> dst_parallel_desc) {
  return CachedBroadcastGroup(src_parallel_desc, dst_parallel_desc, true);
}

Maybe<std::unordered_map<int64_t, Symbol<ParallelDesc>>> GetBroadcastGroupWithoutAcrossNode(
    Symbol<ParallelDesc> src_parallel_desc, Symbol<ParallelDesc> dst_parallel_desc) {
  return CachedBroadcastGroup(src_parallel_desc, dst_parallel_desc, false);
}

}  // namespace oneflow
