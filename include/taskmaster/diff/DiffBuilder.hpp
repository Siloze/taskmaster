#pragma once

#include "taskmaster/diff/Diff.hpp"
#include "taskmaster/io/ConfigParser.hpp"

namespace taskmaster {

    class DiffBuilder {
        public:
            Diff build(const JobMap &oldJobs, const JobMap &newJobs) const;
    };

}
