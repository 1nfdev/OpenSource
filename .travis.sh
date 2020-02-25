travis_before_install() {
  git submodule update --init --recursive
}

travis_install() {
  if [ "$CXX" = "g++" ]; then
    sudo apt-get install -qq g++-4.8
  fi
  
  if [ "$PVS_ANALYZE" = "Yes" ]; then
    wget -q -O - https://files.viva64.com/etc/pubkey.txt \
      | sudo apt-key add -
    sudo wget -O /etc/apt/sources.list.d/viva64.list \
      https://files.viva64.com/etc/viva64.list  
    
    sudo apt-get update -qq
    sudo apt-get install -qq pvs-studio \
                             libio-socket-ssl-perl \
                             libnet-ssleay-perl
  fi
    
  download_extract \
    "https://cmake.org/files/v3.6/cmake-3.6.2-Linux-x86_64.tar.gz" \
    cmake-3.6.2-Linux-x86_64.tar.gz
}

download_extract() {
  aria2c -x 16 $1 -o $2
  tar -xf $2
}

travis_script() {
  if [ -d cmake-3.6.2-Linux-x86_64 ]; then
    export PATH=$(pwd)/cmake-3.6.2-Linux-x86_64/bin:$PATH
  fi
  
  CMAKE_ARGS="-DHEADLESS=ON ${CMAKE_ARGS}"
  if [ "$PVS_ANALYZE" = "Yes" ]; then
    CMAKE_ARGS="-DCMAKE_EXPORT_COMPILE_COMMANDS=On ${CMAKE_ARGS}"
  fi
  cmake $CMAKE_ARGS CMakeLists.txt
  make
}

travis_after_success() {
  if [ "$PVS_ANALYZE" = "Yes" ]; then
    pvs-studio-analyzer credentials $PVS_USERNAME $PVS_KEY -o PVS-Studio.lic
    pvs-studio-analyzer analyze -j2 -l PVS-Studio.lic \
                                    -o PVS-Studio-${CC}.log \
                                    --disableLicenseExpirationCheck
    
    plog-converter -t html PVS-Studio-${CC}.log -o PVS-Studio-${CC}.html
    sendemail -t mail@domain.com \
              -u "PVS-Studio $CC report, commit:$TRAVIS_COMMIT" \
              -m "PVS-Studio $CC report, commit:$TRAVIS_COMMIT" \
              -s smtp.gmail.com:587 \
              -xu $MAIL_USER \
              -xp $MAIL_PASSWORD \
              -o tls=yes \
              -f $MAIL_USER \
              -a PVS-Studio-${CC}.log PVS-Studio-${CC}.html
  fi
}