#include "ValidateDBJob.h"
#include "CursorInfo.h"
#include "Indexer.h"
#include "RTags.h"
#include "Server.h"
#include <clang-c/Index.h>

ValidateDBJob::ValidateDBJob(const std::shared_ptr<Project> &proj, const Set<ByteArray> &prev)
    : Job(0, proj), mPrevious(prev)
{
}

void ValidateDBJob::execute()
{
    int errors = 0;
    int total = 0;
    Set<ByteArray> newErrors;

    Scope<const SymbolMap&> scope = project()->lockSymbolsForRead();
    if (scope.isNull())
        return;
    const SymbolMap &map = scope.data();
    for (SymbolMap::const_iterator it = map.begin(); it != map.end(); ++it) {
        if (isAborted()) {
            return;
        }
        const CursorInfo &ci = it->second;
        if (!ci.symbolLength) {
            const ByteArray &usr = it->first;
            if (!mPrevious.contains(usr))
                error() << "Invalid entry for " << it->second;
            newErrors.insert(usr);
            ++errors;
        }
    }
    mErrors(newErrors);
    error("Checked %d CursorInfo objects, %d errors", total, errors);
}
