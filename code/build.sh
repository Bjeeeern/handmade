#!/bin/bash

mkdir -p ../build/osx

clang -framework Cocoa -framework Foundation osx_game.mm -o ../build/osx/osx_game
