#include "net_types.hpp"

const class_description* class_description::UNKNOWN = new base_class(0, L"{UnknownClass}", nullptr, UNKNOWN_SIZE);
const function_spec* function_spec::UNKNOWN = new function_spec(0, L"{UnknownMethod}", class_description::UNKNOWN, class_description::UNKNOWN, {}, false);
