#ifndef CompileMessage_h
#define CompileMessage_h

#include "List.h"
#include "ByteArray.h"
#include "ClientMessage.h"
#include "RTags.h"

class CompileMessage : public ClientMessage
{
public:
    enum { MessageId = ProjectId };

    CompileMessage(const Path &path = Path(), const ByteArray &args = ByteArray());

    virtual int messageId() const { return MessageId; }

    Path path() const { return mPath; }

    ByteArray arguments() const { return mArgs; }
    void setArguments(const ByteArray &arguments) { mArgs = arguments; }

    ByteArray encode() const;
    void fromData(const char *data, int size);
private:
    Path mPath;
    ByteArray mArgs;
};

#endif
