######################################################################
# Automatically generated by qmake (2.01a) Mon Feb 6 20:18:57 2012
######################################################################

TEMPLATE = app
TARGET =
DEPENDPATH += .
INCLUDEPATH += . ../3rdparty/leveldb/src/include
include(../shared/shared.pri)
include(../shared/clang.pri)

# Input
SOURCES += \
    main.cpp \
    Compressor.cpp \
    Indexer.cpp \
    Database.cpp \
    UnitCache.cpp \
    Resource.cpp \
    Resources.cpp \
    Server.cpp \
    SHA256.cpp \
    DumpJob.cpp \
    FollowLocationJob.cpp \
    MatchJob.cpp \
    RecompileJob.cpp \
    ReferencesJob.cpp \
    Rdm.cpp \
    StatusJob.cpp \
    PokeJob.cpp 

HEADERS += \
    Compressor.h \
    Indexer.h \
    Database.h \
    UnitCache.h \
    Resource.h \
    Resources.h \
    Server.h \
    SHA256.h \
    DumpJob.h \
    FollowLocationJob.h \
    MatchJob.h \
    RecompileJob.h \
    ReferencesJob.h \
    Rdm.h \
    StatusJob.h \
    PokeJob.h \
    Location.h \
    Source.h \
    LevelDB.h
