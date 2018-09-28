/*
 *  Copyright (c) 2014-present, Facebook, Inc.
 *
 *  This source code is licensed under the MIT license found in the LICENSE
 *  file in the root directory of this source tree.
 *
 */
#pragma once

#include <unordered_map>

#include <folly/Range.h>

namespace folly {
struct dynamic;
} // namespace folly

namespace facebook {
namespace memcache {

constexpr size_t kNumTries = 32;

/**
 * A weighted CH3 hash function.
 *
 * Each server is assigned a weight between 0.0 and 1.0 inclusive.
 * The algorithm:
 *
 *   Try retryCount times:
 *     index = CH3(key + next_salt(), n)
 *     probability = SpookyHashV2_uint32(key)
 *     if (probability < server_weight[index] * uint32_max):
 *       return index
 *   return index
 *
 * Where next_salt() initially returns an empty string, and subsequently
 * distinct salt strings.
 * (the actual salts used are strings "0", "1", ..., "9", "01", "11", "21",
 *  i.e. reversed decimal representations of an increasing counter).
 *
 * Note that if all weights are 1.0, the algorithm returns the same indices
 * as simply CH3(key, n).
 *
 * The algorithm is consistent both with respect to individual weights and
 * mostly consistent wrt n. i.e. reducing any single weight slightly will
 * only spread out a small fraction of the load from that server to all other
 * servers, but changing the number of servers may involve some spillover.
 * Consistency is a function of how far the weights are from 1, with all weights
 * at 1 being perfectly consistent
 *
 * NOTE: The algorithm gives up after given no. of retries and returns the index
 * of the last retry. If the weights are too skewed or if there are too many
 * zeros in the vector then the algorithm can fail.

 * The probability of the algorithm returning in single attempt is equal to the
 * avg. weight (SUM(weights) / COUNT(weights)). If the avg. weight is not close
 * to 1 then retries are useful and one may need more retries if the avg. weight
 * is too low. For instance is avg. weight is 0.25, then each iteration of the
 * algorithm will fail with probability (1 - 0.25) and it requires upto 16
 * retries to bring the chances of failure below 1%.
 */

std::vector<double> ch3wParseWeights(const folly::dynamic& json, size_t n);

size_t weightedCh3Hash(
    folly::StringPiece key,
    folly::Range<const double*> weights,
    size_t retryCount = kNumTries);

class WeightedCh3HashFunc {
 public:
  /**
   * @param weights  A list of server weights.
   *                 Pool size is taken to be weights.size()
   */
  explicit WeightedCh3HashFunc(std::vector<double> weights);

  /**
   * @param json  Json object of the following format:
   *              {
   *                "weights": [ ... ]
   *              }
   * @param n     Number of servers in the config.
   */
  WeightedCh3HashFunc(const folly::dynamic& json, size_t n);

  size_t operator()(folly::StringPiece key) const;

  /**
   * @return Saved weights.
   */
  const std::vector<double>& weights() const {
    return weights_;
  }

  static const char* type() {
    return "WeightedCh3";
  }

 private:
  const std::vector<double> weights_;
};

} // namespace memcache
} // namespace facebook
