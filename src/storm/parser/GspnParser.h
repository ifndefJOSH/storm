#include "storm/storage/gspn/GSPN.h"

namespace storm {
    namespace parser {
        class GspnParser {
        public:
            static storm::gspn::GSPN* parse(std::string const& filename);
        };
    }
}
