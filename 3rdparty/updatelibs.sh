#!/bin/sh

KYOTO=kyotocabinet-1.2.75

test -d leveldb/src || git clone https://code.google.com/p/leveldb/ leveldb/src
cd leveldb/src && git pull && cd ../..
if [ ! -d kyoto/$KYOTO ]; then
   mkdir -p kyoto/$KYOTO
   cd kyoto
   wget http://fallabs.com/kyotocabinet/pkg/$KYOTO.tar.gz
   tar xzfv $KYOTO.tar.gz
   rm -f src
   ln -s $KYOTO src
   cd src
   ./configure
fi
