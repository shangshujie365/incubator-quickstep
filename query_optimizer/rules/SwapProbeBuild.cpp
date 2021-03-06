/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 **/

#include "query_optimizer/rules/SwapProbeBuild.hpp"

#include <cstddef>
#include <memory>
#include <vector>

#include "query_optimizer/expressions/AttributeReference.hpp"
#include "query_optimizer/physical/HashJoin.hpp"
#include "query_optimizer/physical/PatternMatcher.hpp"
#include "query_optimizer/physical/Physical.hpp"
#include "query_optimizer/physical/TopLevelPlan.hpp"
#include "query_optimizer/rules/Rule.hpp"


namespace quickstep {
namespace optimizer {

namespace C = cost;
namespace E = expressions;
namespace P = physical;

P::PhysicalPtr SwapProbeBuild::applyToNode(const P::PhysicalPtr &input) {
  P::HashJoinPtr hash_join;

  if (P::SomeHashJoin::MatchesWithConditionalCast(input, &hash_join)
      && hash_join->join_type() == P::HashJoin::JoinType::kInnerJoin) {
    P::PhysicalPtr left = hash_join->left();
    P::PhysicalPtr right = hash_join->right();

    std::size_t left_cardinality = cost_model_->estimateCardinality(left);
    std::size_t right_cardinality = cost_model_->estimateCardinality(right);

    if (right_cardinality > left_cardinality) {
      std::vector<E::AttributeReferencePtr> left_join_attributes = hash_join->left_join_attributes();
      std::vector<E::AttributeReferencePtr> right_join_attributes = hash_join->right_join_attributes();

      P::PhysicalPtr output = P::HashJoin::Create(right,
                                                  left,
                                                  right_join_attributes,
                                                  left_join_attributes,
                                                  hash_join->residual_predicate(),
                                                  hash_join->build_predicate(),
                                                  hash_join->project_expressions(),
                                                  hash_join->join_type());
      LOG_APPLYING_RULE(input, output);
      return output;
    }
  }

  LOG_IGNORING_RULE(input);
  return input;
}

void SwapProbeBuild::init(const P::PhysicalPtr &input) {
  if (cost_model_ == nullptr) {
    P::TopLevelPlanPtr top_level;
    if (P::SomeTopLevelPlan::MatchesWithConditionalCast(input, &top_level)) {
      cost_model_.reset(new C::SimpleCostModel(top_level->shared_subplans()));
    } else {
      std::vector<P::PhysicalPtr> plans = {input};
      cost_model_.reset(new C::SimpleCostModel(plans));
    }
  }
}

}  // namespace optimizer
}  // namespace quickstep
