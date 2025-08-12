#!/bin/bash
$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)
export PATH=$PATH:/home/linuxbrew/.linuxbrew/bin/
export HOMEBREW_NO_INSTALL_FROM_API=1
directory=$(cat ${CMAKE_CURRENT_SOURCE_DIR}/.git/config | grep origin -A 1 | head -n 2 | tail -n 1 | cut -d ':' -f 2)
repository=$(echo $directory | cut -d '/' -f 1)
project=$(echo $directory | cut -d '/' -f 2)
brew tap --force homebrew/core
brew create --cmake https://github.com/$repository/$project/archive/refs/tags/v@AHP_XC_GUI_VERSION@.tar.gz --set-name "$project@@AHP_XC_GUI_VERSION@" --set-version '@AHP_XC_GUI_VERSION@' --tap homebrew/core
brew audit --new $project@@AHP_XC_GUI_VERSION@
brew install --build-from-source --verbose --debug $project@@AHP_XC_GUI_VERSION@
brew test $project@@AHP_XC_GUI_VERSION@
#Editing /home/linuxbrew/.linuxbrew/Homebrew/Library/Taps/homebrew/homebrew-core/Formula/lib/$project@@AHP_XC_GUI_VERSION@.rb
