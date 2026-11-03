#ifndef TASKMASTER_DIFF_DIFF_BUILDER_HPP
#define TASKMASTER_DIFF_DIFF_BUILDER_HPP

#include "taskmaster/diff/Diff.hpp"
#include "taskmaster/io/ConfigParser.hpp"

namespace taskmaster {

/* Compares two parsed JobMaps. DiffExecutor only applies the decisions made
 * here; it never needs to inspect the old configuration. */
class DiffBuilder {
public:
    Diff build(const JobMap &oldJobs, const JobMap &newJobs) const;
};

} // namespace taskmaster

#endif
