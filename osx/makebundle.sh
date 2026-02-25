#!/bin/sh

rm -rf ZoiteChat.app
rm -f *.app.zip

python $HOME/.local/bin/gtk-mac-bundler zoitechat.bundle

echo "Compressing bundle"
zip -9rXq ./ZoiteChat-$(git describe --tags).app.zip ./ZoiteChat.app
