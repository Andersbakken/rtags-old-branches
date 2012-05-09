TEMPLATE = app

libleveldb.commands = \$(MAKE) -C src
libleveldb.target = src/libkyotocabinet.a

QMAKE_EXTRA_TARGETS += libkyotocabinet
PRE_TARGETDEPS += src/libkyotocabinet.a
QMAKE_POST_LINK = cp src/libkyotocabinet.a ..

# nasty hack, not sure how else to prevent the link from being attempted
QMAKE_LINK = echo
