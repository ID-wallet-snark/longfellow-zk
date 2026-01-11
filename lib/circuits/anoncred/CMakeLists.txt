# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# small_test
add_executable(small_test small_test.cc)
target_link_libraries(small_test
  PRIVATE
  mdoc
  algebra
  ec
  flatsha
  util
  gtest_main
  gtest
  benchmark_main
  benchmark
)

# ptrcred_age_over_18_test
add_executable(ptrcred_age_over_18_test ptrcred_age_over_18_test.cc)
target_link_libraries(ptrcred_age_over_18_test
  PRIVATE
  mdoc
  algebra
  ec
  flatsha
  util
  gtest_main
  gtest
  benchmark_main
  benchmark
)

# ptrcred_eu_residency_test
add_executable(ptrcred_eu_residency_test ptrcred_eu_residency_test.cc)
target_link_libraries(ptrcred_eu_residency_test
  PRIVATE
  mdoc
  algebra
  ec
  flatsha
  util
  gtest_main
  gtest
  benchmark_main
  benchmark
)


# ptrcred_age_over_18_benchmark
add_executable(ptrcred_age_over_18_benchmark ptrcred_age_over_18_benchmark.cc)
target_link_libraries(ptrcred_age_over_18_benchmark
  PRIVATE
  mdoc
  algebra
  ec
  flatsha
  util
  gtest
  benchmark
)
