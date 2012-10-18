#ifndef GRTags_h
#define GRTags_h

#include "Project.h"
#include "FileSystemWatcher.h"

class GRParseJob;
class GRTags
{
public:
    GRTags();
    void init(const SharedPtr<Project> &project);
    void onFileAdded(const Path &path);
    void onFileRemoved(const Path &path);
    void onRecurseJobFinished(const Set<Path> &files);
    void recurse();
    void add(const Path &source);
    void onParseJobFinished(const SharedPtr<GRParseJob> &job, const GRMap &data);
    void dirty(uint32_t fileId, GRMap &map);
    bool isIndexed(uint32_t fileId) const;
private:
    WeakPtr<Project> mProject;
    FileSystemWatcher mWatcher;
    int mActive, mCount;
};

#endif
