#include "fireball/components/adblock/network_evaluator.h"

#include <cstring>
#include <utility>

namespace fireball::adblock {
namespace {

constexpr std::size_t kMaximumRedirectBytes = 256 * 1024;
constexpr std::size_t kMaximumRewriteBytes = 8192;

struct ConsumedString {
  bool valid = true;
  std::optional<std::string> value;
};

ConsumedString ConsumeFfiString(char* value, std::size_t maximum_bytes) {
  if (value == nullptr) {
    return {};
  }
  const std::size_t length = strnlen(value, maximum_bytes + 1);
  ConsumedString result;
  if (length > maximum_bytes) {
    result.valid = false;
  } else {
    result.value = std::string(value, length);
  }
  fireball_adblock_string_destroy(value);
  return result;
}

const std::uint8_t* Bytes(std::string_view value) {
  return reinterpret_cast<const std::uint8_t*>(value.data());
}

}  // namespace

FfiNetworkEvaluator::FfiNetworkEvaluator(const FireballAdblockEngine* engine)
    : engine_(engine) {}

NetworkEvaluation FfiNetworkEvaluator::Evaluate(
    const NetworkRequest& request) {
  if (engine_ == nullptr) {
    return {};
  }
  FireballAdblockDecision decision = fireball_adblock_check_network(
      engine_, Bytes(request.url), request.url.size(), Bytes(request.hostname),
      request.hostname.size(), Bytes(request.source_hostname),
      request.source_hostname.size(), Bytes(request.request_type),
      request.request_type.size(), Bytes(request.method), request.method.size(),
      request.third_party);
  ConsumedString redirect =
      ConsumeFfiString(decision.redirect, kMaximumRedirectBytes);
  ConsumedString rewritten =
      ConsumeFfiString(decision.rewritten_url, kMaximumRewriteBytes);
  if (!redirect.valid || !rewritten.valid) {
    return {};
  }

  EvaluationStatus status = EvaluationStatus::kInternalError;
  if (decision.status == FIREBALL_ADBLOCK_STATUS_OK) {
    status = EvaluationStatus::kOk;
  } else if (decision.status == FIREBALL_ADBLOCK_STATUS_INVALID_INPUT) {
    status = EvaluationStatus::kInvalidInput;
  }
  return {status, decision.flags, std::move(redirect.value),
          std::move(rewritten.value)};
}

}  // namespace fireball::adblock
