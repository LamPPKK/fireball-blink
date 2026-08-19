#ifndef FIREBALL_COMPONENTS_ADBLOCK_NETWORK_EVALUATOR_H_
#define FIREBALL_COMPONENTS_ADBLOCK_NETWORK_EVALUATOR_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "fireball/components/adblock/include/fireball_adblock_ffi.h"

namespace fireball::adblock {

enum class EvaluationStatus {
  kOk,
  kInvalidInput,
  kInternalError,
};

struct NetworkEvaluation {
  EvaluationStatus status = EvaluationStatus::kInternalError;
  std::uint32_t flags = 0;
  std::optional<std::string> redirect;
  std::optional<std::string> rewritten_url;
};

struct NetworkRequest {
  std::string_view url;
  std::string_view hostname;
  std::string_view source_hostname;
  std::string_view request_type;
  std::string_view method;
  bool third_party = false;
};

class NetworkEvaluator {
 public:
  virtual ~NetworkEvaluator() = default;
  virtual NetworkEvaluation Evaluate(const NetworkRequest& request) = 0;
};

// Borrowed adapter around one single-sequence adblock-rust engine. The owner
// must keep the engine alive and call this object only from its bound sequence.
class FfiNetworkEvaluator final : public NetworkEvaluator {
 public:
  explicit FfiNetworkEvaluator(const FireballAdblockEngine* engine);

  NetworkEvaluation Evaluate(const NetworkRequest& request) override;

 private:
  const FireballAdblockEngine* engine_;
};

}  // namespace fireball::adblock

#endif  // FIREBALL_COMPONENTS_ADBLOCK_NETWORK_EVALUATOR_H_
